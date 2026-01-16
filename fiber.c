#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "context.h"
#include "queue.h"
#include "fibers.h"

void test_queue() {
  printf("QUEUE TESTS\n");
  // struct scheduler s = {};
  // struct fixed_queue queue = allocate_fixed_queue(sizeof(struct queue_node) * 3);
  struct queue queue = {};

  assert(queue.head == nullptr);
  assert(queue.tail == nullptr);
  // assert(s.run_queue.count == 0);

  int* first = malloc(sizeof(int));
  assert(first != nullptr);
  
  queue_push(&queue, first);
  
  assert(queue.head != nullptr);
  assert(queue.tail != nullptr);
  assert(queue.tail == queue.head);
  
  int* second = malloc(sizeof(int));
  assert(second != nullptr);

  queue_push(&queue, second);
  assert(queue.head != nullptr);
  assert(queue.tail != nullptr);
  assert(queue.tail != queue.head);
  assert(queue.head->next == queue.tail);
  assert(queue.tail->prev == queue.head);
  // assert(s.run_queue.count == 2);

  int* third = malloc(sizeof(int));
  assert(third != nullptr);

  queue_push(&queue, third);
  assert(queue.head != nullptr);
  assert(queue.tail != nullptr);
  assert(queue.tail != queue.head);
  assert(queue.head->next != queue.tail);
  assert(queue.tail->prev != queue.head);
  assert(queue.head->next == queue.tail->prev);
  // assert(s.run_queue.count == 3);

  int idx = 0;
  for(struct queue_node* node = queue.head; node != nullptr; node = node->next) {
    if (idx == 0) {
      assert((int*)node->payload == first);
    }
    else if (idx == 1) {
      assert((int*)node->payload == second);
    }
    else if (idx == 2) {
      assert((int*)node->payload == third);
    }

    ++idx;
  }

  payload_t* popped = queue_pop(&queue);
  // assert(s.run_queue.count == 2);
  assert(queue.head != nullptr);
  assert(queue.tail != nullptr);
  assert((int*)queue.head->payload != first);
  assert((int*)popped == first);
  assert(queue.tail != queue.head);
  assert(queue.head->next == queue.tail);
  assert(queue.tail->prev == queue.head);
  
  payload_t* popped2 = queue_pop(&queue);
  assert(queue.head != nullptr);
  assert(queue.tail != nullptr);
  assert((int*)queue.head->payload != second);
  assert((int*)popped2 == second);
  assert(queue.tail == queue.head);

  payload_t* popped3 = queue_pop(&queue);
  assert(queue.head == nullptr);
  assert(queue.tail == nullptr);

  free(popped3);
  free(popped2);
  free(popped);
  printf("ALL PASSED\n");
}

void bar1(struct execution_context* ctx) {
  for (int i = 0; i < 10; ++i) {
    thrd_sleep(&(struct timespec){.tv_nsec = 500000000 }, NULL);
    printf("bar1\n");
    yield(ctx);
  }
}

void bar2(struct execution_context* ctx) {
  for (int i = 0; i < 10; ++i) {
    thrd_sleep(&(struct timespec){.tv_nsec = 500000000 }, NULL);
    printf("bar2\n");
    yield(ctx);
  }
}

void bar3(struct execution_context* ctx) {
  for (int i = 0; i < 10; ++i) {
    thrd_sleep(&(struct timespec){.tv_nsec = 500000000 }, NULL);
    printf("bar3\n");
    yield(ctx);
  }
}

void foo(struct execution_context* ctx) {
  printf("foo\n");

  start(bar1);
  start(bar2);
  start(bar3);
  
  printf("foo finish\n");
}

int main() {
  struct runtime* rt = allocate_runtime(foo);

  runtime_start(rt);

  thrd_sleep(&(struct timespec){.tv_sec = 1 }, NULL);

  printf("main\n");

  runtime_stop(rt);

  printf("final\n");

  free_runtime(rt);

  return 0;
}

