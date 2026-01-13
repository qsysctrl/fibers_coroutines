#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "coro.h"

void foo(coro_t* c) {
  printf("coro: step 2\n");

  coro_suspend(c);

  printf("coro: step 4\n");

  printf("coro: end\n");
}

constexpr int arr_size = 10;
constexpr int arr[arr_size] = {
  1, 2, 3, 4, 5, 6, 7, 8, 9, 10
};
void coro1_array_process(coro_t* c) {
  for (int i = 0; i < arr_size; i += 2) {
    printf("coro1: array element %d == %d;\n", i, arr[i]);
    coro_suspend(c);
  }
}
void main_array_process(coro_t* c) {
  for (int i = 1; i < arr_size; i += 2) {
    coro_resume(c);
    printf("main: array element %d == %d;\n", i, arr[i]);
  }
}

int main() {
  auto coro1 = allocate_coro(&coro1_array_process);

  coro_resume(coro1);

  main_array_process(coro1);

  if (coro_resume(coro1) == false) {
    printf("Cororoutine completed");
    free_coro(coro1);
  }

  return 0;
}

