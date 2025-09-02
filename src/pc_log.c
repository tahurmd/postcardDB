#include "pc_log.h"
#include <stdio.h>
#include <stdarg.h>

#ifndef PC_LOG_LEVEL
#define PC_LOG_LEVEL 2
#endif

void pc_log_log(int level, const char *file, int line, const char *fmt, ...)
{
  if (level > PC_LOG_LEVEL)
    return;

  const char *tag = (level == 3)   ? "DBG"
                    : (level == 2) ? "INF"
                    : (level == 1) ? "WRN"
                                   : "ERR";

  // Prefix
  fprintf(stderr, "[PC][%s] %s:%d: ", tag, file, line);

  // Body (format string + variable args)
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);

  // Newline
  fputc('\n', stderr);
}
