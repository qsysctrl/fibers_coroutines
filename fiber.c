#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>

#include "context.h"
#include "queue.h"
#include "fibers.h"

void nothing(fiber_t* f) {
  (void)f;
  return;
}

void bar1(fiber_t* f) {
  printf("bar1 running on thread %zu\n", thrd_current());
  for (int i = 0; i < 10; ++i) {
    thrd_sleep(&(struct timespec){.tv_nsec = 500000000 }, NULL);
    printf("bar1\n");
    yield(f);
  }
  printf("bar1 complete\n");
}

void bar2(fiber_t* f) {
  printf("bar2 running on thread %zu\n", thrd_current());
  for (int i = 0; i < 10; ++i) {
    thrd_sleep(&(struct timespec){.tv_nsec = 500000000 }, NULL);
    printf("bar2\n");
    yield(f);
  }
  printf("bar2 complete\n");
}

void bar3(fiber_t* f) {
  printf("bar3 running on thread %zu\n", thrd_current());
  for (int i = 0; i < 10; ++i) {
    thrd_sleep(&(struct timespec){.tv_nsec = 500000000 }, NULL);
    printf("bar3\n");
    yield(f);
  }
  printf("bar3 complete\n");
}

void foo(struct execution_context* ctx) {
  printf("foo running on %zu\n", thrd_current());

  start(nothing);
  start(nothing);

  start(bar1);
  yield(ctx);

  start(bar2);
  yield(ctx);

  start(bar3);
  yield(ctx);

  printf("foo finish\n");
}

int main() {
  struct runtime* rt = allocate_runtime(foo);

  runtime_start(rt);

  thrd_sleep(&(struct timespec){.tv_sec = 2 }, NULL);

  printf("main\n");

  thrd_sleep(&(struct timespec){.tv_sec = 2 }, NULL);

  printf("stoping runtime:\n");
  runtime_graceful_stop(rt);

  printf("final\n");

  free_runtime(rt);

  return 0;
}
