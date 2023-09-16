#ifdef __linux__
// #define _GNU_SOURCE // accept4
#endif
#include "connection.h"
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/event.h>
#include <time.h>   // clock_gettime
#include <unistd.h> // close

// https://github.com/libevent/libevent
int num_reqs = 0;

/*
https://libevent.org/doc/bufferevent_8h.html#a031df52978c5237b70fb8ae7df572c97
the ctx corresponds to cbarg
*/

void readcb(struct bufferevent *bev, void *ctx) {
  char buf[BUFSIZ] = "";
  bufferevent_read(bev, buf, BUFSIZ);
  int fd = open(buf, O_RDONLY | O_NONBLOCK);
  if (fd == -1)
    handle_error("open");
  struct evbuffer *output = bufferevent_get_output(bev);
  /*
  https://libevent.org/doc/buffer_8h.html#a0d9db8b232ebf8d63c660ec429981e91
  default with EVBUFFER_FLAG_DRAINS_TO_FD set
  */
  // evbuffer_set_flags(output, EVBUFFER_FLAG_DRAINS_TO_FD);
  evbuffer_add_file(output, fd, 0, -1);
}

void errorcb(struct bufferevent *bev, short error, void *ctx) {
  if (error & BEV_EVENT_EOF) {
    if (--num_reqs == 0) {
      struct event_base *base = bufferevent_get_base(bev);
      event_base_loopexit(base, NULL);
    }
  } else if (error & BEV_EVENT_ERROR) {
    fprintf(stderr, "error: %s\n", strerror(error));
  }
  bufferevent_free(bev);
}

void do_accept(int sfd, short event, void *arg) {
  struct event_base *base = arg;
#ifdef __linux__
  int cfd = accept4(sfd, NULL, NULL, SOCK_NONBLOCK);
  if (cfd == -1)
    handle_error("accept4");
#else
  int cfd = accept(sfd, NULL, NULL);
  if (cfd == -1)
    handle_error("accept");
  evutil_make_socket_nonblocking(cfd);
#endif
  struct bufferevent *bev =
      bufferevent_socket_new(base, cfd, BEV_OPT_CLOSE_ON_FREE);
  /*
  here no write callback function because write is implemented in readcb by `evbuffer_add_file`.
  here cb -> callback although "aiocb - asynchronous I/O control block"
  */
  bufferevent_setcb(bev, readcb, NULL, errorcb, NULL);
  bufferevent_enable(bev, EV_READ | EV_WRITE);
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    printf("Usage: %s numReqs", argv[0]);
    exit(EXIT_FAILURE);
  }
  struct timespec start, end;
  if (clock_gettime(CLOCK_MONOTONIC, &start) == -1)
    handle_error("clock_gettime");

  num_reqs = atoi(argv[1]);
  int sfd = init_socket(1, 1);

  /*
  https://libevent.org/libevent-book/Ref2_eventbase.html
  */
  struct event_base *base = event_base_new();
  /*
  http://www.wangafu.net/~nickm/libevent-book/Ref4_event.html shows how to use these APIs
  https://libevent.org/doc/event_8h.html#a74bbaf2d529670cc0ab793497b41700f
  https://man7.org/linux/man-pages/man3/event.3.html EV_PERSIST
  */
  struct event *listener_event =
      event_new(base, sfd, EV_READ | EV_PERSIST, do_accept, (void *)base);
  event_add(listener_event, NULL);
  /*
  1. https://libevent.org/doc/event_8h.html#a19d60cb72a1af398247f40e92cf07056
  blocks and waits for end or event_base_loopbreak, etc.
  2. it calls epoll which is asynchronous except for timeout 0.
  by https://stackoverflow.com/a/49182928/21294350 and https://stackoverflow.com/a/6977364/21294350
  */
  event_base_dispatch(base);
  event_base_free(base);
  if (clock_gettime(CLOCK_MONOTONIC, &end) == -1)
    handle_error("clock_gettime");
  // nanoseconds
  printf("%f\n",
         ((end.tv_sec - start.tv_sec) * 1E9 + end.tv_nsec - start.tv_nsec));
  close(sfd);
  return 0;
}
