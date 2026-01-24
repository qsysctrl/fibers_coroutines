gcc -c set_context.s -o obj/set_context.o
gcc -c get_context.s -o obj/get_context.o
gcc -c swap_context.s -o obj/swap_context.o
gcc -c setup_context.s -o obj/setup_context.o

gcc -std=c2x -Wall -Wextra -Wpedantic -O2 -DNDEBUG obj/set_context.o obj/get_context.o obj/swap_context.o obj/setup_context.o -o benchs/fiber_bench fiber_benchmark.c
