#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "context.h"
#include "fibers_N_1.h"


void test_queue() {
  struct scheduler s = {};

  assert(s.run_queue.head == nullptr);
  assert(s.run_queue.tail == nullptr);
  assert(s.run_queue.count == 0);

  struct execution_context* ctx = malloc(sizeof(struct execution_context));
  assert(ctx != nullptr);
  
  queue_push(&s.run_queue, ctx);
  
  assert(s.run_queue.head != nullptr);
  assert(s.run_queue.tail != nullptr);
  assert(s.run_queue.tail == s.run_queue.head);
  
  struct execution_context* ctx2 = malloc(sizeof(struct execution_context));
  assert(ctx2 != nullptr);
  queue_push(&s.run_queue, ctx2);
  assert(s.run_queue.head != nullptr);
  assert(s.run_queue.tail != nullptr);
  assert(s.run_queue.tail != s.run_queue.head);
  assert(s.run_queue.head->next == s.run_queue.tail);
  assert(s.run_queue.tail->prev == s.run_queue.head);
  assert(s.run_queue.count == 2);

  struct execution_context* ctx3 = malloc(sizeof(struct execution_context));
  assert(ctx3 != nullptr);
  queue_push(&s.run_queue, ctx3);
  assert(s.run_queue.head != nullptr);
  assert(s.run_queue.tail != nullptr);
  assert(s.run_queue.tail != s.run_queue.head);
  assert(s.run_queue.head->next != s.run_queue.tail);
  assert(s.run_queue.tail->prev != s.run_queue.head);
  assert(s.run_queue.head->next == s.run_queue.tail->prev);
  assert(s.run_queue.count == 3);

  int idx = 0;
  for(struct queue_node* node = s.run_queue.head; node != nullptr; node = node->next) {
    if (idx == 0) {
      assert(node->fiber == ctx);
    }
    else if (idx == 1) {
      assert(node->fiber == ctx2);
    }
    else if (idx == 2) {
      assert(node->fiber == ctx3);
    }

    ++idx;
  }

  struct execution_context* popped = queue_pop(&s.run_queue);
  assert(s.run_queue.count == 2);
  assert(s.run_queue.head != nullptr);
  assert(s.run_queue.tail != nullptr);
  assert(s.run_queue.head->fiber != ctx);
  assert(popped == ctx);
  assert(s.run_queue.tail != s.run_queue.head);
  assert(s.run_queue.head->next == s.run_queue.tail);
  assert(s.run_queue.tail->prev == s.run_queue.head);
}

int main() {
  fibers_N_1_example();

  return 0;
}

