// CRC32C (Castagnoli) – PR-003
// Streaming usage:
//   uint32_t crc = PC_CRC32C_SEED;                 // begin
//   crc = pc_crc32c_update(crc, buf1, len1);       // update 1..N
//   crc = pc_crc32c_update(crc, buf2, len2);
//   uint32_t final = PC_CRC32C_FINALIZE(crc);      // finalize
//
// One-shot usage:
//   uint32_t final = pc_crc32c(buf, len);          // does seed+update+finalize
#ifndef PC_CRC32C_H
#define PC_CRC32C_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

// Standard CRC32C init/final constants
#define PC_CRC32C_SEED 0xFFFFFFFFu
#define PC_CRC32C_FINALIZE(x) ((uint32_t)~(x))

  // Streaming update: takes current CRC state (seeded) and returns new state (not finalized).
  uint32_t pc_crc32c_update(uint32_t crc, const void *data, size_t len);

  // One-shot helper: computes CRC32C(buf,len) with standard seed and xor-out.
  static inline uint32_t pc_crc32c(const void *data, size_t len)
  {
    return PC_CRC32C_FINALIZE(pc_crc32c_update(PC_CRC32C_SEED, data, len));
  }

#ifdef __cplusplus
} // extern "C"
#endif

#endif // PC_CRC32C_H
