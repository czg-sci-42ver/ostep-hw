/*
From the io_uring.pdf, many old APIs like aio_read/aio_write are kept.
*/
#include "connection.h"
#include <liburing.h>
#include <stdio.h>
#include <time.h>   // clock_gettime
#include <unistd.h> // close

#define PARALLEL_ACCEPT_RECV
#define NO_DUP_ZERO

// man io_uring
// https://kernel.dk/io_uring.pdf
// https://github.com/axboe/liburing
// https://github.com/torvalds/linux/blob/master/fs/io_uring.c
// https://github.com/torvalds/linux/blob/master/include/uapi/linux/io_uring.h
// https://lwn.net/Kernel/Index/#io_uring

struct user_data {
  char buf[BUFSIZ];
  int socket_fd;
  int file_fd;
  int index;
  int io_op;
};

struct user_data data_arr[LISTEN_BACKLOG];

int numAccepts = 0, numReqs = 0;

void prep_accept(struct io_uring *ring, int sfd) {
  struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
  if (sqe == NULL)
    handle_error("io_uring_get_sqe");

  io_uring_prep_accept(sqe, sfd, NULL, NULL, 0);
  // https://github.com/axboe/liburing/commit/8ecd3fd959634df81d66af8b3a69c16202a014e8
  /*
  See example https://github.com/axboe/liburing/blob/b58921e0b0ae84b6f1cd22b87c66a6e91a540ec8/examples/link-cp.c#L74
  and https://github.com/axboe/liburing/blob/b58921e0b0ae84b6f1cd22b87c66a6e91a540ec8/examples/ucontext-cp.c#L58C2-L58C23
  it seems `data` in io_uring_sqe_set_data depends on manual implementation.
  */
  data_arr[--numAccepts].io_op = IORING_OP_ACCEPT;
  data_arr[numAccepts].index = numAccepts;
  io_uring_sqe_set_data(sqe, &data_arr[numAccepts]);
  if (io_uring_submit(ring) < 0)
    handle_error("io_uring_submit");
}

void prep_recv(struct io_uring *ring, int sfd, int cfd, int index) {
  struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
  if (sqe == NULL)
    handle_error("io_uring_get_sqe");

  data_arr[index].io_op = IORING_OP_RECV;
  data_arr[index].socket_fd = cfd;
  memset(data_arr[index].buf, 0, BUFSIZ);
  io_uring_prep_recv(sqe, cfd, data_arr[index].buf, BUFSIZ, 0);
  io_uring_sqe_set_data(sqe, &data_arr[index]);
  if (numAccepts > 0)
    prep_accept(ring, sfd);
  #ifdef PARALLEL_ACCEPT_RECV
  if (io_uring_submit(ring) < 0)
  #else
  else if (io_uring_submit(ring) < 0)
  #endif
    handle_error("io_uring_submit");
}

void prep_read(struct io_uring *ring, int index) {
  struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
  if (sqe == NULL)
    handle_error("io_uring_get_sqe");

  int file_fd = open(data_arr[index].buf, O_RDONLY);
  if (file_fd == -1) {
    fprintf(stderr, "buf: %s\n", data_arr[index].buf);
    handle_error("open");
  }

  data_arr[index].io_op = IORING_OP_READ;
  data_arr[index].file_fd = file_fd;
  #ifndef NO_DUP_ZERO
  memset(data_arr[index].buf, 0, BUFSIZ);
  #endif
  io_uring_prep_read(sqe, file_fd, data_arr[index].buf, BUFSIZ, 0);
  io_uring_sqe_set_data(sqe, &data_arr[index]);
  if (io_uring_submit(ring) < 0)
    handle_error("io_uring_submit");
}

void prep_send(struct io_uring *ring, int index) {
  struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
  if (sqe == NULL)
    handle_error("io_uring_get_sqe");

  close(data_arr[index].file_fd);
  data_arr[index].io_op = IORING_OP_SEND;
  io_uring_prep_send(sqe, data_arr[index].socket_fd, data_arr[index].buf,
                     BUFSIZ, 0);
  io_uring_sqe_set_data(sqe, &data_arr[index]);
  if (io_uring_submit(ring) < 0)
    handle_error("io_uring_submit");
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    fprintf(stderr, "Usage: %s numReqs\n", argv[0]);
    exit(EXIT_FAILURE);
  }
  struct timespec start, end;
  if (clock_gettime(CLOCK_MONOTONIC, &start) == -1)
    handle_error("clock_gettime");
  numReqs = atoi(argv[1]);
  if (numReqs <= 0) {
    fprintf(stderr, "Get out\n");
    exit(EXIT_FAILURE);
  }
  numAccepts = numReqs;
  int sfd = init_socket(1, 0);
  struct io_uring ring;
  /*
  see io_uring.pdf p12 for the pair of init and exit.
  */
  if (io_uring_queue_init(LISTEN_BACKLOG, &ring, IORING_SETUP_SQPOLL))
    handle_error("io_uring_queue_init");
  prep_accept(&ring, sfd);

  while (numReqs > 0) {
    struct io_uring_cqe *cqe;
    if (io_uring_wait_cqe(&ring, &cqe))
      handle_error("io_uring_wait_cqe");
    /*
    https://unixism.net/loti/ref-liburing/cqe.html
    */
    if (cqe->res < 0) {
      fprintf(stderr, "I/O error: %s\n", strerror(-cqe->res));
      exit(EXIT_FAILURE);
    }
    /*
    See io_uring.pdf p3,4 for the user_data usage.
    */
    struct user_data *data = io_uring_cqe_get_data(cqe);
    switch (data->io_op) {
    case IORING_OP_ACCEPT:
    /*
    See `man io_uring_prep_accept` and io_uring.pdf p4 for res usage
    */
      prep_recv(&ring, sfd, cqe->res, data->index);
      break;
    case IORING_OP_RECV:
      prep_read(&ring, data->index);
      break;
    case IORING_OP_READ:
      prep_send(&ring, data->index);
      break;
    case IORING_OP_SEND:
      close(data_arr[data->index].socket_fd);
      numReqs--;
      break;
    default:
      handle_error("Unknown I/O");
    }
    /*
    See io_uring.pdf
    this is to reuse the cqe entry.
    */
    io_uring_cqe_seen(&ring, cqe);
  }
  io_uring_queue_exit(&ring);
  close(sfd);
  if (clock_gettime(CLOCK_MONOTONIC, &end) == -1)
    handle_error("clock_gettime");
  // nanoseconds
  printf("%f\n",
         ((end.tv_sec - start.tv_sec) * 1E9 + end.tv_nsec - start.tv_nsec));
  return 0;
}
