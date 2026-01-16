#ifndef FIXED_QUEUE_H
#define FIXED_QUEUE_H

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


typedef void payload_t;

struct queue_buffer {
  void* ptr;
  size_t size;
  size_t offset;
};

struct queue_node {
  struct payload* payload;
  struct queue_node* next;
  struct queue_node* prev;
};

// struct queue {
//   struct queue_node* head;
//   struct queue_node* tail;
// };

struct fixed_queue {
  struct queue_node* head;
  struct queue_node* tail;
  struct queue_buffer buffer;
};

struct fixed_queue allocate_fixed_queue(size_t start_buffer_size) {
  assert(start_buffer_size > sizeof(struct queue_node));

  struct fixed_queue r = {
    .head = nullptr, 
    .tail = nullptr,
    .buffer = {
      .offset = 0,
      .size = start_buffer_size,
      .ptr = calloc(1, start_buffer_size),
    }
  };

  if (r.buffer.ptr == nullptr) {
    perror("queue allocation error");
    exit(EXIT_FAILURE);
  }

  return r;
}

void free_fixed_queue(struct fixed_queue* queue) {
  free(queue->buffer.ptr);
}

size_t _free_space_amount(struct queue_buffer* buffer) {
  return buffer->size - buffer->offset;
}

struct queue_node* _make_queue_node(struct fixed_queue* queue) {
  if (_free_space_amount(&queue->buffer) < sizeof(struct queue_node)) {
    return nullptr;
  }

  // https://www.gnu.org/software/libc/manual/html_node/Aligned-Memory-Blocks.html
  const size_t align = _Alignof(struct queue_node);
  const size_t aligned_offset  = (queue->buffer.offset + align - 1) & ~(align - 1);

  struct queue_node* const node = (void*)((unsigned char*)queue->buffer.ptr + aligned_offset);
  memcpy(node, &(struct queue_node){}, sizeof(struct queue_node));
  queue->buffer.offset += sizeof(struct queue_node);
  
  return node;
}

bool _is_last_node_in_buffer(struct queue_buffer* buffer, struct queue_node* node) {
  return (void*)((unsigned char*)buffer->ptr + buffer->offset) == (void*)node;
}

void _tryto_destroy_queue_node(struct fixed_queue* queue, struct queue_node* node) {
  if (!_is_last_node_in_buffer(&queue->buffer, node)) {
    return;
  }

  queue->buffer.offset -= sizeof(struct queue_node);
}

void fixed_queue_push(struct fixed_queue* queue, payload_t* e) {
  // struct queue_node* node = calloc(1, sizeof(struct queue_node));
  struct queue_node* node = _make_queue_node(queue);
  if (node == nullptr) {
    #ifdef FIXED_QUEUE_ABORTING
      fprintf(stderr, "Not enough space for new node in the fixed queue buffer");
      abort();
    #endif
    return;
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
payload_t* fixed_queue_pop(struct fixed_queue* queue) {
  if (queue->head == nullptr) {
    return nullptr;
  }

  struct queue_node* popped = queue->head;
  if (popped->next == nullptr) {
    queue->head = nullptr;
    queue->tail = nullptr;
  } else {
    queue->head = popped->next;
  }

  payload_t* res = popped->payload;
  _tryto_destroy_queue_node(queue, popped);

  return res;
}

#endif // #ifndef FIXED_QUEUE_H
