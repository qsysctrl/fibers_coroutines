#ifndef _B_BENCH_H
#define _B_BENCH_H

// #define _POSIX_C_SOURCE 199309L

#include "bench_timer.h"
#include "bench_filter.h"

#include <math.h>
#include <limits.h>
#include <stdalign.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>


#define B_MIN_BENCHMARK_TIME_NS (_B_NS_PER_SEC_LL / 2)
#define B_MIN_WARMUP_TIME_NS (0) // (_B_NS_PER_SEC_LL / 4)

#define _B_BENCH_NAME_MAX_SIZE 128

#define B_BENCHMARKS(...) b_bench_f _b_register[] = { __VA_ARGS__ }; static const char* const _b_benchs_names = #__VA_ARGS__
#define B_DO_NOT_OPTIMIZE(ADDR) __asm__ volatile ("" : : "g"(ADDR) : "memory")  

typedef void(*b_iteration_f)(void* data);

struct _b_state_results {
  double real_mean_ns;
  double real_std_dev_ns;
  double rel_real_std_dev;
  double cpu_mean_ns;
  double cpu_std_dev_ns;
  double rel_cpu_std_dev;
  double bytes_per_sec;
  double items_per_sec;
  size_t iterations;
  size_t warmup_iterations;
};

typedef struct b_state {
  struct _b_timer _timer;
  struct _b_state_results _results;
  b_iteration_f _iteration_f;
  void* _iteration_data;
  long long _min_warmup_time_ns;
  long long _min_benchmark_time_ns;
  size_t _processed_bytes;
  size_t _processed_items;
  bool _iteration_is_completed;
} b_state_t;

typedef void(*b_bench_f)(struct b_state*);

#define _B_B_PER_GB (1024*1024*1024)
#define _B_B_PER_MB (1024*1024)
#define _B_B_PER_KB (1024)

#define _B_GB_PERSEC_SYMBOL "gb/s"
#define _B_MB_PERSEC_SYMBOL "mb/s"
#define _B_KB_PERSEC_SYMBOL "kb/s"
#define _B_B_PERSEC_SYMBOL "b/s"

enum byte_multiples {
  _B_GB,
  _B_MB,
  _B_KB,
  _B_B,
};

static char* _b_bytes_persec_abbrs[] = {
  [_B_GB] = _B_GB_PERSEC_SYMBOL,
  [_B_MB] = _B_MB_PERSEC_SYMBOL,
  [_B_KB] = _B_KB_PERSEC_SYMBOL,
  [_B_B] = _B_B_PERSEC_SYMBOL,
};

enum second_multiples {
  _B_NANOSECOND,
  _B_MICROSECOND,
  _B_MILLISECOND,
  _B_SECOND,
};

static char* _b_second_symbols[] = {
  [_B_NANOSECOND] = _B_NANOSECOND_SYMBOL,
  [_B_MICROSECOND] = _B_MICROSECOND_SYMBOL,
  [_B_MILLISECOND] = _B_MILLISECOND_SYMBOL,
  [_B_SECOND] = _B_SECOND_SYMBOL,
};

static inline struct b_state b_state_init(void) {
  return (struct b_state) { 
    ._iteration_f = NULL,
    ._iteration_data = NULL,
    ._iteration_is_completed = false,
    ._min_warmup_time_ns = B_MIN_WARMUP_TIME_NS,
    ._min_benchmark_time_ns = B_MIN_BENCHMARK_TIME_NS,
    ._timer = { B_TIMER_CPU_AND_REAL },
  };
}

static inline void b_state_set_min_warmup_time_ms(struct b_state* state, size_t ms) {
  state->_min_warmup_time_ns = ms * _B_NS_PER_MSEC_LL;
}
static inline void b_state_set_min_warmup_time_s(struct b_state* state, size_t s) {
  state->_min_warmup_time_ns = s * _B_NS_PER_SEC_LL;
}

static inline void b_state_set_min_benchmark_time_ms(struct b_state* state, size_t ms) {
  state->_min_benchmark_time_ns = ms * _B_NS_PER_MSEC_LL;
}
static inline void b_state_set_min_benchmark_time_s(struct b_state* state, size_t s) {
  state->_min_benchmark_time_ns = s * _B_NS_PER_SEC_LL;
}

static inline void b_state_set_timer_type(struct b_state* state, enum b_timer_type type) {
  state->_timer.type = type;
}

static inline void b_state_set_processed_bytes(struct b_state* state, size_t val) {
  state->_processed_bytes = val;
}
static inline void b_state_set_processed_items(struct b_state* state, size_t val) {
  state->_processed_items = val;
}

static inline void b_state_set_iteration_f(struct b_state* restrict state, b_iteration_f f, void* restrict data) {
  state->_iteration_f = f;
  state->_iteration_data = data;
}

static inline struct _b_timer_results _b_measure(struct b_state* state);

