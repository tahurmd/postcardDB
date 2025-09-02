// Unit tests for the fixed-size ring buffer (SPSC, single-thread usage)
// Build & run: ctest --test-dir build --output-on-failure
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "pc_ring.h"

#define CAP 8u // must be a power of two

static void expect(bool cond, const char *msg)
{
  if (!cond)
  {
    fprintf(stderr, "FAIL: %s\n", msg);
    exit(1);
  }
}

static void test_init_empty(void)
{
  int buf[CAP];
  pc_ring_t r;
  expect(pc_ring_init(&r, buf, CAP, sizeof(int)), "init ok");
  expect(pc_ring_capacity(&r) == CAP, "capacity ok");
  expect(pc_ring_is_empty(&r), "empty after init");
  expect(!pc_ring_is_full(&r), "not full after init");
  expect(pc_ring_size(&r) == 0, "size 0");
}

static void test_push_pop_basic(void)
{
  int buf[CAP];
  pc_ring_t r;
  pc_ring_init(&r, buf, CAP, sizeof(int));

  int in[] = {1, 2, 3};
  expect(pc_ring_push(&r, in, 3) == 3, "pushed 3");
  expect(pc_ring_size(&r) == 3, "size 3 after push");

  int out[3] = {0};
  expect(pc_ring_pop(&r, out, 3) == 3, "popped 3");
  expect(out[0] == 1 && out[1] == 2 && out[2] == 3, "values round-trip");
  expect(pc_ring_is_empty(&r), "empty after pop");
}

static void test_wraparound(void)
{
  int buf[CAP];
  pc_ring_t r;
  pc_ring_init(&r, buf, CAP, sizeof(int));

  int a[] = {0, 1, 2, 3, 4};
  expect(pc_ring_push(&r, a, 5) == 5, "push 5");
  int out_a[3];
  expect(pc_ring_pop(&r, out_a, 3) == 3, "pop 3");

  // head now at 5, tail at 3 -> ring holds {3,4}; space = 8 - 2 = 6
  int b[] = {10, 11, 12, 13, 14, 15};
  expect(pc_ring_push(&r, b, 6) == 6, "push 6 to wrap");
  expect(pc_ring_is_full(&r), "ring now full");
  expect(pc_ring_size(&r) == CAP, "size == capacity");

  int all[CAP];
  expect(pc_ring_pop(&r, all, CAP) == CAP, "pop all");

  int expect_seq[] = {3, 4, 10, 11, 12, 13, 14, 15};
  for (int i = 0; i < (int)CAP; i++)
  {
    if (all[i] != expect_seq[i])
    {
      fprintf(stderr, "wrap mismatch at %d: got %d want %d\n", i, all[i], expect_seq[i]);
      exit(1);
    }
  }
  expect(pc_ring_is_empty(&r), "empty after draining");
}

static void test_peek_and_partial(void)
{
  int buf[CAP];
  pc_ring_t r;
  pc_ring_init(&r, buf, CAP, sizeof(int));

  int in[] = {7, 8, 9};
  expect(pc_ring_push(&r, in, 3) == 3, "push 3");
  const int *peek = (const int *)pc_ring_peek(&r);
  expect(peek && *peek == 7, "peek first element");

  int out[2];
  expect(pc_ring_pop(&r, out, 2) == 2, "pop 2");
  expect(out[0] == 7 && out[1] == 8, "partial pop ok");
  expect(pc_ring_size(&r) == 1, "one element left");
}

int main(void)
{
  test_init_empty();
  test_push_pop_basic();
  test_wraparound();
  test_peek_and_partial();
  puts("ring: ok");
  return 0;
}
