#include "pc_api.h"
#include <string.h>
#include <stdlib.h>

// Internal limits to keep code tiny & safe
#define PC_BLOCK_MAX_POINTS 128u // one block per flush (cap)
#define PC_READBUF_PAGE 256u     // expected program granularity

pc_result_t pc_db_init(pc_db_t *db, pc_flash_t *flash,
                       uint32_t ring_capacity_elems,
                       uint32_t seq_start)
{
  if (!db || !flash || ring_capacity_elems == 0)
    return PC_EINVAL;

  memset(db, 0, sizeof(*db));
  db->flash = flash;
  db->ring_storage = (pc_point_ram_t *)calloc(ring_capacity_elems, sizeof(pc_point_ram_t));
  if (!db->ring_storage)
    return PC_EINVAL;

  if (!pc_ring_init(&db->ring, db->ring_storage, ring_capacity_elems, sizeof(pc_point_ram_t)))
  {
    free(db->ring_storage);
    db->ring_storage = NULL;
    return PC_EINVAL;
  }
  db->ring_capacity = ring_capacity_elems;
  db->next_seq = seq_start;
  db->app_open = false;
  return PC_OK;
}

void pc_db_deinit(pc_db_t *db)
{
  if (!db)
    return;
  // Note: we don't commit an open segment here; caller should flush explicitly.
  db->app_open = false;
  free(db->ring_storage);
  db->ring_storage = NULL;
  memset(&db->ring, 0, sizeof(db->ring));
}

pc_result_t pc_write(pc_db_t *db, uint16_t metric_id, uint16_t series_id,
                     uint32_t ts, float value)
{
  if (!db)
    return PC_EINVAL;
  pc_point_ram_t p;
  p.ts = ts;
  p.metric_id = metric_id;
  p.series_id = series_id;
  p.value = value;
  size_t pushed = pc_ring_push(&db->ring, &p, 1);
  return (pushed == 1) ? PC_OK : PC_BUSY;
}

// Pop into dst up to cap, but only while metric/series match the first element.
// Returns number popped (0..cap).
static uint32_t ring_pop_same_series(pc_ring_t *r, pc_point_ram_t *dst, uint32_t cap)
{
  if (pc_ring_is_empty(r) || cap == 0)
    return 0;

  // Peek first
  const pc_point_ram_t *first = (const pc_point_ram_t *)pc_ring_peek(r);
  if (!first)
    return 0;

  uint16_t m = first->metric_id;
  uint16_t s = first->series_id;

  // Pop first into dst[0]
  (void)pc_ring_pop(r, dst, 1);
  uint32_t n = 1;

  // Keep peeking & popping while series/metric match
  while (n < cap && !pc_ring_is_empty(r))
  {
    const pc_point_ram_t *nxt = (const pc_point_ram_t *)pc_ring_peek(r);
    if (!nxt)
      break;
    if (nxt->metric_id != m || nxt->series_id != s)
      break;
    (void)pc_ring_pop(r, dst + n, 1);
    n++;
  }
  return n;
}

pc_result_t pc_db_flush_once(pc_db_t *db)
{
  if (!db)
    return PC_EINVAL;

  if (pc_ring_is_empty(&db->ring))
  {
    // Nothing to do. If an appender is open but empty, leave it open for next time.
    return PC_OK;
  }

  // Open appender lazily
  if (!db->app_open)
  {
    pc_result_t st = pc_appender_open(&db->app, db->flash, /*base*/ 0 + 0, db->next_seq++);
    if (st != PC_OK)
      return st;
    db->app_open = true;
  }

  // Drain one block worth of same (metric, series)
  pc_point_ram_t buf[PC_BLOCK_MAX_POINTS];
  uint32_t n = ring_pop_same_series(&db->ring, buf, PC_BLOCK_MAX_POINTS);
  if (n == 0)
    return PC_OK; // nothing matched (unlikely)

  // Split into simple arrays for appender
  uint32_t ts[PC_BLOCK_MAX_POINTS];
  float val[PC_BLOCK_MAX_POINTS];
  uint16_t metric = buf[0].metric_id;
  uint16_t series = buf[0].series_id;

  for (uint32_t i = 0; i < n; ++i)
  {
    ts[i] = buf[i].ts;
    val[i] = buf[i].value;
  }

  // Try to append; if no space, commit and open a new segment
  pc_result_t st = pc_appender_append_block(&db->app, metric, series, ts, val, n);
  if (st == PC_NO_SPACE)
  {
    // Commit current, open new, then append
    pc_result_t rc = pc_appender_commit(&db->app, PC_SEG_DATA);
    if (rc != PC_OK)
      return rc;
    db->app_open = false;

    rc = pc_appender_open(&db->app, db->flash, /*base*/ 0 + 0, db->next_seq++);
    if (rc != PC_OK)
      return rc;
    db->app_open = true;

    st = pc_appender_append_block(&db->app, metric, series, ts, val, n);
  }
  return st;
}

