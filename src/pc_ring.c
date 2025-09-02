#include "pc_ring.h"
#include <string.h> // memcpy

// Internal helper: address of the element at logical 'index' (with wrap).
static inline uint8_t *slot_ptr(const pc_ring_t *r, uint32_t index)
{
  // Wrap with mask (fast because capacity is power of two)
  uint32_t pos = (index & r->mask) * r->elem_size;
  return r->buf + pos;
}

bool pc_ring_init(pc_ring_t *r, void *buffer, uint32_t capacity_elems, uint32_t elem_size)
{
  if (!r || !buffer || elem_size == 0 || !pc_is_pow2_u32(capacity_elems))
  {
    return false;
  }
  r->buf = (uint8_t *)buffer;
  r->elem_size = elem_size;
  r->capacity = capacity_elems;
  r->mask = capacity_elems - 1u;
  r->head = 0u;
  r->tail = 0u;
  return true;
}

uint32_t pc_ring_push(pc_ring_t *r, const void *elems, uint32_t count)
{
  if (!r || !elems || count == 0)
    return 0u;

  // How many free slots?
  uint32_t used = r->head - r->tail;
  uint32_t space = r->capacity - used;
  if (space == 0)
    return 0u;

  if (count > space)
    count = space;

  // First chunk: from current head to end of buffer (or until we filled 'count')
  uint32_t head_idx = r->head & r->mask;
  uint32_t first_space = r->capacity - head_idx;
  uint32_t first = (count < first_space) ? count : first_space;

  if (first)
  {
    memcpy(slot_ptr(r, r->head), elems, first * r->elem_size);
  }

  // Second chunk: if we wrapped, copy the remainder starting at index 0
  uint32_t second = count - first;
  if (second)
  {
    memcpy(slot_ptr(r, r->head + first),
           (const uint8_t *)elems + first * r->elem_size,
           second * r->elem_size);
  }

  r->head += count;
  return count;
}

uint32_t pc_ring_pop(pc_ring_t *r, void *out_elems, uint32_t max_count)
{
  if (!r || !out_elems || max_count == 0)
    return 0u;

  // How many elements are available?
  uint32_t avail = r->head - r->tail;
  if (avail == 0)
    return 0u;

  if (max_count > avail)
    max_count = avail;

  // First chunk: from current tail to end of buffer (or until we read 'max_count')
  uint32_t tail_idx = r->tail & r->mask;
  uint32_t first_avail = r->capacity - tail_idx;
  uint32_t first = (max_count < first_avail) ? max_count : first_avail;

  if (first)
  {
    memcpy(out_elems, slot_ptr(r, r->tail), first * r->elem_size);
  }

  // Second chunk: if we wrapped, copy the remainder from index 0
  uint32_t second = max_count - first;
  if (second)
  {
    memcpy((uint8_t *)out_elems + first * r->elem_size,
           slot_ptr(r, r->tail + first),
           second * r->elem_size);
  }

  r->tail += max_count;
  return max_count;
}

const void *pc_ring_peek(const pc_ring_t *r)
{
  if (!r || r->head == r->tail)
    return NULL;
  return (const void *)slot_ptr(r, r->tail);
}
