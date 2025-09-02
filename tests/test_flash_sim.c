// PR-004 tests: geometry, erase/program rules, bad sectors, wear stats
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "pc_flash.h"

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
  pc_flash_t f = {0};

  // Tiny device for testing: 16KB total, 4KB sectors, 256B program
  const size_t TOTAL = 16 * 1024, SECTOR = 4096, PROG = 256;
  expect(pc_flash_init(&f, TOTAL, SECTOR, PROG, 0xFF), "init");

  expect(pc_flash_total(&f) == TOTAL, "total");
  expect(pc_flash_sector_bytes(&f) == SECTOR, "sector_bytes");
  expect(pc_flash_prog_bytes(&f) == PROG, "prog_bytes");
  expect(pc_flash_sector_count(&f) == (TOTAL / SECTOR), "sector_count");

  // Erase sector 0 (already erased, but tests wear++)
  expect(pc_flash_erase_sector(&f, 0) == PC_OK, "erase sector 0");
  uint32_t mn = 0, mx = 0, avg = 0;
  pc_flash_wear_stats(&f, &mn, &mx, &avg);
  expect(mx >= 1, "wear bumped");

  // Program must be aligned and only 1->0
  uint8_t page[PROG];
  memset(page, 0xFF, sizeof page); // all ones
  page[0] = 0xF0;                  // lower some bits

  // OK write at aligned address
  expect(pc_flash_program(&f, 0, page, PROG) == PC_OK, "program aligned");

  // Verify readback
  uint8_t rb[PROG];
  expect(pc_flash_read(&f, 0, rb, PROG) == PC_OK, "readback");
  expect(rb[0] == 0xF0, "value programmed");
  expect(rb[1] == 0xFF, "untouched byte stays FF");

  // Attempt to set a 0->1 should fail
  uint8_t up[PROG];
  memset(up, 0xFF, sizeof up);
  up[0] = 0xFF; // would try to raise bits back to 1
  expect(pc_flash_program(&f, 0, up, PROG) == PC_EINVAL, "0->1 forbidden");

  // Alignment errors
  expect(pc_flash_program(&f, 1, page, PROG) == PC_EINVAL, "addr align");
  expect(pc_flash_program(&f, 0, page, PROG - 1) == PC_EINVAL, "len align");

  // Bad sector behavior: mark sector 1 bad, then any access that touches it fails
  expect(pc_flash_mark_bad(&f, 1, true) == PC_OK, "mark bad");
  expect(pc_flash_is_bad(&f, 1) == true, "is bad");
  // Program into sector 1 -> error
  expect(pc_flash_program(&f, SECTOR, page, PROG) == PC_FLASH_IO, "program bad");
  // Erase bad sector -> error
  expect(pc_flash_erase_sector(&f, 1) == PC_FLASH_IO, "erase bad");
  // Read that spans into bad sector -> error
  uint8_t buf[SECTOR]; // size not used fully
  expect(pc_flash_read(&f, SECTOR, buf, PROG) == PC_FLASH_IO, "read bad");

  pc_flash_free(&f);
  puts("flash_sim: ok");
  return 0;
}
