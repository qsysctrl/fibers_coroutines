#ifndef FIBERS_H
#define FIBERS_H

// Source - https://stackoverflow.com/a
// Posted by Davislor, modified by community. See post 'Timeline' for change history
// Retrieved 2026-01-20, License - CC BY-SA 4.0
// #define _XOPEN_SOURCE   600
// #define _POSIX_C_SOURCE 200112L


#include <threads.h>
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
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stddef.h>
#include <sys/sysinfo.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>
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
  struct runtime* rt;
  fiber_t* current_fb;
  int stealing_backoff;
};
constexpr int g_stealing_backoff_max = 2;

struct processor {
  struct queue local_run_queue; // executable fibers
  struct queue free_list; // free fibers (after completion of the execution)
  struct scheduler* current_thread; // associated thread
  struct runtime* rt;
  bool shutdown_flag;
};


struct runtime {
  struct queue global_run_queue;
  struct queue detached_procs_queue;
  struct processor* procs; // PROCS_MAX = user defined maximum number of processors
  pthread_t* threads;
  size_t procs_count; // length of the `procs` array
  size_t threads_count; // length of the `scheds` array
  size_t sleep_count;
  bool shutdown_flag;
  pthread_mutex_t rt_mtx;
  pthread_cond_t rt_cnd;
};

struct scheduler* get_scheduler() {
  static thread_local struct scheduler instance = {};
  return &instance;
}

void _scheduler_detach_processor(struct scheduler* sched) {
  assert(sched != nullptr);
  struct runtime* const rt = sched->proc->rt;
  assert(rt != nullptr);

  mutex_try_lock(&rt->rt_mtx);

  sched->proc->current_thread = nullptr;
  assert(sched->proc != nullptr);
  queue_push(&rt->detached_procs_queue, sched->proc);
  assert(rt->detached_procs_queue.count != 0);

  mutex_try_unlock(&rt->rt_mtx);

  sched->proc = nullptr;

  printf("Processor detach signal sended\n");
  pthread_cond_signal(&rt->rt_cnd);
}

bool _scheduler_get_free_processor(struct scheduler* sched) {
  assert(sched != nullptr);
  struct runtime* const rt = sched->rt;
  assert(rt != nullptr);

  assert(sched->proc == nullptr);

  mutex_try_lock(&rt->rt_mtx);

  struct processor* proc = queue_pop(&rt->detached_procs_queue);
  if (proc == nullptr) {
    return false;
  }

  assert(proc->current_thread == nullptr);

  sched->proc = proc;
  proc->current_thread = sched;

  mutex_try_unlock(&rt->rt_mtx);

  printf("[TH:%zu], got free processor\n", pthread_self());

  return true;
}

void _fiber_switch_to_scheduler(fiber_t* ctx) {
  swap_context(&ctx->_ctx, &ctx->_caller_ctx);
}

void yield() {
  const struct scheduler* const sched = get_scheduler();
  assert(sched->current_fb != nullptr);

  if (sched->proc != nullptr) {
    queue_push(&sched->proc->local_run_queue, sched->current_fb);
  }
  _fiber_switch_to_scheduler(sched->current_fb);
}

void _fiber_trampoline(void*, void*, void*, void*, void*, void*, fiber_t* fb) {
  assert(fb != nullptr);
  fb->_user(fb);
  fb->_is_done = true;
  _fiber_switch_to_scheduler(fb);
}

[[nodiscard]]
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

void free_fiber(fiber_t* fb) {
  free_stack(fb->_stack_view);
  free(fb);
}

void sleep_for(struct timespec duration) {
  bool detach = duration.tv_sec != 0 || duration.tv_nsec > 10;
  struct scheduler* const sched = get_scheduler();

  if (detach) {
    assert(get_scheduler()->rt != nullptr);
    _scheduler_detach_processor(sched);
    printf("[TH:%zu] processor detached\n", pthread_self());
  }

  // nanosleep(&duration, nullptr);
  thrd_sleep(&duration, nullptr); // TODO: fix

  if (detach) {
    assert(get_scheduler()->proc == nullptr);
    assert(get_scheduler()->rt != nullptr);
    _scheduler_get_free_processor(sched);
    printf("[TH:%zu] processor attached\n", pthread_self());
  }
}

bool scheduler_add_fiber(struct scheduler* sched, fiber_f f) {
  if (sched == nullptr) {
    fprintf(stderr, "schdeuler_add_fiber error: sched nullptr\n");
    exit(EXIT_FAILURE);
  }

  if (sched->proc == nullptr) {
    fprintf(stderr, "schdeuler_add_fiber error: scheduler not attached to any processor\n");
    exit(EXIT_FAILURE);
  }

  if (sched->proc->shutdown_flag) {
    return false;
  }

  fiber_t* fb = queue_pop(&sched->proc->free_list);
  if (fb != nullptr) {
    printf("[TH:%zu] got new fiber context from free queue\n", pthread_self());
    _fiber_reset(fb, f);
    queue_push(&sched->proc->local_run_queue, fb);
    return true;
  }

  fb = _allocate_fiber(f);
  queue_push(&sched->proc->local_run_queue, fb);
  return true;
}
bool go(fiber_f f) {
  return scheduler_add_fiber(get_scheduler(), f);
}

