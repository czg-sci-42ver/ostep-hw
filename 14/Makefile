# `CFLAGS='-g' make --always-make` by https://stackoverflow.com/a/56613606/21294350
CC = clang
CFLAGS += -Wall -Wextra

all: null.out forget_free.out size_100.out free_then_print.out vector.out free_then_print_6.out

clean:
	rm -f ./*.out

vector.out: vector.c vector.h

%.out: %.c
	$(CC) -o $@ $< $(CFLAGS)

free_then_print_6.out:free_then_print.c
	$(CC) -DPROBLEM_6 -o $@ $< $(CFLAGS)