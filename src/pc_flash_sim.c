#include "pc_flash.h"
#include <string.h> // memcpy, memset
#include <stdlib.h> // malloc, free
#include <limits.h>

static bool aligned(size_t x, size_t a) { return (a == 0) ? (x == 0) : (x % a) == 0; }
static size_t addr_to_sector(const pc_flash_t *f, size_t addr)
{
  return addr / f->sector_bytes;
}
static bool range_in_one_piece(const pc_flash_t *f, size_t addr, size_t len)
{
  return (addr + len) <= f->total_bytes;
}
static bool range_hits_bad(const pc_flash_t *f, size_t addr, size_t len)
{
  size_t start = addr_to_sector(f, addr);
  size_t end = addr_to_sector(f, addr + len - 1);
  if (len == 0)
    return false;
  if (end >= f->sector_count)
    return true; // out of range considered "bad"
  for (size_t s = start; s <= end; ++s)
  {
    if (f->bad[s])
      return true;
  }
  return false;
}

bool pc_flash_init(pc_flash_t *f,
                   size_t total_bytes,
                   size_t sector_bytes,
                   size_t prog_bytes,
                   uint8_t erased_val)
{
  if (!f)
    return false;
  // Geometry validation
  if (total_bytes == 0 || sector_bytes == 0 || prog_bytes == 0)
    return false;
  if (total_bytes % sector_bytes != 0)
    return false;
  // prog granularity should divide sector
  if (sector_bytes % prog_bytes != 0)
    return false;

  size_t sectors = total_bytes / sector_bytes;

  uint8_t *mem = (uint8_t *)malloc(total_bytes);
  uint32_t *wear = (uint32_t *)calloc(sectors, sizeof(uint32_t));
  bool *bad = (bool *)calloc(sectors, sizeof(bool));
  if (!mem || !wear || !bad)
  {
    free(mem);
    free(wear);
    free(bad);
    return false;
  }
  memset(mem, erased_val, total_bytes);

  f->mem = mem;
  f->total_bytes = total_bytes;
  f->sector_bytes = sector_bytes;
  f->prog_bytes = prog_bytes;
  f->sector_count = sectors;
  f->erased_val = erased_val;
  f->wear = wear;
  f->bad = bad;
  return true;
}

void pc_flash_free(pc_flash_t *f)
{
  if (!f)
    return;
  free(f->mem);
  f->mem = NULL;
  free(f->wear);
  f->wear = NULL;
  free(f->bad);
  f->bad = NULL;
  f->total_bytes = f->sector_bytes = f->prog_bytes = f->sector_count = 0;
  f->erased_val = 0xFF;
}

pc_result_t pc_flash_read(const pc_flash_t *f, size_t addr, void *out, size_t len)
{
  if (!f || !out)
    return PC_EINVAL;
  if (!range_in_one_piece(f, addr, len))
    return PC_EINVAL;
  if (len == 0)
    return PC_OK;
  if (range_hits_bad(f, addr, len))
    return PC_FLASH_IO;
  memcpy(out, f->mem + addr, len);
  return PC_OK;
}

pc_result_t pc_flash_program(pc_flash_t *f, size_t addr, const void *data, size_t len)
{
  if (!f || !data)
    return PC_EINVAL;
  if (len == 0)
    return PC_OK;
  if (!range_in_one_piece(f, addr, len))
    return PC_EINVAL;
  if (!aligned(addr, f->prog_bytes) || !aligned(len, f->prog_bytes))
    return PC_EINVAL;
  if (range_hits_bad(f, addr, len))
    return PC_FLASH_IO;

  const uint8_t *src = (const uint8_t *)data;
  uint8_t *dst = f->mem + addr;

  // Check no 0 -> 1 transitions
  for (size_t i = 0; i < len; ++i)
  {
    uint8_t oldb = dst[i];
    uint8_t newb = src[i];
    // If any bit is trying to go from 0 to 1, it's invalid
    if ((~oldb) & newb)
    {
      return PC_EINVAL;
    }
  }
  // Program = bitwise AND (1->0 only)
  for (size_t i = 0; i < len; ++i)
  {
    dst[i] &= src[i];
  }
  return PC_OK;
}

pc_result_t pc_flash_erase_sector(pc_flash_t *f, size_t sector_index)
{
  if (!f)
    return PC_EINVAL;
  if (sector_index >= f->sector_count)
    return PC_EINVAL;
  if (f->bad[sector_index])
    return PC_FLASH_IO;

  size_t base = sector_index * f->sector_bytes;
  memset(f->mem + base, f->erased_val, f->sector_bytes);
  // wear count bump (saturate at UINT32_MAX)
  if (f->wear[sector_index] != UINT32_MAX)
  {
    f->wear[sector_index] += 1;
  }
  return PC_OK;
}

pc_result_t pc_flash_mark_bad(pc_flash_t *f, size_t sector_index, bool is_bad)
{
  if (!f)
    return PC_EINVAL;
  if (sector_index >= f->sector_count)
    return PC_EINVAL;
  f->bad[sector_index] = is_bad;
  return PC_OK;
}

bool pc_flash_is_bad(const pc_flash_t *f, size_t sector_index)
{
  if (!f)
    return true;
  if (sector_index >= f->sector_count)
    return true;
  return f->bad[sector_index];
}

void pc_flash_wear_stats(const pc_flash_t *f, uint32_t *out_min, uint32_t *out_max, uint32_t *out_avg)
{
  if (!f || f->sector_count == 0)
    return;
  uint64_t sum = 0;
  uint32_t mn = UINT32_MAX, mx = 0;
  for (size_t i = 0; i < f->sector_count; ++i)
  {
    uint32_t w = f->wear[i];
    if (w < mn)
      mn = w;
    if (w > mx)
      mx = w;
    sum += w;
  }
  if (out_min)
    *out_min = mn;
  if (out_max)
    *out_max = mx;
  if (out_avg)
    *out_avg = (uint32_t)(sum / f->sector_count);
}