static inline struct _b_state_results _b_measure_iterations(struct b_state* state, size_t init, size_t final) {
  struct _b_filter real_outlier_filter = _b_filter_init();
  struct _b_filter cpu_outlier_filter = _b_filter_init(); 
  for (; init < final; ++init) {
    struct _b_timer_results mres = _b_measure(state);
    if (mres.real_time >= 0) {
      if (!_b_filter_add(&real_outlier_filter, mres.real_time)) {
        if(init > 0) --init;
        continue;
      }
    }
    if (mres.cpu_time >= 0) {
      if (!_b_filter_add(&cpu_outlier_filter, mres.cpu_time)) {
        if(init > 0) --init;
        continue;
      }
    }
  }

  struct _b_state_results res = {
    .real_mean_ns = _b_filter_get_mean(&real_outlier_filter),
    .real_std_dev_ns = _b_filter_get_stddev(&real_outlier_filter),
    .cpu_mean_ns = _b_filter_get_mean(&cpu_outlier_filter),
    .cpu_std_dev_ns = _b_filter_get_stddev(&cpu_outlier_filter),
  };
  res.rel_real_std_dev = res.real_mean_ns ? res.real_std_dev_ns / res.real_mean_ns : 0.0 ;
  res.rel_cpu_std_dev = res.cpu_mean_ns ? res.cpu_std_dev_ns / res.cpu_mean_ns : 0.0 ;
  return res;
}

static inline double _b_state_iterate_warmup(struct b_state* state) {
  struct _b_state_results test_warmup_res = _b_measure_iterations(state, 0, 15);
  state->_results.warmup_iterations = (size_t)(state->_min_warmup_time_ns / test_warmup_res.real_mean_ns);
  _b_measure_iterations(state, 0, state->_min_warmup_time_ns / test_warmup_res.real_mean_ns);

  return test_warmup_res.real_mean_ns;
}

static inline void b_state_iterate(struct b_state* state) {  
  if (state == NULL) return;
  if (state->_iteration_f == NULL) return;
  if (state->_min_benchmark_time_ns == 0) return;

  double prior_mean_ns = 0.0;
  if(state->_min_warmup_time_ns > 0) {
    prior_mean_ns = _b_state_iterate_warmup(state);
  } else {
    struct _b_state_results test_bench_its_res = _b_measure_iterations(state, 0, 20);
    if (test_bench_its_res.real_mean_ns)
      prior_mean_ns = test_bench_its_res.real_mean_ns;
    else if (test_bench_its_res.cpu_mean_ns)
      prior_mean_ns = test_bench_its_res.cpu_mean_ns;
  }
  if (prior_mean_ns == 0.0) {
    fprintf(stderr, "Unpossible to calculate iterations count. Maybe you dont choose correct timer type.\n");
    abort();
  }
  
  struct _b_state_results bench_its_res = _b_measure_iterations(state, 0, state->_min_benchmark_time_ns / prior_mean_ns);
  
  state->_iteration_is_completed = true;
  // state->_results = bench_its_res;
  state->_results.cpu_mean_ns = bench_its_res.cpu_mean_ns;
  state->_results.cpu_std_dev_ns = bench_its_res.cpu_std_dev_ns;
  state->_results.real_mean_ns = bench_its_res.real_mean_ns;
  state->_results.real_std_dev_ns = bench_its_res.real_std_dev_ns;
  state->_results.rel_real_std_dev = bench_its_res.rel_real_std_dev;
  state->_results.rel_cpu_std_dev = bench_its_res.rel_cpu_std_dev;
  state->_results.iterations = (size_t)(state->_min_benchmark_time_ns / prior_mean_ns);
  if (bench_its_res.real_mean_ns > 0 ) {
    if (state->_processed_bytes != 0) 
      state->_results.bytes_per_sec = (state->_processed_bytes * _B_NS_PER_SEC_LL) / bench_its_res.real_mean_ns;
    if (state->_processed_items != 0)
      state->_results.items_per_sec = (state->_processed_items * _B_NS_PER_SEC_LL) / bench_its_res.real_mean_ns;
  }
}

static inline enum second_multiples _b_get_second_multiples(long long ns) {
  if (ns >= _B_NS_PER_SEC_LL) return _B_SECOND;
  else if (ns >= _B_NS_PER_MSEC_LL) return _B_MILLISECOND;
  else if (ns >= _B_NS_PER_USEC_LL) return _B_MICROSECOND;
  else return _B_NANOSECOND;
}

static inline enum byte_multiples _b_get_byte_multiples(size_t bytes) {
  if (bytes >= _B_B_PER_GB) return _B_GB;
  else if (bytes >= _B_B_PER_MB) return _B_MB;
  else if (bytes >= _B_B_PER_KB) return _B_KB;
  else return _B_B;
}

static inline struct _b_timer_results _b_measure(struct b_state* state) {
  if (state->_iteration_f == NULL) return (struct _b_timer_results) { -1, -1 };
  struct _b_timer_results tres[2];

  tres[0] = _b_timer_get_now_ns(&state->_timer);

  state->_iteration_f(state->_iteration_data);

  tres[1] = _b_timer_get_now_ns(&state->_timer);

  return _b_timer_results_diff(&tres[0], &tres[1]);
}

