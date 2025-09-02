// PR-005: 4 KB segment format + header-last commit.
//
// Layout inside one flash *segment* (one erase block, e.g., 4096 B):
//
//  [ base ............................................... base+H-1 ][ base+H .. base+S-1 ]
//  |<------------------- pre-header region  ---------------------->|<--- commit page ---->|
//   payload + block headers + untouched (still 0xFF) bytes            segment commit header
//
// Where:
//   S = segment_bytes (e.g., 4096)
//   P = program_bytes  (e.g., 256)
//   H = S - P  (the last P bytes are reserved for the commit header)
//
// Crash safety: we only write the commit header *last* on the final page.
// CRC32C covers the entire pre-header region [base, base+H) exactly as present
// in flash at commit time (including any 0xFF bytes you never wrote).
//
// Verification = recompute CRC over [base, base+H) and compare with header.
//
// Notes:
// - We keep the header small (fits at the start of the final program page).
// - Programming obeys flash rules via pc_flash_* (alignment, 1->0 only).
// - This module is host-side, built on top of pc_flash_sim.

#ifndef PC_LOGSEG_H
#define PC_LOGSEG_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "pc_result.h"
#include "pc_flash.h"

#ifdef __cplusplus
extern "C" {
#endif

// Magic 'PCD1' (PostCarD format v1)
#define PC_SEG_MAGIC  0x50434431u  // 'P' 'C' 'D' '1'
#define PC_SEG_VERSION 1

typedef enum {
    PC_SEG_DATA  = 1,  // data segment (payload + block headers)
    PC_SEG_INDEX = 2,  // snapshot / index segment
    PC_SEG_EPOCH = 3   // epoch marker (bounded recovery)
} pc_seg_type_t;

// Commit header written on the *last* program page of the segment.
// (We write a full page, header packed at the front, remaining bytes left 0xFF.)
typedef struct __attribute__((packed)) {
    uint32_t magic;        // PC_SEG_MAGIC
    uint16_t version;      // PC_SEG_VERSION
    uint16_t type;         // pc_seg_type_t
    uint32_t seqno;        // monotonically increasing segment sequence
    uint32_t ts_min;       // earliest timestamp contained
    uint32_t ts_max;       // latest timestamp contained
    uint32_t record_count; // number of points/records encoded
    uint32_t crc32c;       // CRC32C over [base .. base+H), i.e., everything before this header
} pc_segment_hdr_t;

// Geometry helpers for the segment at a given flash device
static inline size_t pc_logseg_segment_bytes(const pc_flash_t* f) { return pc_flash_sector_bytes(f); }
static inline size_t pc_logseg_commit_page_bytes(const pc_flash_t* f){ return pc_flash_prog_bytes(f); }
static inline size_t pc_logseg_preheader_bytes(const pc_flash_t* f) {
    return pc_logseg_segment_bytes(f) - pc_logseg_commit_page_bytes(f);
}

// Convenience: erase the segment containing 'base' (base must be sector-aligned)
pc_result_t pc_logseg_erase(pc_flash_t* f, size_t base);

// Write data into the pre-header region at [base + offset, ...].
// Enforces bounds + program alignment + not overlapping the commit page.
// Returns PC_OK or an error code.
pc_result_t pc_logseg_program_data(pc_flash_t* f, size_t base, size_t offset, const void* data, size_t len);

// Compute CRC32C over the full pre-header region [base .. base+H).
pc_result_t pc_logseg_crc32c_region(const pc_flash_t* f, size_t base, uint32_t* out_crc);

// Write the commit header (last step). This is the atomic "commit".
pc_result_t pc_logseg_commit(pc_flash_t* f, size_t base,
                             uint16_t type, uint32_t seqno,
                             uint32_t ts_min, uint32_t ts_max, uint32_t record_count);

// Read & verify a segment. Returns:
//   PC_OK       → committed and CRC is valid (out_hdr filled)
//   PC_CORRUPT  → header present but CRC mismatch or bad magic/version
//   PC_EINVAL   → bad alignment/args
pc_result_t pc_logseg_verify(const pc_flash_t* f, size_t base, pc_segment_hdr_t* out_hdr);

// Helper: is the commit page still erased (no header written)?
bool pc_logseg_header_erased(const pc_flash_t* f, size_t base);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // PC_LOGSEG_H