void _processor_free_free_queue(struct processor* proc) {
  if (proc == nullptr) {
    return;
  }

  fiber_t* fb = nullptr;
  while ((fb = queue_pop(&proc->free_list)) != nullptr) {
    free_fiber(fb);
  }
}

void _scheduler_try_fiber_steal(struct scheduler* sched) {
  struct runtime* const rt = sched->rt;

  if (rt->detached_procs_queue.count != 0) {

    size_t idx = (size_t)rand() % rt->detached_procs_queue.count;

    struct queue_node* target = rt->detached_procs_queue.head;
    assert(target != nullptr);

    for (size_t i = 0; i < idx; ++i) {
      target = target->next;
    }

    assert(target != nullptr);
    struct processor* target_proc = target->payload;

    // return queue_pop(&proc->local_run_queue);
    if (target_proc->local_run_queue.count == 0) {
      return;
    }
    struct queue stealed = queue_batch_pop(&target_proc->local_run_queue, (target_proc->local_run_queue.count + 1) / 2);
    queue_batch_push(&sched->proc->local_run_queue, &stealed);

    printf("[TH:%zu] stealed from detached processor\n", pthread_self());
  }
  else {
    if (sched->stealing_backoff >= g_stealing_backoff_max) {
      sched->stealing_backoff = 0;
      size_t idx = (size_t)rand() % rt->procs_count;

      struct processor* target = &rt->procs[idx];

      if (target->local_run_queue.count != 0) {
        size_t to_batch = (target->local_run_queue.count + 1) / 2;
        struct queue stealed = queue_batch_pop(&target->local_run_queue, to_batch);
        queue_batch_push(&sched->proc->local_run_queue, &stealed);
      }
    }

    sched->stealing_backoff += 1;
  }

  // return nullptr;
}

void _runtime_send_stop_signal(struct runtime* rt);

fiber_t* _scheduler_get_executable_fiber(struct scheduler* sched) {
  assert(sched->proc->rt != nullptr);

  struct runtime* const rt = sched->proc->rt;
  pthread_mutex_t* const grq_mtx = &rt->rt_mtx;
  pthread_cond_t* const grq_cnd = &rt->rt_cnd;

  fiber_t* fb = nullptr;

  for (;;) {
    if (sched->proc == nullptr) {
      if (!_scheduler_get_free_processor(sched)) {
        printf("[TH:%zu] failed to get processor\n", pthread_self());
        goto sleep;
      }
      assert(sched->proc != nullptr);
    }

    fb = queue_pop(&sched->proc->local_run_queue);
    if (fb != nullptr) {
      // Q_LOG_TRACE(g_rt_logger, "[TH:%zu] start executing from local queue", pthread_self());
      return fb;
    }

    mutex_try_lock(grq_mtx);
    if (rt->global_run_queue.count != 0) {
      size_t to_batch = (rt->global_run_queue.count + 1) / 2;

      struct queue batch = queue_batch_pop(&rt->global_run_queue, to_batch);
      printf("[TH:%zu] batched from global queue - %zu fibers\n", pthread_self(), batch.count);
      queue_batch_push(&sched->proc->local_run_queue, &batch);
      // Q_LOG_TRACE(g_rt_logger, "[TH:%zu] local queue count after moving from global queue = %zu", pthread_self(), sched->proc->local_run_queue.count);

      mutex_try_unlock(grq_mtx);
      continue;
    }

    _scheduler_try_fiber_steal(sched);
    if (sched->proc->local_run_queue.count != 0) {
      mutex_try_unlock(grq_mtx);
      printf("[TH:%zu] get stealed - %zu fibers\n", pthread_self(), sched->proc->local_run_queue.count);
      continue;
    }

    if (rt->shutdown_flag) {
      mutex_try_unlock(grq_mtx);
      return nullptr;
    }


    if (rt->sleep_count == rt->threads_count - 1) {
      mutex_try_unlock(grq_mtx);
      _runtime_send_stop_signal(rt);
      printf("Last thread exits\n");
      return nullptr;
    }

  sleep:
    // printf("%zu: going to sleep\n", pthread_self());
    // Q_LOG_TRACE(g_rt_logger, "[TH:%zu] going to sleep", pthread_self());
    rt->sleep_count += 1;
    pthread_cond_wait(grq_cnd, grq_mtx);
    rt->sleep_count -= 1;
    mutex_try_unlock(grq_mtx);
  }
  assert(false);
  unreachable();
}

