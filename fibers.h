#ifndef FIBERS_H
#define FIBERS_H

#include <asm-generic/errno-base.h>
#include <bits/pthreadtypes.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef FIBERS_N_1_H // TODO: delete this
# error "Fibers is already defined in by including fibers_N_1.h"
#endif

/*
 * TODOs:
 * [ ] netpoll
 * [ ] Fibers stealing
 * - [ ] lock-free SPMC queue for local run queues
 */


// #include <threads.h>
#include <assert.h>
#include <string.h>
#include <stddef.h>
#include <sys/sysinfo.h>
#include <pthread.h>
#include "context.h"
#include "queue.h"

void mutex_try_lock(pthread_mutex_t* mtx) {
  int err = pthread_mutex_lock(mtx);
  if (err != 0) {
    fprintf(stderr, "mutex lock error: %s\n", strerror(err));
    exit(EXIT_FAILURE);
  }
}
void mutex_try_unlock(pthread_mutex_t* mtx) {
  int err = pthread_mutex_unlock(mtx);
  if (err != 0) {
    fprintf(stderr, "mutex unlock error: %s\n", strerror(err));
    exit(EXIT_FAILURE);
  }
}


typedef struct execution_context fiber_t;

typedef void(*fiber_f)(fiber_t*);

struct processor;

struct scheduler {
  pthread_t thread;
  struct processor* proc; // associated processor
};

struct processor {
  struct queue local_run_queue; // executable fibers
  struct queue free_list; // free fibers (after completion of the execution)
  struct scheduler* current_thread; // associated thread
  struct runtime* rt;
  bool shutdown_flag;
};


struct runtime {
  struct queue global_run_queue;
  struct processor* procs; // PROCS_MAX = user defined maximum number of processors
  pthread_t* threads;
  size_t procs_count; // length of the `procs` array
  size_t threads_count; // length of the `scheds` array
  bool shutdown_flag;
  pthread_mutex_t rt_mtx;
  pthread_cond_t rt_cnd;
};

struct scheduler* get_scheduler() {
  static thread_local struct scheduler instance = {};
  return &instance;
}

void _fiber_switch_to_scheduler(fiber_t* ctx) {
  swap_context(&ctx->_ctx, &ctx->_caller_ctx);
}

void yield(fiber_t* current_fb) {
  queue_push(&get_scheduler()->proc->local_run_queue, current_fb);
  _fiber_switch_to_scheduler(current_fb);
}

void _fiber_trampoline(void*, void*, void*, void*, void*, void*, fiber_t* fb) {
  assert(fb != nullptr);
  fb->_user(fb);
  fb->_is_done = true;
  _fiber_switch_to_scheduler(fb);
}

fiber_t* _allocate_fiber(fiber_f f) {
  struct execution_context* r = malloc(sizeof(coro_t));
  memcpy(r, &(struct execution_context){
    ._user = f,
    ._stack_view = allocate_guarded_stack(4096),
    ._caller_ctx = {},
    ._ctx = {
      .rip = (void*)&_fiber_trampoline,
    },
    ._is_done = false,
  }, sizeof(struct execution_context));

  void* stack_base = get_stack_start(r->_stack_view);
  r->_ctx.rsp = setup_context(stack_base, &_fiber_trampoline, r);
  return r;
}

void _fiber_reset(fiber_t* fb, fiber_f f) {
  fb->_user = f;
  memset(&fb->_ctx, 0, sizeof(fb->_ctx));
  memset(&fb->_ctx, 0, sizeof(fb->_caller_ctx));
  fb->_ctx.rip = (void*)&_fiber_trampoline;
  fb->_is_done = false;

  void* stack_base = get_stack_start(fb->_stack_view);
  fb->_ctx.rsp = setup_context(stack_base, &_fiber_trampoline, fb);
}

void _push_fiber_to_free_queue(struct scheduler* sched, fiber_t* fb) {
  queue_push(&sched->proc->free_list, fb);
}

void free_fiber(fiber_t* fb) {
  free_stack(fb->_stack_view);
  free(fb);
}

bool scheduler_add_fiber(struct scheduler* sched, fiber_f f) {
  // No data race because sched->proc is used only by current thread
  if (sched->proc->shutdown_flag) {
    return false;
  }

  fiber_t* fb = queue_pop(&sched->proc->free_list);
  if (fb != nullptr) {
    printf("%zu: got new fiber context from free queue\n", pthread_self());
    _fiber_reset(fb, f);
    queue_push(&sched->proc->local_run_queue, fb);
    return true;
  }

  fb = _allocate_fiber(f);
  queue_push(&sched->proc->local_run_queue, fb);
  return true;
}
bool start(fiber_f f) {
  return scheduler_add_fiber(get_scheduler(), f);
}

void _proc_free_free_queue_fibers(struct processor* proc) {
  fiber_t* fb = nullptr;
  while ((fb = queue_pop(&proc->free_list)) != nullptr) {
    free_fiber(fb);
  }
}

void _scheduler_shutdown() {
  printf("%zu: shutdown\n", pthread_self());

  struct scheduler* sched = get_scheduler();

  _proc_free_free_queue_fibers(sched->proc);
}

