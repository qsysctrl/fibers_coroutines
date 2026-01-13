#ifndef FIBERS_N_1_H
#define FIBERS_N_1_H

/*
Наивный подход:
У скедулера будет очередь исполняющихся, но прерванных (yielded) файберов.
Когда файбер прерывается (уступает исполнение), скедулер запускает первый файбер из этой очереди.
Если очередь пустая, просто продолжает выполнение текущего файбера.
*/

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "context.h"


struct queue_node {
  struct execution_context* fiber;
  struct queue_node* next;
  struct queue_node* prev;
};

struct queue {
  struct queue_node* head;
  struct queue_node* tail;
  size_t count;
};

void queue_push(struct queue* queue, struct execution_context* e) {
  struct queue_node* node = malloc(sizeof(struct queue_node)); // https://stackoverflow.com/questions/1538420/difference-between-malloc-and-calloc
  if (node == nullptr) {
    perror("queue_add_element, malloc error");
    exit(EXIT_FAILURE);
  }
  memset(node, 0, sizeof(struct queue_node));
  node->fiber = e;

  if (queue->tail == nullptr) {
    assert(queue->head == nullptr);

    queue->head = node;
    queue->tail = node;

    queue->count += 1;
    return;
  }

  queue->tail->next = node;
  node->prev = queue->tail;
  queue->tail = node;

  queue->count += 1;
}

[[nodiscard]]
struct execution_context* queue_pop(struct queue* queue) {
  if (queue->head == nullptr) {
    return nullptr;
  }

  struct queue_node* popped = queue->head; //     -- --
  struct queue_node* new_head = queue->head->next;
  if (new_head != nullptr) {
    new_head->prev = nullptr;
  }
  queue->head = new_head;

  struct execution_context* res = popped->fiber;
  free(popped);

  queue->count -= 1;

  return res;
}

void fiber_trampoline(void*, void*, void*, void*, void*, void*, struct execution_context* ctx) {
  assert(ctx != nullptr);
  ctx->_user(ctx);
  ctx->_is_done = true;
  swap_context(&ctx->_ctx, &ctx->_caller_ctx); // switch to scheduler
}

struct execution_context* allocate_fiber(user_f f) {
  struct execution_context* r = malloc(sizeof(coro_t));
  memcpy(r, &(struct execution_context){
    ._user = f,
    ._stack_view = allocate_guarded_stack(4096),
    ._caller_ctx = {},
    ._ctx = {
      .rip = &fiber_trampoline,
    },
    ._is_done = false,
  }, sizeof(struct execution_context));

  void* stack_base = get_stack_start(r->_stack_view);
  r->_ctx.rsp = setup_context(stack_base, &fiber_trampoline, r);
  return r;
}

void free_fiber(struct execution_context* ctx) {
  free_stack(ctx->_stack_view);
  free(ctx);
}

void fiber_yield(struct execution_context* ctx) {
  swap_context(&ctx->_ctx, &ctx->_caller_ctx);
}

struct scheduler {
  struct queue run_queue;
};

void scheduler_run(struct scheduler* s) {
  for(;;) {
    struct execution_context* ctx = queue_pop(&s->run_queue);
    if (ctx == nullptr) {
      break;
    }

    swap_context(&ctx->_caller_ctx, &ctx->_ctx);
    if (ctx->_is_done) {
      free_fiber(ctx);
      continue;
    }
    queue_push(&s->run_queue, ctx);
  }
}

void scheduler_add_fiber(struct scheduler* s, user_f f) {
  struct execution_context* ctx = allocate_fiber(f);
  queue_push(&s->run_queue, ctx);
}

[[nodiscard]]
struct scheduler* get_scheduler() {
  static __thread struct scheduler instance = {};
  return &instance; 
}

void n_1_foo(struct execution_context* ctx) {
  printf("foo() step 1\n");
  fiber_yield(ctx);
  printf("foo() step 2\n");
}

void n_1_bar(struct execution_context* ctx) {
  printf("bar() step 1\n");

  struct scheduler* s = get_scheduler();
  scheduler_add_fiber(s, &n_1_foo);

  fiber_yield(ctx);
  printf("bar() step 2\n");
  fiber_yield(ctx);
  printf("bar() step 3\n");
}

void fibers_N_1_example() {
  struct scheduler* s = get_scheduler();
  scheduler_add_fiber(s, &n_1_foo);
  scheduler_add_fiber(s, &n_1_bar);

  scheduler_run(s);

  printf("final\n");
}

#endif // #define FIBERS_N_1_H
