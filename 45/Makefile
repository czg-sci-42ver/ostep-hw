CC = clang
CFLAGS = -Wall -Wextra

all: check-xor.out check-fletcher.out crc.out create-csum.out check-csum.out crc_orig.out crc_orig_mod.out crc_srecord_log.out

%.out: %.c common.h
	$(CC) -o $@ $< $(CFLAGS)
crc_srecord_log.out: crc.c common.h
	$(CC) -o $@ $< $(CFLAGS) -Dsrecord_log
