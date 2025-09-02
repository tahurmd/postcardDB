// Threaded stress test for SPSC ring (PR-002).
// Verifies order & count across two threads using C11 atomics + pthreads.

#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h> // nanosleep

#include "pc_ring.h"

typedef struct
{
  uint32_t seq;
} item_t;

enum
{
  CAP = 1024,
  TOTAL = 100000,
  BATCH = 32
};

static item_t storage[CAP];
static pc_ring_t ring;
static atomic_uint produced;
static atomic_uint consumed;
static atomic_uint errors;

static void backoff(void)
{
  struct timespec ts = {0, 1000000}; // 1 ms
  nanosleep(&ts, NULL);
}

static void *producer_thread(void *_)
{
  (void)_;
  uint32_t p = 0;
  item_t tmp[BATCH];

  while (p < TOTAL)
  {
    // Prepare a small batch with sequential seq numbers.
    uint32_t want = BATCH;
    if (want > TOTAL - p)
      want = TOTAL - p;
    for (uint32_t i = 0; i < want; ++i)
      tmp[i].seq = p + i;

    uint32_t pushed = pc_ring_push(&ring, tmp, want);
    if (pushed == 0)
    {
      backoff();
      continue;
    }

    p += pushed;
    atomic_fetch_add_explicit(&produced, pushed, memory_order_relaxed);
  }
  return NULL;
}

static void *consumer_thread(void *_)
{
  (void)_;
  uint32_t c = 0;
  item_t tmp[BATCH];
  uint32_t expect_next = 0;

  while (c < TOTAL)
  {
    uint32_t got = pc_ring_pop(&ring, tmp, BATCH);
    if (got == 0)
    {
      backoff();
      continue;
    }

    // Check sequence order
    for (uint32_t i = 0; i < got; ++i)
    {
      if (tmp[i].seq != expect_next)
      {
        fprintf(stderr, "order error at %u: got %u, want %u\n",
                c + i, tmp[i].seq, expect_next);
        atomic_fetch_add_explicit(&errors, 1u, memory_order_relaxed);
        expect_next = tmp[i].seq + 1; // resync
      }
      else
      {
        expect_next++;
      }
    }
    c += got;
    atomic_fetch_add_explicit(&consumed, got, memory_order_relaxed);
  }
  return NULL;
}

int main(void)
{
  // Init ring
  if (!pc_ring_init(&ring, storage, CAP, sizeof(item_t)))
  {
    fprintf(stderr, "ring init failed\n");
    return 1;
  }
  atomic_store(&produced, 0u);
  atomic_store(&consumed, 0u);
  atomic_store(&errors, 0u);

  // Launch threads
  pthread_t prod, cons;
  if (pthread_create(&prod, NULL, producer_thread, NULL) != 0)
    return 2;
  if (pthread_create(&cons, NULL, consumer_thread, NULL) != 0)
    return 3;

  pthread_join(prod, NULL);
  pthread_join(cons, NULL);

  unsigned p = atomic_load(&produced);
  unsigned c = atomic_load(&consumed);
  unsigned e = atomic_load(&errors);

  if (p != TOTAL || c != TOTAL)
  {
    fprintf(stderr, "count mismatch: produced=%u consumed=%u\n", p, c);
    return 4;
  }
  if (e != 0)
  {
    fprintf(stderr, "ordering errors: %u\n", e);
    return 5;
  }

  puts("ring_threads: ok");
  return 0;
}
