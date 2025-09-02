#include "pc_appender.h"
#include <string.h>

static int is_pow2(size_t x) { return x && ((x & (x - 1)) == 0); }

static pc_result_t flush_page(pc_appender_t *a)
{
  if (a->page_off == 0)
    return PC_OK;
  size_t addr = a->base + (a->seg_off - a->page_off);
  if ((addr % a->prog) != 0)
    return PC_EINVAL;
  pc_result_t rc = pc_flash_program(a->f, addr, a->page, a->prog);
  memset(a->page, 0xFF, a->prog);
  a->page_off = 0;
  return rc;
}

static pc_result_t emit_bytes(pc_appender_t *a, const void *src, size_t len)
{
  const uint8_t *p = (const uint8_t *)src;
  while (len)
  {
    size_t space = a->prog - a->page_off;
    size_t chunk = (len < space) ? len : space;

    memcpy(a->page + a->page_off, p, chunk);
    a->page_off += chunk;
    a->seg_off += chunk;
    p += chunk;
    len -= chunk;

    // never run past pre-header region
    if (a->seg_off > a->preH)
      return PC_NO_SPACE;

    if (a->page_off == a->prog)
    {
      pc_result_t rc = flush_page(a);
      if (rc != PC_OK)
        return rc;
    }
  }
  return PC_OK;
}

pc_result_t pc_appender_open(pc_appender_t *a, pc_flash_t *f, size_t base, uint32_t seqno)
{
  if (!a || !f)
    return PC_EINVAL;

  a->f = f;
  a->base = base;
  a->seg = pc_flash_sector_bytes(f);
  a->prog = pc_flash_prog_bytes(f);

  if (a->seg == 0 || a->prog == 0)
    return PC_EINVAL;
  if (!is_pow2(a->seg) || !is_pow2(a->prog))
    return PC_EINVAL;
  if ((base % a->seg) != 0)
    return PC_EINVAL;
  if (a->prog > sizeof(a->page))
    return PC_EINVAL;

  a->preH = a->seg - a->prog;

  // erase segment
  pc_result_t st = pc_logseg_erase(f, base);
  if (st != PC_OK)
    return st;

  memset(a->page, 0xFF, a->prog);
  a->page_off = 0;
  a->seg_off = 0;
  a->ts_min = 0xFFFFFFFFu;
  a->ts_max = 0u;
  a->record_count = 0u;
  a->seqno = seqno;
  a->open = true;
  return PC_OK;
}

pc_result_t pc_appender_append_block(pc_appender_t *a,
                                     uint16_t metric_id,
                                     uint16_t series_id,
                                     const uint32_t *ts_array,
                                     const float *val_array,
                                     uint32_t npoints)
{
  if (!a || !a->open || !ts_array || !val_array)
    return PC_EINVAL;
  if (npoints == 0)
    return PC_EINVAL;

  // Compute how many bytes the block needs.
  const size_t need = sizeof(pc_block_hdr_t) + (size_t)npoints * sizeof(pc_point_disk_t);

  // Check fit conservatively: we may need to flush the partially filled page at the end,
  // which always programs a full page. Because preH is a multiple of prog, the last
  // page we touch will still be within preH if seg_off + need <= preH.
  if (a->seg_off + need > a->preH)
  {
    return PC_NO_SPACE;
  }

  // Write block header
  pc_block_hdr_t hdr;
  hdr.metric_id = metric_id;
  hdr.series_id = series_id;
  hdr.start_ts = ts_array[0];
  hdr.point_count = npoints;

  pc_result_t st = emit_bytes(a, &hdr, sizeof(hdr));
  if (st != PC_OK)
    return st;

  // Write points
  for (uint32_t i = 0; i < npoints; ++i)
  {
    pc_point_disk_t pt;
    pt.ts = ts_array[i];
    pt.value = val_array[i];

    if (pt.ts < a->ts_min)
      a->ts_min = pt.ts;
    if (pt.ts > a->ts_max)
      a->ts_max = pt.ts;

    st = emit_bytes(a, &pt, sizeof(pt));
    if (st != PC_OK)
      return st;
  }

  a->record_count += npoints;
  return PC_OK;
}

pc_result_t pc_appender_commit(pc_appender_t *a, uint16_t type)
{
  if (!a || !a->open)
    return PC_EINVAL;

  // If we never appended anything, we still allow commit with zero records,
  // but ts_min/max will be both zero (okay for our tests; real system may forbid).
  if (a->page_off)
  {
    pc_result_t st = flush_page(a);
    if (st != PC_OK)
      return st;
  }

  pc_result_t rc = pc_logseg_commit(a->f, a->base, type, a->seqno,
                                    a->ts_min == 0xFFFFFFFFu ? 0u : a->ts_min,
                                    a->ts_max,
                                    a->record_count);
  if (rc == PC_OK)
    a->open = false;
  return rc;
}

size_t pc_appender_bytes_remaining(const pc_appender_t *a)
{
  if (!a)
    return 0;
  if (a->seg_off > a->preH)
    return 0;
  return a->preH - a->seg_off;
}
