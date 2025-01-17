#include <errno.h>
#include <stdio.h>
#include <stdlib.h>   // exit
#include <string.h>   // strlen
#include <sys/wait.h> // waitpid
#include <unistd.h>   // fork, pipe, close, write, dup2
/*
It's ok to be similar to code homework 3.
*/
#define REDIRECT_FILENO 0

#define errExit(msg)                                                           \
  do {                                                                         \
    perror(msg);                                                               \
    exit(EXIT_FAILURE);                                                        \
  } while (0)

int main() {
  int pipefd[2];
  if (pipe(pipefd) == -1)
    errExit("pipe");

  pid_t rc[2];
  rc[0] = fork();
  if (rc[0] < 0)
    errExit("fork");
  else if (rc[0] == 0) {
    close(pipefd[0]);                 /* Close unused read end */
    #if REDIRECT_FILENO
    if (pipefd[1] != STDOUT_FILENO) { // APUE 15.2
      if (dup2(pipefd[1], STDOUT_FILENO) != STDOUT_FILENO)
        errExit("dup2");
      printf("\nclose STDOUT_FILENO");
      close(pipefd[1]);
    }
    printf("First child says hello.");
    #else
    char* pack = "First child says hello.";
    write(pipefd[1], pack, strlen(pack));
    #endif
  } else {
    rc[1] = fork();
    if (rc[1] < 0)
      errExit("fork");
    else if (rc[1] == 0) {
      close(pipefd[1]); /* Close unused write end */
      char buf[BUFSIZ];
      #if REDIRECT_FILENO
      if (pipefd[0] != STDIN_FILENO) {
        if (dup2(pipefd[0], STDIN_FILENO) != STDIN_FILENO)
          errExit("dup2");
        close(pipefd[0]);
      }
      read(STDIN_FILENO, buf, BUFSIZ);
      #else
      read(pipefd[0], buf, BUFSIZ);
      #endif
      printf("Second child print: %s\n", buf);
    } else {
      if (waitpid(rc[0], NULL, 0) == -1)
        errExit("waitpid");
      if (waitpid(rc[1], NULL, 0) == -1)
        errExit("waitpid");
    }
  }
  return 0;
}
