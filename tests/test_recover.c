// PR-006 tests: forward recovery scanner
// Build & run: ctest --test-dir build --output-on-failure -R recover
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include "pc_flash.h"
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

static void write_payload_pages(pc_flash_t *f, size_t base, size_t prog, int pages, uint8_t seed)
{
  // Write 'pages' program pages with simple patterns (aligned).
  uint8_t buf[256];
  for (int p = 0; p < pages; ++p)
  {
    for (int i = 0; i < 256; ++i)
      buf[i] = (uint8_t)(seed + p + i);
    expect(pc_logseg_program_data(f, base, (size_t)p * prog, buf, prog) == PC_OK, "program page");
  }
}

int main(void)
{
  // 32KB total → 8 segments of 4KB each
  const size_t TOTAL = 32 * 1024, SEG = 4096, PROG = 256;
  pc_flash_t f = (pc_flash_t){0};
  expect(pc_flash_init(&f, TOTAL, SEG, PROG, 0xFF), "flash init");

  // We'll prepare 6 segments at bases 0..5*SEG
  // 0: valid DATA seq=1
  // 1: valid DATA seq=2
  // 2: uncommitted (no header)
  // 3: committed then corrupted (CRC fail)
  // 4: valid INDEX seq=5
  // 5: bad sector (unreadable)
  // 6,7: untouched

  // seg 0: valid
  expect(pc_logseg_erase(&f, 0 * SEG) == PC_OK, "erase 0");
  write_payload_pages(&f, 0 * SEG, PROG, 2, 0x10);
  expect(pc_logseg_commit(&f, 0 * SEG, PC_SEG_DATA, 1, 100, 199, 100) == PC_OK, "commit 0");

  // seg 1: valid
  expect(pc_logseg_erase(&f, 1 * SEG) == PC_OK, "erase 1");
  write_payload_pages(&f, 1 * SEG, PROG, 1, 0x20);
  expect(pc_logseg_commit(&f, 1 * SEG, PC_SEG_DATA, 2, 200, 299, 50) == PC_OK, "commit 1");

  // seg 2: uncommitted
  expect(pc_logseg_erase(&f, 2 * SEG) == PC_OK, "erase 2");
  write_payload_pages(&f, 2 * SEG, PROG, 1, 0x30);
  // no commit

  // seg 3: committed then corrupted (1->0 change still legal)
  expect(pc_logseg_erase(&f, 3 * SEG) == PC_OK, "erase 3");
  write_payload_pages(&f, 3 * SEG, PROG, 2, 0x40);
  expect(pc_logseg_commit(&f, 3 * SEG, PC_SEG_DATA, 4, 400, 499, 75) == PC_OK, "commit 3");
  // Tamper: clear bits in first page → CRC must fail
  uint8_t zero[256];
  memset(zero, 0x00, sizeof zero);
  expect(pc_logseg_program_data(&f, 3 * SEG, 0, zero, PROG) == PC_OK, "tamper 3");

  // seg 4: valid INDEX
  expect(pc_logseg_erase(&f, 4 * SEG) == PC_OK, "erase 4");
  write_payload_pages(&f, 4 * SEG, PROG, 3, 0x50);
  expect(pc_logseg_commit(&f, 4 * SEG, PC_SEG_INDEX, 5, 500, 599, 33) == PC_OK, "commit 4");

  // seg 5: mark bad (unreadable)
  expect(pc_flash_mark_bad(&f, 5, true) == PC_OK, "mark bad 5");

  // Run recovery
  pc_seg_summary_t got[8] = {0};
  size_t n = 0;
  expect(pc_recover_scan_all(&f, got, 8, &n) == PC_OK, "recover ok");

  // We expect: segs 0, 1, and 4 only (in address order)
  expect(n == 3, "found 3 valid segments");
  expect(got[0].base == 0 * SEG, "entry0 base");
  expect(got[0].seqno == 1 && got[0].type == PC_SEG_DATA, "entry0 fields");

  expect(got[1].base == 1 * SEG, "entry1 base");
  expect(got[1].seqno == 2 && got[1].type == PC_SEG_DATA, "entry1 fields");

  expect(got[2].base == 4 * SEG, "entry2 base");
  expect(got[2].seqno == 5 && got[2].type == PC_SEG_INDEX, "entry2 fields");

  pc_flash_free(&f);
  puts("recover: ok");
  return 0;
}
