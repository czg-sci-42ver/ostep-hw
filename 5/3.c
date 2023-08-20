#include <signal.h> // kill, sigaction
#include <stdio.h>
#include <stdlib.h> // exit
#include <unistd.h> // getppid, fork, pause, pipe
#include <getopt.h>

#define errExit(msg)                                                           \
  do {                                                                         \
    perror(msg);                                                               \
    exit(EXIT_FAILURE);                                                        \
  } while (0)

// APUE 8.9, 10.16
// https://www.gnu.org/software/libc/manual/html_node/Sigsuspend.html
static sig_atomic_t sigflag = 0;
static void sig_handler() { sigflag = 1; }

static void wait_with_signal() {
  printf("wait with signal\n");
  struct sigaction act;
  /*
  This is similar to what csapp instructs. 
  */
  act.sa_handler = sig_handler;
  sigemptyset(&act.sa_mask);
  act.sa_flags = 0;
  if (sigaction(SIGUSR1, &act, NULL) == -1)
    errExit("sigaction");

  sigset_t cont_mask, old_mask;
  sigemptyset(&cont_mask);
  sigaddset(&cont_mask, SIGUSR1);
  /*
  block to ensure `while (sigflag == 0)` will always work
  See https://www.gnu.org/software/libc/manual/html_node/Why-Block.html#:~:text=24.7.,later%2C%20after%20you%20unblock%20them.
  > The only way to test reliably for whether a signal has yet arrived is to test while the signal is blocked.
  */
  if (sigprocmask(SIG_BLOCK, &cont_mask, &old_mask) == -1)
    errExit("sigprocmask");

  pid_t cpid = fork();
  if (cpid < 0)
    errExit("fork");
  else if (cpid == 0) {
    printf("hello\n");
    kill(getppid(), SIGUSR1);
  } else {
    while (sigflag == 0)
      sigsuspend(&old_mask);
    printf("goodbye\n");
  }
}

// APUE 15.2
static void wait_with_pipe() {
  printf("wait with pipe\n");
  /*
  See "(see pipe(7))" in `man 2 pipe`
  Then 
  > If all file descriptors referring to the write end of a pipe have been closed, then an attempt to read(2) from the pipe will see end-of-file (read(2) ...
  > An application that uses pipe(2) and fork(2) should use suitable  close(2)  calls  to
  >     close unnecessary duplicate file descriptors;
  */
  int pipefd[2];
  if (pipe(pipefd) == -1)
    errExit("pipe");

  pid_t cpid = fork();
  if (cpid < 0)
    errExit("fork");
  else if (cpid == 0) {
    printf("hello\n");
    close(pipefd[0]);
    write(pipefd[1], "c", 1);
    close(pipefd[1]);
  } else {
    close(pipefd[1]);
    char c;
    /*
    from `man 2 read`:
    EAGAIN The  file  descriptor fd refers to a file other than a socket and has been marked nonblocking (O_NONBLOCK), and the read would block.
    implies blocking (Also see "EWOULDBLOCK" in `man 3 errno`)
    */
    read(pipefd[0], &c, 1); // block
    close(pipefd[0]);
    printf("goodbye\n");
  }
}

_Noreturn static void usage(char *name) {
// static void usage(char *name) {
  fprintf(stderr, "Usage: %s [-s|--signal] [-p|--pipe]\n", name);
  exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
  int opt;
  struct option options[] = {
    {"signal", no_argument, NULL, 's'},
    {"pipe", no_argument, NULL, 'p'}
  };
  if ((opt = getopt_long(argc, argv, "sp", options, NULL)) != -1) {
    switch (opt) {
    case 's':
      wait_with_signal();
      break;
    case 'p':
      wait_with_pipe();
      break;
    default:
      usage(argv[0]);
    }
  } else
    usage(argv[0]);

  return 0;
}
