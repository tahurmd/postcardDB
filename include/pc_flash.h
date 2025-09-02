// PR-004: Flash simulator (host-only)
// - Models NOR-like flash with:
//     * erase-by-sector (e.g., 4096 bytes)
//     * program granularity (e.g., 256 bytes)
//     * bit transitions only 1 -> 0 (never 0 -> 1 without erase)
//     * per-sector wear counters and bad-sector flags
// - Used by tests and higher layers (segment writer/recovery) on host.
//
// Typical geometry for Pico flash in our README: sector=4KB, prog=256B.
// We'll keep the geometry configurable for tests.

#ifndef PC_FLASH_H
#define PC_FLASH_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "pc_result.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    // Backing store
    uint8_t*  mem;            // total_bytes
    size_t    total_bytes;

    // Geometry
    size_t    sector_bytes;   // e.g., 4096
    size_t    prog_bytes;     // e.g., 256
    size_t    sector_count;   // = total_bytes / sector_bytes
    uint8_t   erased_val;     // usually 0xFF

    // Bookkeeping
    uint32_t* wear;           // erase count per sector
    bool*     bad;            // bad sector flags
} pc_flash_t;

// --- Lifecycle ---

// Create in-memory flash with given geometry. All bytes set to erased_val.
// Returns false on invalid geometry or allocation failure.
bool pc_flash_init(pc_flash_t* f,
                   size_t total_bytes,
                   size_t sector_bytes,
                   size_t prog_bytes,
                   uint8_t erased_val);

// Free internal allocations. Safe on a zeroed struct.
void pc_flash_free(pc_flash_t* f);

// --- Geometry helpers ---
static inline size_t pc_flash_total(const pc_flash_t* f){ return f->total_bytes; }
static inline size_t pc_flash_sector_bytes(const pc_flash_t* f){ return f->sector_bytes; }
static inline size_t pc_flash_prog_bytes(const pc_flash_t* f){ return f->prog_bytes; }
static inline size_t pc_flash_sector_count(const pc_flash_t* f){ return f->sector_count; }

// --- I/O ---

// Read len bytes at addr into out. Bounds/bad-sector checked.
// Returns PC_OK or an error (PC_EINVAL for bounds, PC_FLASH_IO for bad sectors).
pc_result_t pc_flash_read(const pc_flash_t* f, size_t addr, void* out, size_t len);

// Program len bytes at addr. Requirements:
//  * addr and len must be multiples of prog_bytes
//  * cannot set any bit 0 -> 1 (must have been erased)
//  * cannot touch bad sectors
// Returns PC_OK, PC_EINVAL (alignment/bounds/bit-violation), or PC_FLASH_IO (bad sector).
pc_result_t pc_flash_program(pc_flash_t* f, size_t addr, const void* data, size_t len);

// Erase a whole sector to erased_val, increment wear counter.
// Returns PC_OK, PC_EINVAL (out of range), PC_FLASH_IO (bad sector).
pc_result_t pc_flash_erase_sector(pc_flash_t* f, size_t sector_index);

// --- Maintenance ---

// Mark/unmark a sector as bad. Returns PC_EINVAL if index out of range.
pc_result_t pc_flash_mark_bad(pc_flash_t* f, size_t sector_index, bool is_bad);

// Query bad flag.
bool pc_flash_is_bad(const pc_flash_t* f, size_t sector_index);

// Wear stats (min/max/avg) over all sectors. If any pointer is NULL, it's skipped.
void pc_flash_wear_stats(const pc_flash_t* f, uint32_t* out_min, uint32_t* out_max, uint32_t* out_avg);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // PC_FLASH_H
