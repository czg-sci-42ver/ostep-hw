#ifndef __REQUEST_H__

#define MAXBUF (512)

enum result {
    OK,
    NotFound,
    Forbidden,
	Error
};

#define SMALL_BUF (50)
#include <sys/types.h> // man off_t

typedef struct __Buffer_t {
	int fd;
	int is_static;
	off_t size;
	char pathname[SMALL_BUF];
	char cgiargs[SMALL_BUF];
	int handling;
} Buffer_t;

int pre_handle_request(int fd, Buffer_t *reqBuf);
int request_serve_dynamic(int fd, char *filename, char *cgiargs);
int request_serve_static(int fd, char *filename, int filesize);

#endif // __REQUEST_H__
