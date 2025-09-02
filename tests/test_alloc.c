// PR-010 tests: allocator rotates across segments and returns NO_SPACE when full.
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "pc_api.h"
#include "pc_recover.h"

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
  // 20KB total → 5 segments of 4KB, prog 256B
  const size_t TOTAL = 20 * 1024, SEG = 4096, PROG = 256;
  pc_flash_t f = {0};
  expect(pc_flash_init(&f, TOTAL, SEG, PROG, 0xFF), "flash init");

  // Mark the 3rd segment bad to exercise skipping (index 2)
  expect(pc_flash_mark_bad(&f, 2, true) == PC_OK, "mark bad seg2");

  pc_db_t db;
  expect(pc_db_init(&db, &f, /*ring cap*/ 2048, /*seq start*/ 1) == PC_OK, "db init");

  // Push enough points to force multiple segments.
  // Each segment preH ≈ 3840 bytes, point=8 bytes + small headers; about 470-480 pts/segment.
  // We'll write ~1200 points for metric 1 to land across 3 segments (skipping bad one).
  for (uint32_t i = 0; i < 1200; ++i)
  {
    expect(pc_write(&db, 1, 0, 1000 + i, (float)i) == PC_OK, "write");
  }

  // Flush everything
  expect(pc_db_flush_until_empty(&db) == PC_OK, "flush all");

  // Recovery: we expect 3 valid segments at indices 0, 3, 4 (since index 2 is bad, index 1 fills after 0? depends on sizes)
  // Instead of hardcoding exact indices, we just require >=2 segments and that bases are strictly increasing.
  pc_seg_summary_t segs[8];
  size_t n = 0;
  expect(pc_recover_scan_all(&f, segs, 8, &n) == PC_OK, "recover ok");
  expect(n >= 2, "at least two segments");
  for (size_t i = 1; i < n; ++i)
  {
    expect(segs[i].base > segs[i - 1].base, "bases increasing");
  }

  // Now try to keep allocating until the device fills, expect NO_SPACE eventually.
  size_t dummy_base = 0;
  pc_alloc_t alloc;
  expect(pc_alloc_init(&alloc, &f) == PC_OK, "alloc init");
  // Consume all remaining free segments by acquiring and immediately committing empty ones.
  // (We simulate "use" by writing nothing and committing via appender.)
  for (int k = 0; k < 10; ++k)
  {
    pc_result_t st = pc_alloc_acquire(&alloc, &dummy_base);
    if (st == PC_NO_SPACE)
      break;
    pc_appender_t a;
    expect(pc_appender_open(&a, &f, dummy_base, 1000 + k) == PC_OK, "open empty");
    expect(pc_appender_commit(&a, PC_SEG_DATA) == PC_OK, "commit empty");
  }
  // Now allocator should report no space.
  expect(pc_alloc_acquire(&alloc, &dummy_base) == PC_NO_SPACE, "no space after filling");

  pc_db_deinit(&db);
  pc_flash_free(&f);
  puts("alloc: ok");
  return 0;
}