fiber_t* _scheduler_get_executable_fiber() {
  struct scheduler* const sched = get_scheduler();

  assert(sched->proc != nullptr);
  assert(sched->proc->rt != nullptr);

  struct runtime* const rt = sched->proc->rt;
  pthread_mutex_t* const grq_mtx = &rt->rt_mtx;
  pthread_cond_t* const grq_cnd = &rt->rt_cnd;

  fiber_t* fb = nullptr;

  for (;;) {
    fb = queue_pop(&sched->proc->local_run_queue);
    if (fb != nullptr) {
      printf("%zu: start executing from local queue\n", pthread_self());
      return fb;
    }
    printf("%zu: local run queue is empty, trying global queue\n", pthread_self());

    mutex_try_lock(grq_mtx);
    fb = queue_pop(&rt->global_run_queue);
    if (fb != nullptr) {
      mutex_try_unlock(grq_mtx);
      printf("%zu: start executing from global queue\n", pthread_self());
      return fb;
    }

    if (rt->shutdown_flag) {
      mutex_try_unlock(grq_mtx);
      return nullptr;
    }

    pthread_cond_wait(grq_cnd, grq_mtx);
    mutex_try_unlock(grq_mtx);
  }
  assert(false);
  unreachable();
}

void _runtime_bind_proc_to_sched(struct runtime* rt) {
  assert(rt != nullptr);

  struct scheduler* sched = get_scheduler();

  // TODO: mechanism of selecting the processor for binding
  // Now just statis var
  static size_t proc_idx = 0;

  mutex_try_lock(&rt->rt_mtx);
  sched->proc = &rt->procs[proc_idx];
  rt->procs[proc_idx].current_thread = sched;

  ++proc_idx;
  if (proc_idx == rt->procs_count) {
    proc_idx = 0;
  }
  mutex_try_unlock(&rt->rt_mtx);
}

void* _scheduler_start_loop(void* _rt) {
  assert(_rt != nullptr);

  printf("%zu: join in loop\n", pthread_self());

  struct runtime* rt = _rt;

  _runtime_bind_proc_to_sched(rt);

  for (;;) {
    fiber_t* fb = _scheduler_get_executable_fiber();
    if (fb == nullptr) {
      break;
    }

    swap_context(&fb->_caller_ctx, &fb->_ctx);
    if (fb->_is_done) {
      printf("%zu: push completed fiber context to free queue\n", pthread_self());
      struct scheduler* const sched = get_scheduler();
      queue_push(&sched->proc->free_list, fb);
      // _push_fiber_to_free_queue(fb);
    }
  }

  _scheduler_shutdown();

  return nullptr;
}

void _runtime_init_procs(struct runtime* rt) {
  assert(rt != nullptr);
  assert(rt->procs != nullptr);

  for (size_t i = 0; i < rt->procs_count; ++i) {
    struct processor* const proc = &(rt->procs[i]);

    proc->rt = rt;
  }
}

[[nodiscard]]
struct runtime* allocate_runtime(fiber_f start_f) {
  int nps = get_nprocs();
  assert(nps >= 1);

  struct runtime* rt = malloc(sizeof(struct runtime));
  memcpy(rt, &(struct runtime) {
    // Equals for now
    .procs_count = (size_t)nps,
    .threads_count = (size_t)nps,

    .shutdown_flag = false,

  }, sizeof(struct runtime));

  rt->procs = calloc(rt->procs_count, sizeof(struct processor));
  if (rt->procs == nullptr) {
    perror("allocate_runtime() calloc error");
    exit(EXIT_FAILURE);
  }
  rt->threads = calloc(rt->threads_count, sizeof(struct scheduler));
  if (rt->threads == nullptr) {
    free(rt->procs);
    perror("allocate_runtime() calloc error");
    exit(EXIT_FAILURE);
  }

  pthread_mutex_init(&rt->rt_mtx, nullptr);
  pthread_cond_init(&rt->rt_cnd, nullptr);

  fiber_t* start_fb = _allocate_fiber(start_f);
  queue_push(&rt->global_run_queue, start_fb);

  _runtime_init_procs(rt);

  assert(rt->global_run_queue.head != nullptr);
  assert(rt->shutdown_flag == false);
  return rt;
}

void free_runtime(struct runtime* rt) {
  if (pthread_mutex_destroy(&rt->rt_mtx) == EBUSY) {
    fprintf(stderr, "Cannot destroy mutex because it is currently locked");
    exit(EXIT_FAILURE);
  }
  if (pthread_cond_destroy(&rt->rt_cnd)) {
    fprintf(stderr, "Cannot destroy cond because some threads are currently waiting on it");
    exit(EXIT_FAILURE);
  }

  free(rt->procs);
  free(rt->threads);

  free(rt);
}

void* _test(void*) {
  printf("\nhui\n");
  return 0;
}

void runtime_start(struct runtime* rt) {
  printf("runtime start\n");

  printf("threads count = %zu\n", rt->threads_count);

  for (size_t i = 0; i < rt->threads_count; ++i) {

    int err = pthread_create(&rt->threads[i], nullptr, &_scheduler_start_loop, rt);
    if (err != 0) {
      fprintf(stderr, "pthread_create error %s\n", strerror(err));
      exit(EXIT_FAILURE);
    }

    printf("thread %zu created\n", rt->threads[i]);
  }
}

void runtime_graceful_stop(struct runtime* rt) {
  mutex_try_lock(&rt->rt_mtx);

  assert(rt->global_run_queue.head == nullptr); // for debug
  rt->shutdown_flag = true;

  for (size_t i = 0; i < rt->procs_count; ++i) {
    rt->procs[i].shutdown_flag = true;
  }

  mutex_try_unlock(&rt->rt_mtx);
  pthread_cond_broadcast(&rt->rt_cnd);

  for (size_t i = 0; i < rt->threads_count; ++i) {
    int err = pthread_join(rt->threads[i], nullptr);
    if (err != 0) {
      fprintf(stderr, "thread join error: %s\n", strerror(err));
      exit(EXIT_FAILURE);
    }
  }
  printf("runtime stoped\n");
}

#endif // #ifndef FIBERS_H
