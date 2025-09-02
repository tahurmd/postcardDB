// PR-010: Simple circular segment allocator
// - Chooses the next free segment (commit page erased == 0xFF)
// - Skips bad sectors
// - Wraps around the device
// - Returns PC_NO_SPACE if no free segment exists
//
// Notes:
// - "Free" means: commit page is fully erased (no header written).
// - We assume a single writer (our flusher), so no concurrent allocs.

#ifndef PC_ALLOC_H
#define PC_ALLOC_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "pc_result.h"
#include "pc_flash.h"
#include "pc_logseg.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    pc_flash_t* f;
    size_t seg_bytes;       // e.g., 4096
    size_t prog_bytes;      // e.g., 256
    size_t sector_count;    // total segments = total_bytes / seg_bytes
    size_t next_index;      // where to start the next search
} pc_alloc_t;

// Initialize allocator for the given flash device.
// Starts searching from index 0.
pc_result_t pc_alloc_init(pc_alloc_t* a, pc_flash_t* f);

// Acquire the base address of the next free segment, advancing next_index.
// Returns PC_OK and *out_base on success, PC_NO_SPACE if none available.
pc_result_t pc_alloc_acquire(pc_alloc_t* a, size_t* out_base);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // PC_ALLOC_H
