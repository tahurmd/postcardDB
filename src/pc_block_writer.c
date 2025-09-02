#include "pc_block.h"
#include <string.h>
#include <stdbool.h>

// Return 1 if x is a power of two (and non-zero), else 0.
static int is_pow2(size_t x) { return x && ((x & (x - 1)) == 0); }

// Small context for page-buffered programming into the pre-header region.
typedef struct
{
  pc_flash_t *f;
  size_t base; // segment base address (sector-aligned)
  size_t prog; // program granularity (e.g., 256)
  size_t preH; // pre-header size = segment_bytes - prog

  // Page staging buffer: prefilled with 0xFF so we only do 1->0 transitions.
  // We assume prog <= 512 (true for our geometry).
  uint8_t page[512];
  size_t page_off; // bytes filled in current page [0..prog]
  size_t seg_off;  // total bytes written into pre-header so far [0..preH]
} pc_bw_ctx_t;

// Program the current staged page to flash (if anything is staged).
static pc_result_t pc_bw_flush_page(pc_bw_ctx_t *c)
{
  if (c->page_off == 0)
    return PC_OK; // nothing to write
  size_t addr = c->base + (c->seg_off - c->page_off);
  if ((addr % c->prog) != 0)
    return PC_EINVAL; // must be page-aligned

  pc_result_t rc = pc_flash_program(c->f, addr, c->page, c->prog);

  // Reset buffer for next page.
  memset(c->page, 0xFF, c->prog);
  c->page_off = 0;
  return rc;
}

// Append bytes to the staged page; when full, program one page.
// Also protects against overrunning the pre-header region.
static pc_result_t pc_bw_emit_bytes(pc_bw_ctx_t *c, const void *src, size_t len)
{
  const uint8_t *p = (const uint8_t *)src;
  while (len)
  {
    size_t space = c->prog - c->page_off;
    size_t chunk = (len < space) ? len : space;

    // Copy into the staging buffer. (We prefilled with 0xFF; memcpy is fine.)
    memcpy(c->page + c->page_off, p, chunk);
    c->page_off += chunk;
    c->seg_off += chunk;
    p += chunk;
    len -= chunk;

    // Safety: never exceed pre-header region.
    if (c->seg_off > c->preH)
      return PC_NO_SPACE;

    // If the page is full, program it.
    if (c->page_off == c->prog)
    {
      pc_result_t rc = pc_bw_flush_page(c);
      if (rc != PC_OK)
        return rc;
    }
  }
  return PC_OK;
}

pc_result_t pc_block_write_segment(pc_flash_t *f,
                                   size_t base,
                                   uint16_t metric_id,
                                   uint16_t series_id,
                                   const uint32_t *ts_array,
                                   const float *val_array,
                                   uint32_t npoints,
                                   uint32_t seqno)
{
  if (!f || !ts_array || !val_array)
    return PC_EINVAL;
  if (npoints == 0)
    return PC_EINVAL;

  const size_t seg = pc_flash_sector_bytes(f);
  const size_t prog = pc_flash_prog_bytes(f);
  const size_t preH = seg - prog; // pre-header region size

  if (seg == 0 || prog == 0)
    return PC_EINVAL;
  if (!is_pow2(seg) || !is_pow2(prog))
    return PC_EINVAL;
  if ((base % seg) != 0)
    return PC_EINVAL;
  if (prog > 512)
    return PC_EINVAL; // our simple staging buffer is 512 bytes

  // Compute total bytes we intend to write into the pre-header: header + N points.
  const size_t header_sz = sizeof(pc_block_hdr_t);
  const size_t point_sz = sizeof(pc_point_disk_t);
  const size_t total_sz = header_sz + (size_t)npoints * point_sz;

  // We program whole pages. Ensure the padded size still fits pre-header.
  const size_t needed = ((total_sz + prog - 1) / prog) * prog;
  if (needed > preH)
    return PC_NO_SPACE;

  // Erase the whole segment first (required for programming).
  pc_result_t st = pc_logseg_erase(f, base);
  if (st != PC_OK)
    return st;

  // Initialize the writer context.
  pc_bw_ctx_t ctx;
  ctx.f = f;
  ctx.base = base;
  ctx.prog = prog;
  ctx.preH = preH;
  ctx.page_off = 0;
  ctx.seg_off = 0;
  memset(ctx.page, 0xFF, prog);

  // Block header
  pc_block_hdr_t hdr;
  hdr.metric_id = metric_id;
  hdr.series_id = series_id;
  hdr.start_ts = ts_array[0];
  hdr.point_count = npoints;

  st = pc_bw_emit_bytes(&ctx, &hdr, sizeof(hdr));
  if (st != PC_OK)
    return st;

  // Stream points and track ts_min/ts_max.
  uint32_t ts_min = ts_array[0];
  uint32_t ts_max = ts_array[0];

  for (uint32_t i = 0; i < npoints; ++i)
  {
    pc_point_disk_t pt;
    pt.ts = ts_array[i];
    pt.value = val_array[i];

    if (pt.ts < ts_min)
      ts_min = pt.ts;
    if (pt.ts > ts_max)
      ts_max = pt.ts;

    st = pc_bw_emit_bytes(&ctx, &pt, sizeof(pt));
    if (st != PC_OK)
      return st;
  }

  // If there are leftover bytes in the current page, program that page too.
  if (ctx.page_off)
  {
    st = pc_bw_flush_page(&ctx);
    if (st != PC_OK)
      return st;
  }

  // Final safety: we must be within pre-header bounds.
  if (ctx.seg_off > preH)
    return PC_NO_SPACE;

  // Commit the segment header last (atomic).
  return pc_logseg_commit(f, base, PC_SEG_DATA, seqno, ts_min, ts_max, npoints);
}
