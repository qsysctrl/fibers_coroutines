#ifndef CORO_H
#define CORO_H

/*
 * 1:1 coroutines
*/

#include "context.h"
#include <assert.h>
#include <string.h>

void coro_trampoline(void*, void*, void*, void*, void*, void*, coro_t* c) {
  c->_user(c);
  c->_is_done = true;
  swap_context(&c->_ctx, &c->_caller_ctx);
}

// From caller to coroutine
// Returns `true` if success resuming and false if coroutine is done already
bool coro_resume(coro_t* c) {
  assert(c != nullptr);
  if (!c->_is_done) {
    swap_context(&c->_caller_ctx, &c->_ctx);
    return true;
  }
  return false;
}

// From coroutine to caller
void coro_suspend(coro_t* c) {
  assert(c != nullptr);
  swap_context(&c->_ctx, &c->_caller_ctx);
}

coro_t* allocate_coro(user_f f) {
  coro_t* r = malloc(sizeof(coro_t));
  memcpy(r, &(coro_t){
    ._user = f,
    ._stack_view = allocate_guarded_stack(4096),
    ._caller_ctx = {},
    ._ctx = {
      .rip = &coro_trampoline,
    },
    ._is_done = false,
  }, sizeof(coro_t));

  void* stack_base = get_stack_start(r->_stack_view);
  printf("%p\n", stack_base);
  r->_ctx.rsp = setup_context(stack_base, &coro_trampoline, r);
  return r;
}

void free_coro(coro_t* c) {
  assert(c != nullptr);

  free_stack(c->_stack_view);
  free(c);
}

#endif // #ifndef CORO_H
