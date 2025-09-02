// PR-006: Forward recovery scanner
// Scans flash linearly, verifying each 4KB segment:
//  - If commit page is erased → skip (uncommitted/partial).
//  - If header present but CRC fails → skip (corrupt).
//  - If sector marked bad / I/O error → skip.
//  - Otherwise → return a small summary for the caller.
//
// Matches README: "Forward recovery (linear, idempotent)".

#ifndef PC_RECOVER_H
#define PC_RECOVER_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "pc_result.h"
#include "pc_flash.h"
#include "pc_logseg.h"

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct
  {
    size_t base;           // segment base address (sector-aligned)
    uint16_t type;         // pc_seg_type_t
    uint32_t seqno;        // segment sequence number
    uint32_t ts_min;       // earliest timestamp
    uint32_t ts_max;       // latest timestamp
    uint32_t record_count; // logical records encoded
  } pc_seg_summary_t;

  // Scan the entire device and collect valid segments (in address order).
  // - out can be NULL if you only care that it runs.
  // - *found returns how many entries were written to 'out' (<= max_out).
  // Returns PC_OK on success, PC_EINVAL on bad args.
  pc_result_t pc_recover_scan_all(const pc_flash_t *f,
                                  pc_seg_summary_t *out,
                                  size_t max_out,
                                  size_t *found);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // PC_RECOVER_H
