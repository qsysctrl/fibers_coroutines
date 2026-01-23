gcc -c set_context.s -o obj/set_context.o
gcc -c get_context.s -o obj/get_context.o
gcc -c swap_context.s -o obj/swap_context.o
gcc -c setup_context.s -o obj/setup_context.o

# address, ub, leak sans -g:
gcc -std=c2x -Wall -Wextra -Wpedantic -fsanitize=address,undefined,leak -g obj/set_context.o obj/get_context.o obj/swap_context.o obj/setup_context.o -o out/fiber fiber.c

# thread san -g:
# gcc -std=c2x -Wall -Wextra -Wpedantic -fsanitize=thread -g obj/set_context.o obj/get_context.o obj/swap_context.o obj/setup_context.o -o out/fiber fiber.c

# -g
# gcc -std=c2x -Wall -Wextra -Wpedantic -g -lm obj/set_context.o obj/get_context.o obj/swap_context.o obj/setup_context.o -o out/fiber fiber.c

# -O2
# gcc -std=c2x -Wall -Wextra -Wpedantic -O2 obj/set_context.o obj/get_context.o obj/swap_context.o obj/setup_context.o -o out/fiber fiber.c
