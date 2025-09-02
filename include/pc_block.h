// PR-007: Minimal block format + one-shot segment writer (no compression)
//
// Layout we write into the pre-header region:
//   [ pc_block_hdr_t ][ pc_point_disk_t x N ]
//
// Notes
// - This PR is "one block per segment" to keep things simple.
// - Only one metric/series per block.
// - A later PR can add multiple blocks, per-metric grouping, compression, etc.

#ifndef PC_BLOCK_H
#define PC_BLOCK_H

#include <stdint.h>
#include <stddef.h>
#include "pc_result.h"
#include "pc_flash.h"
#include "pc_logseg.h"

#ifdef __cplusplus
extern "C" {
#endif

// On-flash block header (tiny, packed).
typedef struct __attribute__((packed)) {
    uint16_t metric_id;     // metric dictionary id
    uint16_t series_id;     // series id (0 if untagged)
    uint32_t start_ts;      // first timestamp in this block
    uint32_t point_count;   // number of points following
} pc_block_hdr_t;

// On-flash point payload (no metric/series here; stored in the header)
typedef struct __attribute__((packed)) {
    uint32_t ts;            // unix seconds
    float    value;         // float32
} pc_point_disk_t;

// One-shot helper: writes a single block into the pre-header region and commits
// the segment header last (atomic). It:
//  - erases the segment (base must be segment-aligned),
//  - writes [block header + N points] sequentially using page-buffering,
//  - computes ts_min/ts_max from your timestamps,
//  - commits with type=PC_SEG_DATA and record_count=N.
//
// Returns PC_OK on success, or:
//  - PC_EINVAL (bad args/alignment),
//  - PC_NO_SPACE (block does not fit in pre-header),
//  - PC_FLASH_IO (simulated flash I/O error/bad sector).
pc_result_t pc_block_write_segment(pc_flash_t* f,
                                   size_t base,
                                   uint16_t metric_id,
                                   uint16_t series_id,
                                   const uint32_t* ts_array,
                                   const float*    val_array,
                                   uint32_t        npoints,
                                   uint32_t        seqno);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // PC_BLOCK_H
