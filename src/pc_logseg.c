#include "pc_logseg.h"
#include "pc_crc32c.h"
#include <string.h> // memset, memcpy

// Local helpers
static bool is_aligned(size_t x, size_t a) { return (a == 0) ? (x == 0) : (x % a) == 0; }

pc_result_t pc_logseg_erase(pc_flash_t *f, size_t base)
{
  if (!f)
    return PC_EINVAL;
  const size_t seg = pc_logseg_segment_bytes(f);
  if (!is_aligned(base, seg))
    return PC_EINVAL;
  size_t sector_index = base / seg;
  return pc_flash_erase_sector(f, sector_index);
}

pc_result_t pc_logseg_program_data(pc_flash_t *f, size_t base, size_t offset, const void *data, size_t len)
{
  if (!f || (!data && len > 0))
    return PC_EINVAL;
  const size_t seg = pc_logseg_segment_bytes(f);
  const size_t prog = pc_logseg_commit_page_bytes(f);
  const size_t preH = pc_logseg_preheader_bytes(f);

  if (!is_aligned(base, seg))
    return PC_EINVAL;
  if (offset + len > preH)
    return PC_EINVAL; // must not cross into commit page
  if (!is_aligned(base + offset, prog) || !is_aligned(len, prog))
    return PC_EINVAL;

  return pc_flash_program(f, base + offset, data, len);
}

pc_result_t pc_logseg_crc32c_region(const pc_flash_t *f, size_t base, uint32_t *out_crc)
{
  if (!f || !out_crc)
    return PC_EINVAL;
  const size_t seg = pc_logseg_segment_bytes(f);
  const size_t prog = pc_logseg_commit_page_bytes(f);
  const size_t preH = pc_logseg_preheader_bytes(f);
  if (!is_aligned(base, seg))
    return PC_EINVAL;

  uint32_t crc = PC_CRC32C_SEED;
  // Read in program-sized chunks to avoid large temp buffers.
  uint8_t buf[512]; // ok even if prog is 256; we will read prog each time
  if (prog > sizeof(buf))
  {
    // Extremely large program size not supported in this helper
    return PC_EINVAL;
  }
  for (size_t off = 0; off < preH; off += prog)
  {
    pc_result_t st = pc_flash_read(f, base + off, buf, prog);
    if (st != PC_OK)
      return st;
    crc = pc_crc32c_update(crc, buf, prog);
  }
  *out_crc = PC_CRC32C_FINALIZE(crc);
  return PC_OK;
}

pc_result_t pc_logseg_commit(pc_flash_t *f, size_t base,
                             uint16_t type, uint32_t seqno,
                             uint32_t ts_min, uint32_t ts_max, uint32_t record_count)
{
  if (!f)
    return PC_EINVAL;
  const size_t seg = pc_logseg_segment_bytes(f);
  const size_t prog = pc_logseg_commit_page_bytes(f);
  const size_t preH = pc_logseg_preheader_bytes(f);
  if (!is_aligned(base, seg))
    return PC_EINVAL;

  // Compute CRC across the entire pre-header region as currently on flash.
  uint32_t crc = 0;
  pc_result_t st = pc_logseg_crc32c_region(f, base, &crc);
  if (st != PC_OK)
    return st;

  // Build header.
  pc_segment_hdr_t hdr;
  hdr.magic = PC_SEG_MAGIC;
  hdr.version = PC_SEG_VERSION;
  hdr.type = type;
  hdr.seqno = seqno;
  hdr.ts_min = ts_min;
  hdr.ts_max = ts_max;
  hdr.record_count = record_count;
  hdr.crc32c = crc;

  // Write header into the last program page.
  // We program a full page (prog bytes), header at the start, rest 0xFF.
  uint8_t page[512]; // assume prog <= 512; we checked in crc helper similarly
  if (prog > sizeof(page))
    return PC_EINVAL;
  memset(page, 0xFF, prog);
  memcpy(page, &hdr, sizeof(hdr));

  size_t header_addr = base + preH; // last page start
  // Alignment: header_addr should be aligned to prog by construction.
  if (!is_aligned(header_addr, prog))
    return PC_EINVAL;

  return pc_flash_program(f, header_addr, page, prog);
}

bool pc_logseg_header_erased(const pc_flash_t *f, size_t base)
{
  if (!f)
    return true;
  const size_t seg = pc_logseg_segment_bytes(f);
  const size_t prog = pc_logseg_commit_page_bytes(f);
  const size_t preH = pc_logseg_preheader_bytes(f);
  if (!is_aligned(base, seg))
    return true;

  uint8_t page[512];
  if (prog > sizeof(page))
    return true;
  if (pc_flash_read(f, base + preH, page, prog) != PC_OK)
    return true;
  for (size_t i = 0; i < prog; ++i)
  {
    if (page[i] != 0xFF)
      return false;
  }
  return true;
}

pc_result_t pc_logseg_verify(const pc_flash_t *f, size_t base, pc_segment_hdr_t *out_hdr)
{
  if (!f)
    return PC_EINVAL;
  const size_t seg = pc_logseg_segment_bytes(f);
  const size_t prog = pc_logseg_commit_page_bytes(f);
  const size_t preH = pc_logseg_preheader_bytes(f);
  if (!is_aligned(base, seg))
    return PC_EINVAL;

  uint8_t page[512];
  if (prog > sizeof(page))
    return PC_EINVAL;

  pc_result_t st = pc_flash_read(f, base + preH, page, prog);
  if (st != PC_OK)
    return st;

  // If the page is still fully erased, there's no header -> treat as corrupt/uncommitted.
  bool all_ff = true;
  for (size_t i = 0; i < prog; ++i)
  {
    if (page[i] != 0xFF)
    {
      all_ff = false;
      break;
    }
  }
  if (all_ff)
    return PC_CORRUPT;

  // Unpack header
  pc_segment_hdr_t hdr = {0};
  memcpy(&hdr, page, sizeof(hdr));

  if (hdr.magic != PC_SEG_MAGIC || hdr.version != PC_SEG_VERSION)
  {
    return PC_CORRUPT;
  }

  // Recompute CRC over pre-header region and compare
  uint32_t crc = 0;
  st = pc_logseg_crc32c_region(f, base, &crc);
  if (st != PC_OK)
    return st;

  if (crc != hdr.crc32c)
  {
    return PC_CORRUPT;
  }

  if (out_hdr)
    *out_hdr = hdr;
  return PC_OK;
}