void _runtime_add_fiber(struct runtime* rt, fiber_t* fb) {
  mutex_try_lock(&rt->rt_mtx);

  // fiber_t* fb = _allocate_fiber(f);
  queue_push(&rt->global_run_queue, fb);

  mutex_try_unlock(&rt->rt_mtx);
  pthread_cond_signal(&rt->rt_cnd);
}

void _runtime_bind_proc_to_sched(struct runtime* rt, struct scheduler* sched) {
  assert(rt != nullptr);

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

void* _scheduler_start_runtime_loop(void* _rt) {
  assert(_rt != nullptr);

  struct scheduler* const sched = get_scheduler();

  // Q_LOG_TRACE(g_rt_logger, "[TH:%zu] starting scheduler", pthread_self());

  struct runtime* rt = _rt;
  sched->rt = _rt;

  _runtime_bind_proc_to_sched(rt, sched);

  for (;;) {
    fiber_t* fb = _scheduler_get_executable_fiber(sched);
    if (fb == nullptr) {
      break;
    }

    sched->current_fb = fb;

    swap_context(&fb->_caller_ctx, &fb->_ctx);
    if (fb->_is_done) {
      printf("[TH:%zu] pushing completed fiber to free queue\n", pthread_self());
      queue_push(&sched->proc->free_list, fb);
    }
  }

  printf("[TH:%zu] shutdown\n", pthread_self());
  // _scheduler_shutdown();

  return nullptr;
}

void _runtime_init_processors(struct runtime* rt) {
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

  if (start_f != nullptr) {
  fiber_t* start_fb = _allocate_fiber(start_f);
  queue_push(&rt->global_run_queue, start_fb);
  }

  _runtime_init_processors(rt);

  // assert(rt->global_run_queue.head != nullptr);
  assert(rt->shutdown_flag == false);
  return rt;
}

void free_runtime(struct runtime* rt) {
  if (pthread_mutex_destroy(&rt->rt_mtx) == EBUSY) {
    fprintf(stderr, "Cannot destroy mutex because it is currently locked\n");
    exit(EXIT_FAILURE);
  }
  if (pthread_cond_destroy(&rt->rt_cnd) != 0) {
    fprintf(stderr, "Cannot destroy cond because some threads are currently waiting on it\n");
    exit(EXIT_FAILURE);
  }

  for (size_t i = 0; i < rt->procs_count; ++i) {
    _processor_free_free_queue(&rt->procs[i]);
  }

  free(rt->procs);
  free(rt->threads);

  free(rt);
}

void runtime_start(struct runtime* rt) {
  if (rt == nullptr) {
    fprintf(stderr, "runtime_start error: null runtime pointer argument\n");
    exit(EXIT_FAILURE);
  }
  printf("threads count = %zu\n", rt->threads_count);

  for (size_t i = 0; i < rt->threads_count; ++i) {

    int err = pthread_create(&rt->threads[i], nullptr, &_scheduler_start_runtime_loop, rt);
    if (err != 0) {
      fprintf(stderr, "pthread_create error: %s\n", strerror(err));
      exit(EXIT_FAILURE);
    }
  }
}

// Blocks until has been completed
void runtime_run(struct runtime* rt) {
  runtime_start(rt);

  for (size_t i = 0; i < rt->threads_count; ++i) {
    int err = pthread_join(rt->threads[i], nullptr);
    if (err != 0) {
      fprintf(stderr, "thread join error: %s\n", strerror(err));
      exit(EXIT_FAILURE);
    }
  }
  printf("===runtime stoped===\n");
}

void _scheduler_shutdown() {
  printf("%zu: shutdown\n", pthread_self());

  struct scheduler* sched = get_scheduler();

  _processor_free_free_queue(sched->proc);
}

void _runtime_send_stop_signal(struct runtime* rt) {
  mutex_try_lock(&rt->rt_mtx);

  rt->shutdown_flag = true;

  mutex_try_unlock(&rt->rt_mtx);
  pthread_cond_broadcast(&rt->rt_cnd);
}

void runtime_graceful_stop(struct runtime* rt) {
  if (rt == nullptr) {
    fprintf(stderr, "runtime_graceful_stop error: null runtime pointer argument\n");
    exit(EXIT_FAILURE);
  }

  _runtime_send_stop_signal(rt);

  for (size_t i = 0; i < rt->threads_count; ++i) {
    int err = pthread_join(rt->threads[i], nullptr);
    if (err != 0) {
      fprintf(stderr, "thread join error: %s\n", strerror(err));
      exit(EXIT_FAILURE);
    }
  }
  printf("===runtime stoped===\n");
}


#endif // #ifndef FIBERS_H
