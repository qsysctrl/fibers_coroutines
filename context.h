#ifndef FIBER_CONTEXT_H
#define FIBER_CONTEXT_H

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>

#define GUARD_PAGES_COUNT (1)
#define RED_ZONE_SIZE (128)

size_t get_page_size() {
  return sysconf(_SC_PAGESIZE);
}

size_t get_byte_of_pages(size_t n) {
  return get_page_size() * n;
}

typedef unsigned char byte;

struct context {
  void* rip;
  void* rsp;

  void* rbx;
  void* rbp;
  void* r15;
  void* r14;
  void* r13;
  void* r12;
};

struct view {
  byte* ptr;
  size_t length; // includes guard page. Actual stack length excludes guard page size
};

byte* get_stack_start(struct view stack_v) {
  return stack_v.ptr + stack_v.length - RED_ZONE_SIZE;
}
size_t get_stack_size(struct view stack_v) {
  return get_stack_start(stack_v) - (stack_v.ptr + get_byte_of_pages(GUARD_PAGES_COUNT));
}

[[nodiscard]]
struct view map_stack(size_t user_pages_n, size_t guard_pages_n) {
  assert(user_pages_n != 0);
  assert(guard_pages_n != 0);

  size_t length = get_byte_of_pages(user_pages_n + guard_pages_n);
  void* result = mmap(nullptr,
                      length,
                      PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS,
                      -1,
                      0);
  if (result == MAP_FAILED) {
    return (struct view){ .ptr = MAP_FAILED };
  }

  size_t guarded_length = get_byte_of_pages(guard_pages_n);
  int err = mprotect(result, guarded_length, PROT_NONE);
  if (err == -1) {
    return (struct view){ .ptr = MAP_FAILED };
  }

  return (struct view){ .ptr = result, .length = length };
}

// Allocates zeroed stack memory at least `at_least_b` bytes (+ the guard page bytes length)
[[nodiscard("allocated stack")]]
struct view allocate_guarded_stack(size_t at_least_bytes) {
  const size_t page_size = get_page_size();
  size_t pages = at_least_bytes / page_size;
  if (at_least_bytes % page_size != 0) {
    ++pages;
  }

  auto result =  map_stack(pages, GUARD_PAGES_COUNT);
  if (result.ptr == MAP_FAILED) {
    perror("allocating stack map error");
    exit(EXIT_FAILURE);
  }

  return result;
}

int free_stack(struct view sv) {
  int r = munmap(sv.ptr, sv.length);
  if (r == -1) {
    perror("munmap freeing the stack error");
  }
  return r;
}


struct execution_context;
typedef void(*user_f)(struct execution_context*);

struct execution_context {
  user_f _user;
  struct view _stack_view;
  struct context _caller_ctx;
  struct context _ctx;
  bool _is_done;
  void* payload;
};

typedef struct execution_context coro_t;

// Get current context
extern void get_context(struct context* ctx);

// Set context
extern void set_context(const struct context* ctx);

// Save current context in `current` and set context `to`
extern void swap_context(struct context* restrict current, const struct context* restrict to);

typedef void(*trampoline)(void*, void*, void*, void*, void*, void*, struct execution_context* user);

extern void* setup_context(void* restrict stack_start, trampoline t, struct execution_context* user);

#endif // #ifndef FIBER_CONTEXT_H
