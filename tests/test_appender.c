// PR-008 tests: append multiple blocks into a segment and commit.
// Verifies header fields and total record_count.
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "pc_flash.h"
#include "pc_appender.h"
#include "pc_logseg.h"
#include "pc_recover.h"

static void expect(int cond, const char *msg)
{
  if (!cond)
  {
    fprintf(stderr, "FAIL: %s\n", msg);
    exit(1);
  }
}

// Fill arrays with simple data
static void fill_points(uint32_t *ts, float *v, uint32_t n, uint32_t start_ts)
{
  for (uint32_t i = 0; i < n; ++i)
  {
    ts[i] = start_ts + i;
    v[i] = (float)(i);
  }
}

int main(void)
{
  const size_t TOTAL = 16 * 1024, SEG = 4096, PROG = 256;
  pc_flash_t f = (pc_flash_t){0};
  expect(pc_flash_init(&f, TOTAL, SEG, PROG, 0xFF), "flash init");

  pc_appender_t a;
  expect(pc_appender_open(&a, &f, 0, /*seq*/ 101) == PC_OK, "open");

  // First block: 60 points
  uint32_t ts1[60];
  float v1[60];
  fill_points(ts1, v1, 60, 1000);
  expect(pc_appender_append_block(&a, /*metric*/ 1, /*series*/ 0, ts1, v1, 60) == PC_OK, "append blk1");

  // Second block: 70 points
  uint32_t ts2[70];
  float v2[70];
  fill_points(ts2, v2, 70, 2000);
  expect(pc_appender_append_block(&a, /*metric*/ 2, /*series*/ 0, ts2, v2, 70) == PC_OK, "append blk2");

  // There should still be space left (we didn't fill pre-header)
  size_t rem = pc_appender_bytes_remaining(&a);
  expect(rem > 0, "bytes remaining > 0");

  // Commit
  expect(pc_appender_commit(&a, PC_SEG_DATA) == PC_OK, "commit ok");
  expect(!pc_appender_is_open(&a), "closed after commit");

  // Verify header content
  pc_segment_hdr_t hdr;
  expect(pc_logseg_verify(&f, 0, &hdr) == PC_OK, "verify ok");
  expect(hdr.seqno == 101, "seq ok");
  expect(hdr.type == PC_SEG_DATA, "type ok");
  expect(hdr.record_count == (60u + 70u), "count ok");
  expect(hdr.ts_min == 1000u, "ts_min ok");
  expect(hdr.ts_max == (2000u + 70u - 1u), "ts_max ok");

  // Recovery should see exactly one valid segment
  pc_seg_summary_t out[4];
  size_t n = 0;
  expect(pc_recover_scan_all(&f, out, 4, &n) == PC_OK, "recover ok");
  expect(n == 1, "one segment");
  expect(out[0].seqno == 101 && out[0].record_count == 130, "summary ok");

  pc_flash_free(&f);
  puts("appender: ok");
  return 0;
}
