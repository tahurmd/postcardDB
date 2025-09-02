// PR-008: Multi-block segment appender (no compression)
// - Writes any number of blocks into the pre-header region
//   [ pc_block_hdr_t ][ pc_point_disk_t x N ] [ next block ] ...
// - Maintains running ts_min / ts_max / record_count
// - Flushes program pages as needed, commits header last (atomic)
// - Safe for a single writer (the flusher on Core1).
//
// Typical flow:
//   pc_appender_t a;
//   pc_appender_open(&a, flash, base, seqno);
//   pc_appender_append_block(&a, metric_id, series_id, ts[], val[], n);
//   pc_appender_append_block(&a, ...);
//   pc_appender_commit(&a, PC_SEG_DATA);  // header-last
//
// Notes:
// - All writes respect flash rules (page-aligned programs, 1->0 only).
// - If there isn't enough space for the next block -> PC_NO_SPACE.
// - Once committed, appender is closed and cannot append more.

#ifndef PC_APPENDER_H
#define PC_APPENDER_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "pc_result.h"
#include "pc_flash.h"
#include "pc_logseg.h"
#include "pc_block.h"

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct
  {
    pc_flash_t *f;
    size_t base; // segment base address (sector-aligned)
    size_t seg;  // segment bytes (e.g., 4096)
    size_t prog; // program bytes (e.g., 256)
    size_t preH; // pre-header size = seg - prog

    // staging buffer for one program page (prefilled 0xFF)
    uint8_t page[512];
    size_t page_off; // bytes currently staged in 'page' [0..prog]
    size_t seg_off;  // total bytes written into pre-header [0..preH]

    // running stats for segment header
    uint32_t ts_min;
    uint32_t ts_max;
    uint32_t record_count;

    // bookkeeping
    uint32_t seqno;
    bool open; // true after open/erase, false after commit/close
  } pc_appender_t;

  // Open appender on a fresh (erased) segment at 'base' with given seqno.
  // This function erases the segment and initializes the context.
  pc_result_t pc_appender_open(pc_appender_t *a, pc_flash_t *f, size_t base, uint32_t seqno);

  // Append one block (header + N points). Updates ts_min/max and record_count.
  // Returns PC_OK or PC_NO_SPACE if the block would not fit (nothing is written in that case).
  pc_result_t pc_appender_append_block(pc_appender_t *a,
                                       uint16_t metric_id,
                                       uint16_t series_id,
                                       const uint32_t *ts_array,
                                       const float *val_array,
                                       uint32_t npoints);

  // Commit the segment (header-last) with accumulated stats; closes the appender.
  pc_result_t pc_appender_commit(pc_appender_t *a, uint16_t type);

  // How many bytes remain in the pre-header region (approx; not counting the staged page not yet flushed).
  size_t pc_appender_bytes_remaining(const pc_appender_t *a);

  // Is the appender still open (not committed)?
  static inline bool pc_appender_is_open(const pc_appender_t *a) { return a && a->open; }

#ifdef __cplusplus
} // extern "C"
#endif

#endif // PC_APPENDER_H
