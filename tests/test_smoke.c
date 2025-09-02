// Smoke test: proves the project compiles, links, and runs.
// Run with: ctest --test-dir build --output-on-failure
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "pc_result.h"
#include "pc_log.h"
#include "pc_crc32c.h"

int main(void)
{
  // Check the result-code helper returns names we expect.
  if (strcmp(pc_result_str(PC_OK), "PC_OK") != 0)
  {
    fprintf(stderr, "pc_result_str(PC_OK) mismatch\n");
    return 1;
  }
  if (strcmp(pc_result_str(PC_INVALID_RANGE), "PC_INVALID_RANGE") != 0)
  {
    fprintf(stderr, "pc_result_str(PC_INVALID_RANGE) mismatch\n");
    return 1;
  }

  // Exercise the log macros (visible on stderr).
  LOG_INFO("smoke: starting");
  LOG_DEBUG("smoke: debug message (hidden unless PC_LOG_LEVEL>=3)");
  LOG_WARN("smoke: warning example");
  LOG_ERROR("smoke: error example");

  // Call the CRC32C stub just to ensure linkage.
  uint8_t buf[4] = {1, 2, 3, 4};
  (void)pc_crc32c_update(0, buf, sizeof buf);

  printf("smoke: ok\n");
  return 0;
}
