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

#endif // ifndef QUEUE_H
