#ifndef FIBERS_H
#define FIBERS_H

#ifdef FIBERS_N_1_H // TODO: delete this
# error "Fibers is already defined in by including fibers_N_1.h"
#endif


#include <threads.h>
#include <assert.h>
#include <string.h>
#include <sys/sysinfo.h>
#include "context.h"
#include "queue.h"


typedef struct execution_context fiber_t;

typedef void(*fiber_f)(fiber_t*);

struct processor;

struct scheduler {
  thrd_t thread;
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
  thrd_t* threads;
  size_t procs_count; // length of the `procs` array
  size_t threads_count; // length of the `scheds` array
  bool shutdown_flag;
  mtx_t grq_mtx;
  cnd_t grq_cnd;
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

struct execution_context* _allocate_fiber(fiber_f f) {
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

void free_fiber(fiber_t* fb) {
  free_stack(fb->_stack_view);
  free(fb);
}

bool scheduler_add_fiber(struct scheduler* sched, fiber_f f) {
  if (sched->proc->shutdown_flag) {
    return false;
  }

  fiber_t* fb = _allocate_fiber(f);
  queue_push(&sched->proc->local_run_queue, fb);
  return true;
}
bool start(fiber_f f) {
  return scheduler_add_fiber(get_scheduler(), f);
}

fiber_t* _scheduler_get_executable_fiber(struct scheduler* sched) {
  assert(sched != nullptr);
  assert(sched->proc != nullptr);
  assert(sched->proc->rt != nullptr);

  fiber_t* fb = queue_pop(&sched->proc->local_run_queue);
  if (fb != nullptr) {
    return fb;
  }
  printf("%zu: local run queue is empty, trying global queue\n",thrd_current());

  struct runtime* const rt = sched->proc->rt;
  mtx_t* const grq_mtx = &rt->grq_mtx;
  cnd_t* const grq_cnd = &rt->grq_cnd;

  mtx_lock(grq_mtx);
  if (rt->shutdown_flag) {
    printf("%zu: shutdown\n", thrd_current());
    mtx_unlock(grq_mtx);
    return nullptr;
  }

  // TODO: pop several fibers in one time
  while (!rt->shutdown_flag) {
    // TODO: fibers stealing
    if ((fb = queue_pop(&rt->global_run_queue)) == nullptr) {
      printf("%zu: global run queue is empty, parking\n", thrd_current());
      assert(rt->parked_count <= rt->threads_count);

      cnd_wait(grq_cnd, grq_mtx);
    }
    else { // fb != nullptr
      printf("%zu: start executing\n", thrd_current());
      break; // while (!rt->shutdown_flag)
    }
  }

  if (fb == nullptr) {
    assert(rt->shutdown_flag == true);
    printf("%zu: shutdown\n", thrd_current());
  }

  if (rt->shutdown_flag) { // for debug only
    assert(fb == nullptr);
  }

  mtx_unlock(grq_mtx);

  return fb;
}

void _runtime_bind_proc_to_sched(struct runtime* rt, struct scheduler* binding_sched) {
  assert(rt != nullptr);
  assert(binding_sched != nullptr);

  // TODO: mechanism of selecting the processor for binding
  // Now just statis var
  static size_t proc_idx = 0;

  binding_sched->proc = &rt->procs[proc_idx];
  rt->procs[proc_idx].current_thread = binding_sched;

  ++proc_idx;
  if (proc_idx == rt->procs_count) {
    proc_idx = 0;
  }
}

int _scheduler_start_loop(void* _rt) {
  struct runtime* rt = _rt;
  struct scheduler* sched = get_scheduler();

  _runtime_bind_proc_to_sched(rt, sched);

  assert(sched != nullptr);

  for (;;) {
    fiber_t* fb = _scheduler_get_executable_fiber(sched);
    if (fb == nullptr) {
      break;
    }

    swap_context(&fb->_caller_ctx, &fb->_ctx);
    if (fb->_is_done) {
      free_fiber(fb);
    }
  }

  return 0;
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

  if (mtx_init(&rt->grq_mtx, mtx_plain) == thrd_error) {
    free(rt->procs);
    free(rt->threads);

    fprintf(stderr, "mtx_init error\n");
    exit(EXIT_FAILURE);
  }
  if (cnd_init(&rt->grq_cnd) == thrd_error) {
    free(rt->procs);
    free(rt->threads);
    mtx_destroy(&rt->grq_mtx);

    fprintf(stderr, "cnd_init() error\n"); // TODO
    exit(EXIT_FAILURE);
  }

  fiber_t* start_fb = _allocate_fiber(start_f);
  queue_push(&rt->global_run_queue, start_fb);

  _runtime_init_procs(rt);

  assert(rt->global_run_queue.head != nullptr);
  assert(rt->shutdown_flag == false);
  return rt;
}

void free_runtime(struct runtime* rt) {
  mtx_destroy(&rt->grq_mtx);
  cnd_destroy(&rt->grq_cnd);

  free(rt->procs);
  free(rt->threads);

  free(rt);
}

void runtime_start(struct runtime* rt) {
  printf("runtime start\n");

  printf("scheds count = %zu\n", rt->threads_count);

  for (size_t i = 0; i < rt->threads_count; ++i) {
    int err = thrd_create(&rt->threads[i], &_scheduler_start_loop, rt);
    if (err == thrd_error) {
      fprintf(stderr, "thrd_create() error: thrd_error\n");
      exit(EXIT_FAILURE);
    }
    else if (err == thrd_nomem) {
      fprintf(stderr, "thrd_create() error: thrd_nomem\n");
      exit(EXIT_FAILURE);
    }
  }
}

void runtime_graceful_stop(struct runtime* rt) {
  mtx_lock(&rt->grq_mtx);
  if (rt->global_run_queue.head == nullptr) {
    rt->shutdown_flag = true;
  }

  for (size_t i = 0; i < rt->procs_count; ++i) {
    rt->procs[i].shutdown_flag = true;
  }

  mtx_unlock(&rt->grq_mtx);
  cnd_broadcast(&rt->grq_cnd);

  for (size_t i = 0; i < rt->threads_count; ++i) {
    thrd_join(rt->threads[i], nullptr);
  }
  printf("runtime stoped\n");
}

#endif // #ifndef FIBERS_H
