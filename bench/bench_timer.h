#ifndef _B_TIMER_H
#define _B_TIMER_H
#define _POSIX_C_SOURCE 199309L


#include <stdio.h>
#include <bits/time.h>
#include <time.h>

#define _B_NS_PER_SEC_LL    1000000000LL
#define _B_NS_PER_MSEC_LL   1000000LL
#define _B_NS_PER_USEC_LL   1000LL

#define _B_NANOSECOND_SYMBOL  "ns"
#define _B_MILLISECOND_SYMBOL "ms"
#define _B_MICROSECOND_SYMBOL "us"
#define _B_SECOND_SYMBOL      "s"


#define _B_CLOCK_ID_REAL CLOCK_MONOTONIC
#define _B_CLOCK_ID_CPU CLOCK_THREAD_CPUTIME_ID

enum b_timer_type {
  B_TIMER_CPU = 1,
  B_TIMER_REAL = 1 << 1,
  B_TIMER_CPU_AND_REAL = B_TIMER_REAL | B_TIMER_CPU,
};

struct _b_timer {
  enum b_timer_type type;
};

static inline long long _b_make_time_ns(struct timespec* ts) {
  return (ts->tv_sec * _B_NS_PER_SEC_LL) + ts->tv_nsec;
}

static inline long long _b_timer_get_now_cpu_time_ns(void) {
  struct timespec ts = {0, 0};
  int err = clock_gettime(_B_CLOCK_ID_CPU, &ts);
  if (err != 0) {
    perror("get_thread_cpu_time error");
    return -1;
  }
  return _b_make_time_ns(&ts);
}

static inline long long _b_timer_get_now_real_time_ns(void) {
  struct timespec ts = {0, 0};
  int err = clock_gettime(_B_CLOCK_ID_REAL, &ts);
  if (err != 0) {
    perror("get_real_time error");
    return -1;
  }
  return _b_make_time_ns(&ts);
}

struct _b_timer_results {
  long long cpu_time;
  long long real_time;
};

static inline struct _b_timer_results _b_timer_get_now_ns(struct _b_timer* timer) {
  struct _b_timer_results res = {-1, -1};
  if ( timer->type & B_TIMER_CPU ) {
    res.cpu_time = _b_timer_get_now_cpu_time_ns();
  }
  if ( timer->type & B_TIMER_REAL ) {
    res.real_time = _b_timer_get_now_real_time_ns();
  }
  return res;
}

static inline struct _b_timer_results 
_b_timer_results_diff(const struct _b_timer_results* restrict start, const struct _b_timer_results* restrict end) {
  long long cpu_time = -1;
  long long real_time = -1;
  if (end->cpu_time >= 0 && start->cpu_time >= 0) {
    cpu_time = end->cpu_time - start->cpu_time;
  }
  if (end->real_time >= 0 && start->real_time >= 0) {
    real_time = end->real_time - start->real_time;
  }
  return (struct _b_timer_results) { .cpu_time = cpu_time, .real_time = real_time };
}

#endif
