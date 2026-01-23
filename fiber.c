#include <assert.h>
#include <pthread.h>
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

struct q_logger *g_logger = nullptr;

void bar1(fiber_t* f) {
  printf("[USER CODE] bar1 running on thread %zu\n", pthread_self());

  for (int i = 0; i < 5; ++i) {
    sleep_for((struct timespec){.tv_nsec = 500000000 });
    printf("-- bar1 step %d\n", i);
    yield();
  }

  printf("[USER CODE] bar1 completed\n");
}

void bar2(fiber_t* f) {
  printf("[USER CODE] bar2 running on thread %zu\n", pthread_self());

  for (int i = 0; i < 10; ++i) {
    sleep_for((struct timespec){.tv_nsec = 500000000 });
    printf("-- bar2 step %d\n", i);
    yield();
  }

  printf("[USER CODE] bar2 completed\n");
}

void bar3(fiber_t* f) {
  printf("[USER CODE] bar3 running on thread %zu\n", pthread_self());

  for (int i = 0; i < 10; ++i) {
    sleep_for((struct timespec){.tv_nsec = 500000000 });
    printf("-- bar3 step %d\n", i);
    yield();
  }

  printf("[USER CODE] bar3 completed\n");
}

void foo(struct execution_context* ctx) {
  printf("[USER CODE] foo running on thread %zu\n", pthread_self());

  go(bar1);

  go(bar2);

  go(bar3);

  printf("[USER CODE] foo completed\n");
}

void test_runtime() {
  struct runtime* rt = allocate_runtime(foo);
  assert(rt != nullptr);

  runtime_run(rt);

  printf("final\n");

  free_runtime(rt);
}

int main() {
  // test_queue();

  test_runtime();

  return 0;
}