pc_result_t pc_db_flush_until_empty(pc_db_t *db)
{
  if (!db)
    return PC_EINVAL;
  pc_result_t st = PC_OK;

  while (!pc_ring_is_empty(&db->ring))
  {
    st = pc_db_flush_once(db);
    if (st != PC_OK && st != PC_NO_SPACE)
      return st;
  }
  // If an appender is open, commit it to finalize the segment.
  if (db->app_open)
  {
    st = pc_appender_commit(&db->app, PC_SEG_DATA);
    if (st != PC_OK)
      return st;
    db->app_open = false;
  }
  return PC_OK;
}

// ---- Reader helpers for query_latest ----

// Decode blocks sequentially until we've consumed 'record_count' points.
static pc_result_t scan_segment_latest(const pc_flash_t *f, size_t base,
                                       uint32_t record_count, uint16_t metric_id,
                                       uint32_t *out_ts, float *out_val)
{
  const size_t prog = pc_flash_prog_bytes(f);
  const size_t preH = pc_flash_sector_bytes(f) - prog;

  size_t off = 0; // offset into pre-header
  uint32_t seen = 0;
  uint32_t best_ts = 0;
  float best_val = 0.0f;

  uint8_t pg[PC_READBUF_PAGE];
  if (prog > sizeof(pg))
    return PC_EINVAL;

  while (seen < record_count && off < preH)
  {
    // Read block header
    pc_block_hdr_t bh;
    if (off + sizeof(bh) > preH)
      break;
    // We can read header even if it crosses page boundary by assembling bytes.
    // For simplicity, read header in one go (flash_read handles splitting across sectors/pages).
    pc_result_t st = pc_flash_read(f, base + off, &bh, sizeof(bh));
    if (st != PC_OK)
      return st;
    off += sizeof(bh);

    // Read points for this block
    for (uint32_t i = 0; i < bh.point_count; ++i)
    {
      pc_point_disk_t pt;
      if (off + sizeof(pt) > preH)
        return PC_CORRUPT;
      st = pc_flash_read(f, base + off, &pt, sizeof(pt));
      if (st != PC_OK)
        return st;
      off += sizeof(pt);

      if (bh.metric_id == metric_id)
      {
        if (pt.ts >= best_ts)
        {
          best_ts = pt.ts;
          best_val = pt.value;
        }
      }
      seen++;
      if (seen >= record_count)
        break;
    }
  }

  if (best_ts == 0 && record_count > 0)
  {
    return PC_METRIC_UNKNOWN; // metric not found in this segment
  }
  *out_ts = best_ts;
  *out_val = best_val;
  return PC_OK;
}

pc_result_t pc_query_latest(pc_db_t *db, uint16_t metric_id,
                            float *out_value, uint32_t *out_ts)
{
  if (!db || !out_value || !out_ts)
    return PC_EINVAL;

  // Scan all committed segments via recovery
  pc_seg_summary_t segs[16];
  size_t n = 0;
  pc_result_t st = pc_recover_scan_all(db->flash, segs, 16, &n);
  if (st != PC_OK)
    return st;

  uint32_t best_ts = 0;
  float best_val = 0.0f;
  bool found = false;

  for (size_t i = 0; i < n; ++i)
  {
    // Verify header to get record_count (already in summary) and sanity
    pc_segment_hdr_t hdr;
    st = pc_logseg_verify(db->flash, segs[i].base, &hdr);
    if (st != PC_OK)
      continue;

    uint32_t ts;
    float val;
    st = scan_segment_latest(db->flash, segs[i].base, hdr.record_count, metric_id, &ts, &val);
    if (st == PC_OK)
    {
      if (!found || ts >= best_ts)
      {
        best_ts = ts;
        best_val = val;
        found = true;
      }
    }
  }

  if (!found)
    return PC_METRIC_UNKNOWN;
  *out_ts = best_ts;
  *out_value = best_val;
  return PC_OK;
}
