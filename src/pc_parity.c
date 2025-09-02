#include "pc_parity.h"

uint8_t pc_parity8_update(uint8_t prev, const void *data, size_t len)
{
  const uint8_t *p = (const uint8_t *)data;
  uint8_t x = prev;
  while (len--)
    x ^= *p++;
  return x;
}

uint8_t pc_parity8(const void *data, size_t len)
{
  return pc_parity8_update(0u, data, len);
}
