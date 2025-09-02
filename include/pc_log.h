// Portable logging (no GNU extensions). Works with or without extra args.
// Example: LOG_INFO("hello");  LOG_DEBUG("x=%d", x);
#ifndef PC_LOG_H
#define PC_LOG_H

#include <stdarg.h>

#ifdef __cplusplus
extern "C"
{
#endif

// 0=ERROR, 1=WARN, 2=INFO, 3=DEBUG (change at compile time if needed)
#ifndef PC_LOG_LEVEL
#define PC_LOG_LEVEL 2
#endif

  // Single implementation function used by the macros below.
  void pc_log_log(int level, const char *file, int line, const char *fmt, ...);

#define LOG_ERROR(...) pc_log_log(0, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_WARN(...) pc_log_log(1, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_INFO(...) pc_log_log(2, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_DEBUG(...) pc_log_log(3, __FILE__, __LINE__, __VA_ARGS__)

#ifdef __cplusplus
} // extern "C"
#endif

#endif // PC_LOG_H
