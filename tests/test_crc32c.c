#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include "pc_crc32c.h"
#include "pc_parity.h"

static void expect(int cond, const char *msg)
{
  if (!cond)
  {
    fprintf(stderr, "FAIL: %s\n", msg);
    exit(1);
  }
}

int main(void)
{
  // Standard test vector for CRC32C (Castagnoli): "123456789"
  const char *s = "123456789";
  uint32_t one_shot = pc_crc32c(s, 9);
  expect(one_shot == 0xE3069283u, "crc32c one-shot 123456789");

  // Streaming vs one-shot should match
  uint32_t crc = PC_CRC32C_SEED;
  crc = pc_crc32c_update(crc, s, 4);     // "1234"
  crc = pc_crc32c_update(crc, s + 4, 5); // "56789"
  uint32_t final = PC_CRC32C_FINALIZE(crc);
  expect(final == one_shot, "crc32c streaming matches one-shot");

  // Parity sanity
  uint8_t p1 = pc_parity8("AB", 2); // 'A'^'B' = 0x41 ^ 0x42 = 0x03
  expect(p1 == (uint8_t)(0x41 ^ 0x42), "parity AB");

  uint8_t p2 = pc_parity8_update(0, "A", 1);
  p2 = pc_parity8_update(p2, "B", 1);
  expect(p2 == p1, "parity streaming matches one-shot");

  puts("crc32c: ok");
  return 0;
}
