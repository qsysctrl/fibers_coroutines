#include "bench/bench.h"

#include "fixed_queue.h"


static constexpr size_t push_benchmark_iterations = 50;
static void push_benchmark_iteration(void* queue) {
  static size_t iterations = push_benchmark_iterations;
  while (iterations != 0) {
    fixed_queue_push(queue, nullptr);

    iterations -= 1;
  }
  iterations = push_benchmark_iterations;
}
static void push_benchmark(struct b_state* state) {
  struct fixed_queue queue = allocate_fixed_queue(sizeof(struct queue_node) * push_benchmark_iterations);

  b_state_set_iteration_f(state, push_benchmark_iteration, &queue);
  b_state_set_processed_items(state, push_benchmark_iterations);
  b_state_iterate(state);
}

B_BENCHMARKS(
  push_benchmark,
);

B_MAIN()