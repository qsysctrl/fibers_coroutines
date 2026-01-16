#ifndef _B_FILTER_H
#define _B_FILTER_H

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define _B_WINDOW_SIZE 101
#define _B_MAD_THRESHOLD 3.0

struct _b_filter {
  long long window[_B_WINDOW_SIZE];
  long long sorted[_B_WINDOW_SIZE];
  size_t index;
  size_t count;
  size_t valid_count;
  double sum;
  double sum_squares;
  double mean;
  double m2;
};

static inline struct _b_filter _b_filter_init(void) {
  return (struct _b_filter) {0};
}

static inline int _b_compare_longs(const void* a, const void* b) {
  long long aa = *(const long long*)a;
  long long bb = *(const long long*)b;
  return (aa > bb) - (aa < bb);
}

static inline long long _b_compute_median(long long* sorted, size_t count) {
  if (sorted == NULL) return -1;
  if (count == 0) return 0;
  if (count % 2 == 1) {
      return sorted[count / 2];
  } else {
      return (sorted[count / 2 - 1] + sorted[count / 2]) / 2;
  }
}

static inline long long _b_compute_mad(struct _b_filter* filter) {
  if (filter == NULL) return -1;  
  if (filter->count < 3) return 0;
    
  memcpy(filter->sorted, filter->window, filter->count * sizeof(long long));
  qsort(filter->sorted, filter->count, sizeof(long long), _b_compare_longs);
  
  long long median = _b_compute_median(filter->sorted, filter->count);
  
  long long deviations[_B_WINDOW_SIZE];
  for (size_t i = 0; i < filter->count; i++) {
      deviations[i] = llabs(filter->window[i] - median); // todo: брать значения сразу из filter->sorted
  }
  
  qsort(deviations, filter->count, sizeof(long long), _b_compare_longs);
  return _b_compute_median(deviations, filter->count);
}

static inline bool _b_filter_add(struct _b_filter* filter, long long value) {
  if (filter == NULL) abort(); //    
  filter->window[filter->index] = value;
  filter->index = (filter->index + 1) % _B_WINDOW_SIZE;
  if (filter->count < _B_WINDOW_SIZE) {
      filter->count++;
  }
  
  if (filter->count < 10) {
    filter->valid_count++;
    filter->sum += value;
    filter->sum_squares += value * value;
    
    double delta = value - filter->mean;
    filter->mean += delta / filter->valid_count;
    filter->m2 += delta * (value - filter->mean);
    
    return true;
  }
  
  long long mad = _b_compute_mad(filter);
  long long median = _b_compute_median(filter->sorted, filter->count);
  
  if (mad == 0) {
      mad = median / 10;
      if (mad == 0) mad = 100;
  }
  
  long long deviation = llabs(value - median);
  bool is_outlier = (deviation > _B_MAD_THRESHOLD * mad * 1.4826);
  
  if (is_outlier) {
      return false;
  }
  
  filter->valid_count++;
  filter->sum += value;
  filter->sum_squares += value * value;
  
  double delta = value - filter->mean;
  filter->mean += delta / filter->valid_count;
  filter->m2 += delta * (value - filter->mean);
  
  return true;
}

static inline double _b_filter_get_mean(const struct _b_filter* filter) {
    return filter->valid_count > 0 ? filter->sum / filter->valid_count : 0.0;
}

static inline double _b_filter_get_stddev(const struct _b_filter* filter) {
    return filter->valid_count > 1 ? sqrt(filter->m2 / (filter->valid_count - 1)) : 0.0;
}


#endif
