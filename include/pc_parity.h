// Simple 8-bit XOR parity for quick scanning / low-cost checksums.
// Not a substitute for CRC; use it as a fast prefilter.
#ifndef PC_PARITY_H
#define PC_PARITY_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// One-shot: XOR of all bytes (0..255).
uint8_t pc_parity8(const void* data, size_t len);

// Streaming update: returns (prev_parity ^ XOR(bytes)).
uint8_t pc_parity8_update(uint8_t prev, const void* data, size_t len);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // PC_PARITY_H
