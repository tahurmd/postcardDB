#include "pc_recover.h"

static bool is_aligned(size_t x, size_t a) { return (a == 0) ? (x == 0) : (x % a) == 0; }

pc_result_t pc_recover_scan_all(const pc_flash_t *f,
                                pc_seg_summary_t *out,
                                size_t max_out,
                                size_t *found)
{
  if (!f)
    return PC_EINVAL;
  if (found)
    *found = 0;

  const size_t seg = pc_flash_sector_bytes(f);
  const size_t total = pc_flash_total(f);
  if (seg == 0 || total == 0)
    return PC_EINVAL;

  size_t write_idx = 0;

  for (size_t base = 0; base + seg <= total; base += seg)
  {
    if (!is_aligned(base, seg))
      continue;

    // Skip bad sectors early (treat as unreadable)
    size_t sector_index = base / seg;
    if (pc_flash_is_bad(f, sector_index))
    {
      continue;
    }

    // If commit page is erased → uncommitted → skip
    if (pc_logseg_header_erased(f, base))
    {
      continue;
    }

    // Verify header + CRC over pre-header region
    pc_segment_hdr_t hdr;
    pc_result_t st = pc_logseg_verify(f, base, &hdr);
    if (st != PC_OK)
    {
      // corrupt or I/O → skip and keep scanning
      continue;
    }

    // Valid segment: emit summary (if space)
    if (out && write_idx < max_out)
    {
      out[write_idx].base = base;
      out[write_idx].type = (uint16_t)hdr.type;
      out[write_idx].seqno = hdr.seqno;
      out[write_idx].ts_min = hdr.ts_min;
      out[write_idx].ts_max = hdr.ts_max;
      out[write_idx].record_count = hdr.record_count;
    }
    write_idx++;
  }

  if (found)
    *found = (out ? (write_idx > max_out ? max_out : write_idx) : 0);
  return PC_OK;
}
