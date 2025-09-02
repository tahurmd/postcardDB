// PR-009: Minimal public API slice (host-only, no networking)
// - pc_db_init / pc_db_deinit
// - pc_write: enqueue a point into the SPSC ring
// - pc_db_flush_once / pc_db_flush_until_empty: drain ring -> flash (multi-block segments)
// - pc_query_latest: scan committed segments for latest value of a metric
//
// Notes
// - Single-writer flusher, as per SPSC plan (Core1 in firmware; here we call it directly in tests).
// - No compression yet. One metric/series per block; blocks are packed back-to-back.

#ifndef PC_API_H
#define PC_API_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "pc_result.h"
#include "pc_ring.h"
#include "pc_flash.h"
#include "pc_appender.h"
#include "pc_recover.h"
#include "pc_block.h"
#include "pc_logseg.h"

#ifdef __cplusplus
extern "C"
{
#endif

  // Ring point format (RAM)
  typedef struct
  {
    uint32_t ts;        // Unix seconds
    uint16_t metric_id; // metric
    uint16_t series_id; // series (0 if untagged)
    float value;        // sample
  } pc_point_ram_t;

  // Opaque DB handle (small, fixed-size)
  typedef struct
  {
    // Flash device (sim on host)
    pc_flash_t *flash;

    // Ring buffer (SPSC)
    pc_ring_t ring;
    // Storage for ring elements (pc_point_ram_t)
    // Capacity chosen at init; pointer + bytes_per_elem are in pc_ring_t.
    // We keep ownership of the backing array here for cleanup.
    pc_point_ram_t *ring_storage;
    uint32_t ring_capacity; // number of elements

    // Appender for current open segment (if any)
    pc_appender_t app;
    bool app_open;

    // Monotonic segment sequence number
    uint32_t next_seq;
  } pc_db_t;

  // Initialize the DB with a flash device + ring capacity (elements).
  // seq_start: initial segment sequence number.
  // Returns PC_OK or PC_EINVAL / allocation failures as PC_EINVAL.
  pc_result_t pc_db_init(pc_db_t *db, pc_flash_t *flash,
                         uint32_t ring_capacity_elems,
                         uint32_t seq_start);

  // Free allocations and close any open appender (does not erase/commit).
  void pc_db_deinit(pc_db_t *db);

  // Enqueue a point into the ring. Returns:
  //   PC_OK      - enqueued
  //   PC_BUSY    - ring full (caller may retry later)
  pc_result_t pc_write(pc_db_t *db, uint16_t metric_id, uint16_t series_id,
                       uint32_t ts, float value);

  // Drain a limited number of points from the ring and append as ONE block.
  // - Opens a segment appender if none is open.
  // - Pack contiguous points that share the FIRST point's (metric_id, series_id).
  // - If the next point in ring has different metric/series, stop (leave it for next call).
  // - If block would not fit, commit current segment, open a new one, then write block.
  // Returns PC_OK (even if ring was empty and nothing happened), or an error from lower layers.
  pc_result_t pc_db_flush_once(pc_db_t *db);

  // Drain the ring entirely, committing current segment at the end.
  pc_result_t pc_db_flush_until_empty(pc_db_t *db);

  // Latest value for a metric across committed segments (and ignores uncommitted).
  // Scans in address order, decodes blocks, and keeps the max timestamp for metric_id.
  // Returns PC_OK if found at least one sample; PC_METRIC_UNKNOWN if none found.
  pc_result_t pc_query_latest(pc_db_t *db, uint16_t metric_id,
                              float *out_value, uint32_t *out_ts);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // PC_API_H
