HEADER=$(shell ls *.h)
all: xcheck.out xcheck_contest_1.out xcheck_contest_2.out xcheck_contest_3.out
#all:
	@echo ${HEADER}
%.out: %.c ${HEADER}
	$(CC) $(CFLAGS) -o $@ $^ -g
