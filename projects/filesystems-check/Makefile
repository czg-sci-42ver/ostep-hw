HEADER=$(shell ls *.h)
all: xcheck.out
#all:
	@echo ${HEADER}
%.out: %.c ${HEADER}
	$(CC) $(CFLAGS) -o $@ $^
