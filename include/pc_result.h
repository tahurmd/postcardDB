
// Kept small and readable; includes a helper to print names in logs/tests.
#ifndef PC_RESULT_H
#define PC_RESULT_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PC_OK = 0,            // Success
    PC_BUSY,              // System busy (e.g., ring high-water)
    PC_RETRY,             // Transient condition; try again
    PC_NO_SPACE,          // Out of space (retention/GC required)
    PC_METRIC_UNKNOWN,    // Metric not found
    PC_TOO_MANY_SERIES,   // Series dictionary is full
    PC_INVALID_RANGE,     // Bad time range or arguments
    PC_CORRUPT,           // Data/format/CRC invalid
    PC_FLASH_IO,          // Flash I/O error
    PC_FLASH_WEAR,        // Wear threshold exceeded
    PC_EINVAL,            // Invalid argument
    PC_UNSUPPORTED,       // Feature not compiled in
    PC_ITER_END           // Iterator exhausted
} pc_result_t;

// Human-readable name for logs & tests.
static inline const char* pc_result_str(pc_result_t r) {
    switch (r) {
        case PC_OK:              return "PC_OK";
        case PC_BUSY:            return "PC_BUSY";
        case PC_RETRY:           return "PC_RETRY";
        case PC_NO_SPACE:        return "PC_NO_SPACE";
        case PC_METRIC_UNKNOWN:  return "PC_METRIC_UNKNOWN";
        case PC_TOO_MANY_SERIES: return "PC_TOO_MANY_SERIES";
        case PC_INVALID_RANGE:   return "PC_INVALID_RANGE";
        case PC_CORRUPT:         return "PC_CORRUPT";
        case PC_FLASH_IO:        return "PC_FLASH_IO";
        case PC_FLASH_WEAR:      return "PC_FLASH_WEAR";
        case PC_EINVAL:          return "PC_EINVAL";
        case PC_UNSUPPORTED:     return "PC_UNSUPPORTED";
        case PC_ITER_END:        return "PC_ITER_END";
        default:                 return "PC_UNKNOWN";
    }
}

#ifdef __cplusplus
} // extern "C"
#endif

#endif // PC_RESULT_H
