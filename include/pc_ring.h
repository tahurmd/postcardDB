// Fixed-size SPSC ring buffer (now thread-safe across two threads).
// - POWER-OF-TWO capacity (2^k) for fast wrap with a bitmask.
// - No mallocs: you provide the backing array.
// - Concurrency model: exactly one producer thread calls push(), one
//   consumer thread calls pop(). That's it. (SPSC)
//
// Memory ordering (C11 atomics):
//   Producer push():
//     - Reads consumer 'tail' with acquire
//     - Writes elements into buffer
//     - Publishes new 'head' with release
//   Consumer pop():
//     - Reads producer 'head' with acquire
//     - Reads elements from buffer
//     - Publishes new 'tail' with release
//
// Invariants:
//   size = head - tail    (modulo uint32 arithmetic, bounded by capacity)
//   empty when head == tail
//   full  when size == capacity
//
// Notes:
//   - peek() is advisory: in SPSC use it immediately from the consumer only.
//   - clear() is only safe when both threads are stopped.

#ifndef PC_RING_H
#define PC_RING_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdatomic.h> // C11 atomics

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct
  {
    uint8_t *buf;          // raw storage for elements
    uint32_t elem_size;    // bytes per element (>=1)
    uint32_t capacity;     // number of elements (power of two)
    uint32_t mask;         // capacity - 1
    _Atomic uint32_t head; // next write index  (producer owns writes)
    _Atomic uint32_t tail; // next read index   (consumer owns writes)
  } pc_ring_t;

  // True if x is power-of-two (and not zero).
  static inline bool pc_is_pow2_u32(uint32_t x) { return x && ((x & (x - 1u)) == 0u); }

  // Initialize with caller-provided storage of size == elem_size * capacity.
  // Returns false if arguments are invalid.
  bool pc_ring_init(pc_ring_t *r, void *buffer, uint32_t capacity_elems, uint32_t elem_size);

  // Introspection
  static inline uint32_t pc_ring_capacity(const pc_ring_t *r) { return r->capacity; }

  // NOTE: size() reads both counters; in SPSC it’s fine to call from either side.
  static inline uint32_t pc_ring_size(const pc_ring_t *r)
  {
    uint32_t h = atomic_load_explicit(&r->head, memory_order_acquire);
    uint32_t t = atomic_load_explicit(&r->tail, memory_order_acquire);
    return h - t;
  }

  static inline bool pc_ring_is_empty(const pc_ring_t *r)
  {
    uint32_t h = atomic_load_explicit(&r->head, memory_order_acquire);
    uint32_t t = atomic_load_explicit(&r->tail, memory_order_acquire);
    return h == t;
  }

  static inline bool pc_ring_is_full(const pc_ring_t *r)
  {
    return pc_ring_size(r) == r->capacity;
  }

  // Optional: how full (0.0 .. 1.0)
  static inline float pc_ring_load_factor(const pc_ring_t *r)
  {
    return (float)pc_ring_size(r) / (float)r->capacity;
  }

  // Reset indices (ONLY when both threads are quiesced).
  static inline void pc_ring_clear(pc_ring_t *r)
  {
    atomic_store_explicit(&r->head, 0u, memory_order_relaxed);
    atomic_store_explicit(&r->tail, 0u, memory_order_relaxed);
  }

  // Producer API: push up to 'count' elements from 'elems'.
  // Returns number actually pushed (0..count).
  uint32_t pc_ring_push(pc_ring_t *r, const void *elems, uint32_t count);

  // Consumer API: pop up to 'max_count' into 'out_elems'.
  // Returns number actually popped (0..max_count).
  uint32_t pc_ring_pop(pc_ring_t *r, void *out_elems, uint32_t max_count);

  // Consumer convenience: peek pointer to first element (NULL if empty).
  // Valid until the slot is popped/overwritten.
  const void *pc_ring_peek(const pc_ring_t *r);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // PC_RING_H
