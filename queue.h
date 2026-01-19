#ifndef QUEUE_H
#define QUEUE_H

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

typedef void payload_t;

struct queue_node {
  struct payload* payload;
  struct queue_node* next;
  struct queue_node* prev;
};

struct queue {
  struct queue_node* head;
  struct queue_node* tail;
};

void queue_push(struct queue* queue, payload_t* e) {
  assert(queue != nullptr);
  assert(e != nullptr);

  struct queue_node* node = calloc(1, sizeof(struct queue_node));
  if (node == nullptr) {
    perror("queue push malloc error");
    exit(EXIT_FAILURE);
  }

  node->payload = e;

  if (queue->tail == nullptr) {
    queue->head = node;
    queue->tail = node;

    return;
  }

  queue->tail->next = node;
  node->prev = queue->tail;
  queue->tail = node;
}

[[nodiscard]]
payload_t* queue_pop(struct queue* queue) {
  assert(queue != nullptr);

  if (queue->head == nullptr) {
    return nullptr;
  }

  struct queue_node* popped = queue->head;
  if (popped == nullptr) {
    return nullptr;
  }

  if (popped->next == nullptr) {
    queue->head = nullptr;
    queue->tail = nullptr;
  } else {
    queue->head = popped->next;
  }

  payload_t* res = popped->payload;
  free(popped);

  return res;
}

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

#endif // ifndef QUEUE_H
