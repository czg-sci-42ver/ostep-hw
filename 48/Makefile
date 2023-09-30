CC = clang
CFLAGS = -Wall -Wextra -pthread -g -luring

all: UDP-client.out UDP-server.out UDP-client_async.out UDP-server_async.out

%.out: %.c UDP-lib.h UDP-lib_async.h
	$(CC) -o $@ $< $(CFLAGS)

#UDP-server.out: UDP-server.c UDP-lib.h
#	$(CC) -o $@ $< UDP-client.c $(CFLAGS)