struct _b_str_view {
  const char* src;
  size_t len;
};

static inline struct _b_str_view _b_make_str_view(const char* start, size_t len) {
  return (struct _b_str_view) {
    .src = start,
    .len = len,
  };
}

static inline void _b_get_benchs_names(struct _b_str_view names[restrict], const char* restrict str) {
  size_t len = strlen(str);
  size_t name_count = 0;
  size_t start = 0;
  bool in_name = false;
  for (size_t i = 0; i <= len; ++i) {
    char c = str[i];
    
    if (!in_name && c != ' ' && c != ',' && c != '\0') {
      start = i;
      in_name = true;
    }
    
    if (in_name && (c == ' ' || c == ',' || c == '\0')) {
      size_t end = i;
      
      names[name_count].src = str + start;
      names[name_count].len = end - start + 1;
      name_count++;
      
      in_name = false;
      
      while (i < len && str[i] == ' ') {
        i++;
      }
    }
  }
}

static inline double _b_round_time(long long ns) {
  switch (_b_get_second_multiples(ns)) {
    case _B_NANOSECOND:  return (double)ns;
    case _B_MICROSECOND: return (double)ns / _B_NS_PER_USEC_LL;
    case _B_MILLISECOND: return (double)ns / _B_NS_PER_MSEC_LL;
    case _B_SECOND: return (double)ns / _B_NS_PER_SEC_LL;
  }
  return -1; // for hide warn
}
static inline double _b_round_bytes(size_t b) {
  switch (_b_get_byte_multiples(b)) {
    case _B_GB: return (double)b / _B_B_PER_GB;
    case _B_MB: return (double)b / _B_B_PER_MB;
    case _B_KB: return (double)b / _B_B_PER_KB;
    case _B_B: return b;
  }
  return -1; // for hide warn
}

static inline void _b_get_bench_name_str(char* buffer, struct _b_str_view* restrict name) {
  snprintf(buffer, name->len, "%s", name->src);
}

static const char* const _b_output_table_row_fmt = "| %-24s | %10s | %10s | %20s | %20s | %14s | %20s |\n";
static inline void _b_print_row(char* name , struct _b_state_results* res) {
  char iterations_buffer[10];
  snprintf(iterations_buffer, sizeof(iterations_buffer), "%zu", res->iterations);
  char warmup_buffer[10];
  snprintf(warmup_buffer, sizeof(warmup_buffer), "%zu", res->warmup_iterations);

  char real_mean_buffer[20];
  snprintf(real_mean_buffer, sizeof(real_mean_buffer), "%.2f%s +/- %.1f%%", _b_round_time(res->real_mean_ns), _b_second_symbols[_b_get_second_multiples(res->real_mean_ns)], res->rel_real_std_dev * 100.0);
  char cpu_mean_buffer[20];
  snprintf(cpu_mean_buffer, sizeof(cpu_mean_buffer), "%.2f%s +/- %.1f%%", _b_round_time(res->cpu_mean_ns), _b_second_symbols[_b_get_second_multiples(res->cpu_mean_ns)], res->rel_cpu_std_dev * 100.0);

  char bytes_p_s_buffer[10];
  snprintf(bytes_p_s_buffer, sizeof(bytes_p_s_buffer), "%0.2f%s", _b_round_bytes(res->bytes_per_sec), _b_bytes_persec_abbrs[_b_get_byte_multiples(res->bytes_per_sec)]);
  
  char items_p_s_buffer[20];
  snprintf(items_p_s_buffer, sizeof(items_p_s_buffer), "%zu", (size_t)res->items_per_sec);

  printf(_b_output_table_row_fmt, name, iterations_buffer, warmup_buffer, real_mean_buffer, cpu_mean_buffer, bytes_p_s_buffer, items_p_s_buffer);
}
static inline void _b_print_header(void) {
  printf(_b_output_table_row_fmt, "Benchmark name", "Iterations", "Warmup", "Time", "CPU", "Bytes per sec", "Items per sec");
  printf(_b_output_table_row_fmt, "------------------------", "----------", "----------", "--------------------", "--------------------", "--------------", "--------------------");
}

#define _B_BENCHS_COUNT sizeof(_b_register) / sizeof(_b_register[0])
#define B_MAIN() \
int main(void) { \
  struct _b_str_view names[_B_BENCHS_COUNT];\
  _b_get_benchs_names(names, _b_benchs_names);\
  char name_buffer[_B_BENCH_NAME_MAX_SIZE];\
  _b_print_header();\
  for (size_t i = 0; i < _B_BENCHS_COUNT; ++i) { \
    _b_get_bench_name_str(name_buffer, &names[i]);\
    struct b_state state = b_state_init(); \
    _b_register[i](&state); \
    if (state._iteration_is_completed) _b_print_row(name_buffer, &state._results);\
  } \
  exit(EXIT_SUCCESS); \
}


#endif

