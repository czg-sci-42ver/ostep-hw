#include "connection.h"
#include <pthread.h>
#include <unistd.h> // close

#define NOT_PENDING_ZERO_DATAGRAM

void *send_requests(void *arg) {
  char *file_name = (char *)arg;
  int sfd = init_socket(0, 0);
  printf("len:%ld\n",strlen(file_name));
  if (send(sfd, file_name, strlen(file_name), 0) == -1)
    handle_error("send");
  char buf[BUFSIZ] = "";
  printf("begin receive\n");
  /*
  by `man 2 recv`, recv doesn't pend with the "zero‐length  datagram". 
  > recv() consumes the pending datagram
  */
  #ifdef NOT_PENDING_ZERO_DATAGRAM
  if (recv(sfd, buf, BUFSIZ, 0) == -1)
  #else
  if (read(sfd, buf, BUFSIZ) == -1)
  #endif
    handle_error("recv");
  printf("receive: %s\n", buf);
  fflush(stdout);
  close(sfd);
  return NULL;
}

#define USE_THREAD

int main(int argc, char *argv[]) {
  if (argc != 3) {
    fprintf(stderr, "Usage: %s file_path num_threads\n", argv[0]);
    exit(EXIT_FAILURE);
  }
  int num_threads = atoi(argv[2]);
  #ifdef USE_THREAD
  if (num_threads > 1) {
    pthread_t threads[num_threads];
    for (int i = 0; i < num_threads; i++) {
      if (pthread_create(&threads[i], NULL, send_requests, argv[1]))
        handle_error("pthread_create");
    }
    for (int i = 0; i < num_threads; i++) {
      if (pthread_join(threads[i], NULL))
        handle_error("pthread_join");
    } 
  } else
    send_requests(argv[1]);
  #else
  while (num_threads!=0) {
    send_requests(argv[1]);
    num_threads--;
  }
  #endif
  return 0;
}
