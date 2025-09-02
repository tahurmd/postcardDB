// PR-007 tests: write one block into a segment and commit it.
// Verifies header fields and a few bytes of the payload.
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "pc_flash.h"
#include "pc_block.h"
#include "pc_logseg.h"

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
  const size_t TOTAL = 16 * 1024, SEG = 4096, PROG = 256;
  pc_flash_t f = (pc_flash_t){0};
  expect(pc_flash_init(&f, TOTAL, SEG, PROG, 0xFF), "flash init");

  // Make some simple points
  enum
  {
    N = 100
  };
  uint32_t ts[N];
  float val[N];
  for (uint32_t i = 0; i < N; ++i)
  {
    ts[i] = 1000u + i;          // strictly increasing
    val[i] = (float)(i * 0.5f); // 0.0, 0.5, 1.0, ...
  }

  // Write one block at segment base 0, seq=7
  expect(pc_block_write_segment(&f, 0, /*metric*/ 1, /*series*/ 0, ts, val, N, /*seq*/ 7) == PC_OK, "block write");

  // Verify header and CRC
  pc_segment_hdr_t hdr;
  expect(pc_logseg_verify(&f, 0, &hdr) == PC_OK, "verify ok");
  expect(hdr.type == PC_SEG_DATA, "type");
  expect(hdr.seqno == 7, "seq");
  expect(hdr.record_count == N, "rcnt");
  expect(hdr.ts_min == ts[0], "ts_min");
  expect(hdr.ts_max == ts[N - 1], "ts_max");

  // (Optional) read back first page and inspect block header + first point
  uint8_t pg[PROG];
  expect(pc_flash_read(&f, 0, pg, PROG) == PC_OK, "read first page");

  pc_block_hdr_t rd_hdr;
  memcpy(&rd_hdr, pg, sizeof(rd_hdr));
  expect(rd_hdr.metric_id == 1, "hdr metric");
  expect(rd_hdr.series_id == 0, "hdr series");
  expect(rd_hdr.start_ts == ts[0], "hdr start_ts");
  expect(rd_hdr.point_count == N, "hdr count");

  // First point directly after header
  size_t off = sizeof(pc_block_hdr_t);
  pc_point_disk_t p0;
  memcpy(&p0, pg + off, sizeof(p0));
  expect(p0.ts == ts[0], "p0 ts");
  expect(p0.value == val[0], "p0 val");

  pc_flash_free(&f);
  puts("block: ok");
  return 0;
}
