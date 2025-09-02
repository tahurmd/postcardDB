// PR-009 tests: write -> flush -> query_latest
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "pc_api.h"
#include "pc_alloc.h"

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
  // Flash 32KB: 8 segments of 4KB, prog 256B
  const size_t TOTAL = 32 * 1024, SEG = 4096, PROG = 256;
  pc_flash_t f = {0};
  expect(pc_flash_init(&f, TOTAL, SEG, PROG, 0xFF), "flash init");

  pc_db_t db;
  expect(pc_db_init(&db, &f, /*ring cap*/ 512, /*seq start*/ 1) == PC_OK, "db init");

  // Write 50 points for metric 1
  for (uint32_t i = 0; i < 50; ++i)
  {
    expect(pc_write(&db, /*metric*/ 1, /*series*/ 0, /*ts*/ 1000 + i, /*val*/ (float)i) == PC_OK, "write m1");
  }

  // Flush all and query_latest
  expect(pc_db_flush_until_empty(&db) == PC_OK, "flush all");
  float v = 0;
  uint32_t ts = 0;
  expect(pc_query_latest(&db, 1, &v, &ts) == PC_OK, "latest m1");
  expect(ts == 1049u, "ts m1");
  expect(v == 49.0f, "val m1");

  // Interleave metrics: 10 of metric 2, then 5 of metric 1
  for (uint32_t i = 0; i < 10; ++i)
  {
    expect(pc_write(&db, /*metric*/ 2, 0, 2000 + i, (float)(100 + i)) == PC_OK, "write m2");
  }
  for (uint32_t i = 0; i < 5; ++i)
  {
    expect(pc_write(&db, /*metric*/ 1, 0, 3000 + i, (float)(200 + i)) == PC_OK, "write m1 b");
  }

  // Do a couple of small flush steps to exercise multi-block behavior
  expect(pc_db_flush_once(&db) == PC_OK, "flush once");
  expect(pc_db_flush_until_empty(&db) == PC_OK, "flush rest");

  // Query both metrics
  expect(pc_query_latest(&db, 2, &v, &ts) == PC_OK, "latest m2");
  expect(ts == 2009u && v == 109.0f, "m2 values");

  expect(pc_query_latest(&db, 1, &v, &ts) == PC_OK, "latest m1");
  expect(ts == 3004u && v == 204.0f, "m1 values second batch");

  pc_db_deinit(&db);
  pc_flash_free(&f);
  puts("api: ok");
  return 0;
}
