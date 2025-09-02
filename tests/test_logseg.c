// PR-005 tests: segment commit (header-last), verify, "crash-before-commit",
// with extra diagnostics so we can see exactly where/why it fails.
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include "pc_flash.h"
#include "pc_logseg.h"
#include "pc_crc32c.h" // for a direct CRC check if needed

static void die(const char *msg)
{
  fprintf(stderr, "FAIL: %s\n", msg);
  exit(1);
}
static void expect(int cond, const char *msg)
{
  if (!cond)
    die(msg);
}

int main(void)
{
  const size_t TOTAL = 16 * 1024, SEG = 4096, PROG = 256;
  pc_flash_t f = (pc_flash_t){0};
  expect(pc_flash_init(&f, TOTAL, SEG, PROG, 0xFF), "flash init");

  const size_t base = 0;
  expect(pc_logseg_erase(&f, base) == PC_OK, "erase seg");

  // Write 3 program pages in the pre-header area.
  uint8_t page[256];
  for (int i = 0; i < 256; ++i)
    page[i] = (uint8_t)i;
  expect(pc_logseg_program_data(&f, base, 0 * PROG, page, PROG) == PC_OK, "prog page0");
  for (int i = 0; i < 256; ++i)
    page[i] = (uint8_t)(i ^ 0x55);
  expect(pc_logseg_program_data(&f, base, 1 * PROG, page, PROG) == PC_OK, "prog page1");
  for (int i = 0; i < 256; ++i)
    page[i] = (uint8_t)(i ^ 0xAA);
  expect(pc_logseg_program_data(&f, base, 2 * PROG, page, PROG) == PC_OK, "prog page2");

  // Pre-commit state
  expect(pc_logseg_header_erased(&f, base) == true, "header erased before commit");
  {
    pc_result_t pre = pc_logseg_verify(&f, base, NULL);
    if (pre != PC_CORRUPT)
    {
      fprintf(stderr, "diag: pre-commit verify returned %d, expected PC_CORRUPT\n", pre);
      return 2;
    }
  }

  // Commit header
  const uint16_t type = PC_SEG_DATA;
  const uint32_t seq = 42;
  const uint32_t tmin = 1000;
  const uint32_t tmax = 2000;
  const uint32_t rcnt = 123;
  {
    pc_result_t cr = pc_logseg_commit(&f, base, type, seq, tmin, tmax, rcnt);
    if (cr != PC_OK)
    {
      fprintf(stderr, "diag: commit returned %d (not PC_OK)\n", cr);
      return 3;
    }
  }

  // Verify after commit
  pc_segment_hdr_t hdr;
  pc_result_t vr = pc_logseg_verify(&f, base, &hdr);
  if (vr != PC_OK)
  {
    fprintf(stderr, "diag: verify after commit returned %d (not PC_OK)\n", vr);

    // Extra CRC print to see mismatch if any
    uint32_t crc = 0;
    pc_result_t cr = pc_logseg_crc32c_region(&f, base, &crc);
    fprintf(stderr, "diag: crc32c_region rc=%d, crc=0x%08X\n", cr, crc);

    // Dump first 16 bytes of header page
    const size_t preH = pc_logseg_preheader_bytes(&f);
    uint8_t hdrpg[256];
    if (pc_flash_read(&f, base + preH, hdrpg, PROG) == PC_OK)
    {
      fprintf(stderr, "diag: header bytes[0..15]:");
      for (int i = 0; i < 16; ++i)
        fprintf(stderr, " %02X", hdrpg[i]);
      fprintf(stderr, "\n");
    }
    return 4;
  }

  // Header field checks (with prints)
  fprintf(stderr, "diag: hdr.magic=0x%08X version=%u type=%u seq=%u ts_min=%u ts_max=%u rcnt=%u crc=0x%08X\n",
          hdr.magic, hdr.version, hdr.type, hdr.seqno, hdr.ts_min, hdr.ts_max, hdr.record_count, hdr.crc32c);

  expect(hdr.magic == PC_SEG_MAGIC, "magic ok");
  expect(hdr.version == PC_SEG_VERSION, "version ok");
  expect(hdr.type == type, "type ok");
  expect(hdr.seqno == seq, "seq ok");
  expect(hdr.ts_min == tmin, "ts_min ok");
  expect(hdr.ts_max == tmax, "ts_max ok");
  expect(hdr.record_count == rcnt, "rcnt ok");

  // Tamper payload page 0 (legal 1->0 change) then verify should fail CRC.
  uint8_t bad[256];
  memset(bad, 0x00, sizeof bad);
  expect(pc_logseg_program_data(&f, base, 0 * PROG, bad, PROG) == PC_OK, "tamper ok");
  {
    pc_result_t v2 = pc_logseg_verify(&f, base, NULL);
    if (v2 != PC_CORRUPT)
    {
      fprintf(stderr, "diag: verify after tamper returned %d (expected PC_CORRUPT)\n", v2);
      return 5;
    }
  }

  pc_flash_free(&f);
  puts("logseg: ok");
  return 0;
}
