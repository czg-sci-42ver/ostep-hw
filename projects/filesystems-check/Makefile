USE_GCC=1
ifeq (${USE_GCC},1)
	CC=gcc
	#HEADER=$(shell ls *.h)
else
	CC=clang
	HEADER=
endif
#CFLAGS=-g -O0 -fno-tree-fre
CFLAGS=-g
all: xcheck.out xcheck_contest_1.out xcheck_contest_2.out xcheck_contest_3.out
	@echo ${HEADER}
%.out: %.c ${HEADER}
	$(CC) $(CFLAGS) -o $@ $^
