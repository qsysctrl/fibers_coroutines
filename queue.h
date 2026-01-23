#ifndef QUEUE_H
#define QUEUE_H

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

typedef void payload_t;

struct queue_node {
  payload_t* payload;
  struct queue_node* next;
  struct queue_node* prev;
};

struct queue {
  struct queue_node* head;
  struct queue_node* tail;
  size_t count;
};

void queue_push(struct queue* queue, payload_t* e) {
  assert(queue != nullptr);
  if (e == nullptr) {
    fprintf(stderr, "nullptr payload pushed\n");
  }

  struct queue_node* node = calloc(1, sizeof(struct queue_node));
  if (node == nullptr) {
    perror("queue push malloc error");
    exit(EXIT_FAILURE);
  }

  queue->count += 1;
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

  if (queue->count > 0) {
    queue->count -= 1;
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

// Returns queue, which head points to start of batch and tale points to end
// Dequeues up to `n` elements
[[nodiscard]]
struct queue queue_batch_pop(struct queue* q, size_t n) {
  if (q == nullptr || q->head == nullptr || n == 0) {
    return (struct queue) {};
  }

  struct queue result = {
    .head = q->head,
  };

  struct queue_node* tail = q->head;
  size_t i = 0;
  for (; i < n - 1 && tail->next != nullptr; ++i) {
    tail = tail->next;
  }

  result.count = i + 1;
  result.tail = tail;

  q->count -= result.count;

  q->head = tail->next;
  if (tail->next != nullptr) {
    tail->next->prev = nullptr;
    tail->next = nullptr;
  }
  if (q->tail != nullptr) {
    if (q->tail->prev == tail) {
      q->tail->prev = nullptr;
    }
    else if (q->tail == tail) {
      q->tail = nullptr;
    }
  }

  return result;
}

// `batch` will be null queue after pushing
void queue_batch_push(struct queue* to, struct queue* batch) {
  if (batch->head == nullptr) {
    return;
  }

  if (to->tail == nullptr) {
    assert(to->head == nullptr);

    to->head = batch->head;
    to->tail = batch->tail;
    to->count = batch->count;
    return;
  }

  to->tail->next = batch->head;
  batch->head->prev = to->tail;
  to->count += batch->count;

  to->tail = batch->tail;

  batch->head = nullptr;
  batch->tail = nullptr;
  batch->count = 0;
}


void test_batch_push() {
  struct queue queue = {};
  queue_push(&queue, nullptr);
  assert(queue.head != nullptr);
  assert(queue.tail != nullptr);
  assert(queue.count == 1);
  assert(queue.tail == queue.head);

  struct queue some_batch = {};
  queue_push(&some_batch, nullptr);
  queue_push(&some_batch, nullptr);
  assert(some_batch.head != nullptr);
  assert(some_batch.tail != nullptr);
  assert(some_batch.count == 2);
  assert(some_batch.tail != queue.head);

  queue_batch_push(&queue, &some_batch);
  assert(queue.count == 3);
  assert(queue.tail != nullptr);
  assert(queue.head != nullptr);
  assert(queue.head != queue.tail);

  int len = 0;
  for (struct queue_node* it = queue.head; it != nullptr; it = it->next) {
    len += 1;
    if (len == 1) {
      assert(queue.head == it);
    }
    if (len == 3) {
      assert(queue.tail == it);
      assert(it->next == nullptr);
      assert(it->prev != nullptr);
    }
  }
  assert((size_t)len == queue.count);
}

void test_zero_sized_batch() {
  struct queue queue = {};

  queue_push(&queue, nullptr);

  struct queue batch = queue_batch_pop(&queue, 0);
  assert(batch.count == 0);
  assert(batch.head == nullptr);
  assert(batch.tail == nullptr);
}

void test_batch_from_not_enough_sized() {
  struct queue queue = {};
  queue_push(&queue, nullptr);
  assert(queue.count == 1);
  assert(queue.head != nullptr);
  assert(queue.tail != nullptr);
  assert(queue.head == queue.tail);

  struct queue batch = queue_batch_pop(&queue, 3);
  assert(batch.head != nullptr);
  assert(batch.tail != nullptr);
  assert(batch.tail == batch.head);
  assert(batch.count == 1);

  assert(queue.count == 0);
  assert(queue.head == nullptr);
  assert(queue.tail == nullptr);
}

void test_batch_from_empty() {
  struct queue empty_q = {};
  assert(empty_q.count == 0);
  assert(empty_q.head == nullptr);
  assert(empty_q.tail == nullptr);

  struct queue batch = queue_batch_pop(&empty_q, 2);
  assert(batch.count == 0);
  assert(batch.head == nullptr);
  assert(batch.tail == nullptr);
}

void test_batch() {
  test_batch_from_empty();
  test_batch_from_not_enough_sized();
  test_zero_sized_batch();

  test_batch_push();

  struct queue queue = {};
  for (int i = 0; i < 5; ++i) {
    queue_push(&queue, nullptr);
  }
  assert(queue.count == 5);
  assert(queue.head != nullptr);
  assert(queue.tail != nullptr);
  assert(queue.tail != queue.head);

  struct queue_node* orig_tail = queue.tail;
  struct queue_node* orig_head = queue.head;

  struct queue batch = queue_batch_pop(&queue, 4);
  assert(batch.count == 4);
  assert(batch.head != nullptr);
  assert(batch.tail != nullptr);
  assert(batch.head == orig_head);
  assert(batch.tail != orig_tail);
  assert(batch.tail != batch.head);

  assert(queue.count == 1);
  assert(queue.head != nullptr);
  assert(queue.tail != nullptr);
  assert(queue.head == queue.tail);
  assert(queue.tail == orig_tail);
  assert(queue.tail->prev == nullptr);
  assert(queue.tail->next == nullptr);
  assert(queue.head->next == nullptr);
  assert(queue.head->prev == nullptr);
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

  test_batch();
  printf("ALL PASSED\n");
}

#endif // ifndef QUEUE_H
