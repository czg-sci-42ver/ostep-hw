CC := clang
OSFLAG := -D $(shell uname -s)
CFLAGS := -Wall -Wextra -pthread
LARGE_NUM=1000000

all: approximate_counter.out queue.out simple_counter.out \
	standard_list.out hand_over_hand.out btree.out \
	approximate_counter_comparison.out simple_counter_comparison.out \
	approximate_counter_check_threashold.out \
	btree_before_more_mod.out \
	btree_1.out

approximate_counter.out: approximate_counter.c
	$(CC) -o $@ $^ $(CFLAGS) -lm $(OSFLAG)

# from csapp_global.pdf p599; -pg doesn't help inside the function
approximate_counter_comparison.out: approximate_counter.c
	$(CC) -o $@ $^ $(CFLAGS) -lm $(OSFLAG) -DONE_THRESHOLD -DONE_MILLION=${LARGE_NUM}

approximate_counter_check_threashold.out: approximate_counter.c
	$(CC) -o $@ $^ $(CFLAGS) -lm $(OSFLAG) -DCOMPARE_THRESHOLD

queue.out: concurrent_queue.c
	$(CC) -o $@ $^ $(CFLAGS)

simple_counter.out: simple_concurrent_counter.c thread_helper.h
	$(CC) -o $@ $< $(CFLAGS) $(OSFLAG)

simple_counter_comparison.out: simple_concurrent_counter.c thread_helper.h
	$(CC) -o $@ $< $(CFLAGS) $(OSFLAG) -DUSE_ONCE_MANY_THREADS -DONE_MILLION=${LARGE_NUM}

standard_list.out: concurrent-linked-list.c
	$(CC) -o $@ $^ $(CFLAGS)

hand_over_hand.out: hand-over-hand-locking-list.c
	$(CC) -o $@ $^ $(CFLAGS)

btree.out: btree.c
	$(CC) -o $@ $^ $(CFLAGS) -g

btree_1.out: btree.c
	$(CC) -o $@ $^ $(CFLAGS) -g -DTHREAD_NUM=1

btree_before_more_mod.out: btree_before_more_mod.c
	$(CC) -o $@ $^ $(CFLAGS) -g

clean:
	rm -f ./*.out
