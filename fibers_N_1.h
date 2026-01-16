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
#include "queue.h"

void fiber_switch_to_scheduler(struct execution_context* ctx) {
  swap_context(&ctx->_ctx, &ctx->_caller_ctx);
}

void fiber_trampoline(void*, void*, void*, void*, void*, void*, struct execution_context* ctx) {
  assert(ctx != nullptr);
  ctx->_user(ctx);
  ctx->_is_done = true;
  fiber_switch_to_scheduler(ctx);
  // swap_context(&ctx->_ctx, &ctx->_caller_ctx); // switch to scheduler
}

struct execution_context* allocate_fiber(user_f f) {
  struct execution_context* r = malloc(sizeof(coro_t));
  memcpy(r, &(struct execution_context){
    ._user = f,
    ._stack_view = allocate_guarded_stack(4096),
    ._caller_ctx = {},
    ._ctx = {
      .rip = (void*)&fiber_trampoline,
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
  // swap_context(&ctx->_ctx, &ctx->_caller_ctx);
  fiber_switch_to_scheduler(ctx);
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

void scheduler_add_fiber_with_payload(struct scheduler* s, user_f f, void* payload) {
  struct execution_context* ctx = allocate_fiber(f);
  ctx->payload = payload;
  queue_push(&s->run_queue, ctx);
}

void scheduler_add_fiber(struct scheduler* s, user_f f) {
  scheduler_add_fiber_with_payload(s, f, nullptr);
}


[[nodiscard]]
struct scheduler* get_scheduler() {
  static __thread struct scheduler instance = {};
  return &instance; 
}

void stack_overflow(size_t i) {
  printf("recursive %zu\n", i);
  stack_overflow(++i);
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
  scheduler_add_fiber(s, &n_1_foo);

  fiber_yield(ctx);
  printf("bar() step 2\n");
  fiber_yield(ctx);
  printf("bar() step 3\n");
}

void test_fibers() {

}

void fibers_N_1_example() {
  struct scheduler* s = get_scheduler();

  scheduler_add_fiber(s, &n_1_foo);
  scheduler_add_fiber(s, &n_1_bar);

  
  scheduler_run(s);

  printf("final\n");
}

#endif // #define FIBERS_N_1_H
