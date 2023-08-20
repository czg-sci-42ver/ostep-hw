/*
See this https://stackoverflow.com/a/5583764/21294350
and `man 7 feature_test_macros` -> search by `/^ +_GNU_SOURCE`
*/
#define _GNU_SOURCE // for execvpe()
/*
1. https://gcc.gnu.org/onlinedocs/cpp/_005f_005fhas_005finclude.html
test whether valid to `#include`
2. `man 7 feature_test_macros`
> it is not necessary to explicitly include it in order to employ feature test macros.
*/

// #if __has_include(<features.h>)
// #  include <features.h> // __GNU_LIBRARY__
// #endif
#include <fcntl.h>  // open
#include <stdio.h>  // perror
#include <stdlib.h> // exit
#include <sys/wait.h>
#include <unistd.h> // fork, exec*, fexecve

#define BUFSIZE 800

#define errExit(msg)                                                           \
  do {                                                                         \
    perror(msg);                                                               \
    exit(EXIT_FAILURE);                                                        \
  } while (0)

int main() { // APUE 8.10
  char *argv[] = {"ls", "-l", "-a", "-h", (char *)NULL};
  char *envp[] = {"PATH=/bin:/usr/bin", NULL};

  /*
  Check PATH
  https://joequery.me/code/environment-variable-c/
  */
  char path[BUFSIZE];
  char *envvar = "PATH";

  // Make sure envar actually exists
  if(!getenv(envvar)){
      fprintf(stderr, "The environment variable %s was not found.\n", envvar);
      exit(1);
  }

  // Make sure the buffer is large enough to hold the environment variable
  // value. 
  if(snprintf(path, BUFSIZE, "%s", getenv(envvar)) >= BUFSIZE){
      fprintf(stderr, "BUFSIZE of %d was too small. Aborting\n", BUFSIZE);
      exit(1);
  }
  printf("PATH: %s\n", path);

  pid_t cpid = fork();
  if (cpid == -1)
    errExit("fork");
  else if (cpid == 0) {
    printf("execl:\n");
    if (execl("/bin/ls", "ls", "-l", "-a", "-h", (char *)NULL) == -1)
      errExit("execl");
  }

  if (wait(NULL) == -1)
    errExit("wait");
  if ((cpid = fork()) == -1)
    errExit("fork");
  else if (cpid == 0) {
    printf("\nexeclp:\n");
    if (execlp("ls", "ls", "-l", "-a", "-h", (char *)NULL) == -1)
      errExit("execlp");
  }

  if (wait(NULL) == -1)
    errExit("wait");
  if ((cpid = fork()) == -1)
    errExit("fork");
  else if (cpid == 0) {
    printf("\nexecle:\n");
    if (execle("/bin/ls", "ls", "-l", "-a", "-h", (char *)NULL, envp) == -1)
      errExit("execle");
  }

  if (wait(NULL) == -1)
    errExit("wait");
  if ((cpid = fork()) == -1)
    errExit("fork");
  else if (cpid == 0) {
    printf("\nexecv:\n");
    if (execv("/bin/ls", argv) == -1)
      errExit("execv");
  }

  if (wait(NULL) == -1)
    errExit("wait");
  if ((cpid = fork()) == -1)
    errExit("fork");
  else if (cpid == 0) {
    printf("\nexecvp:\n");
    if (execvp("ls", argv) == -1)
      errExit("execvp");
  }

  if (wait(NULL) == -1)
    errExit("wait");
  if ((cpid = fork()) == -1)
    errExit("fork");
  else if (cpid == 0) {
    printf("\nexecve:\n");
    if (execve("/bin/ls", argv, envp) == -1) // system call
      errExit("execve");
  }

#ifdef __GNU_LIBRARY__
  printf("Use __GNU_LIBRARY__\n");
  if (wait(NULL) == -1)
    errExit("wait");
  if ((cpid = fork()) == -1)
    errExit("fork");
  else if (cpid == 0) {
    printf("\nexecvpe:\n");
    if (execvpe("ls", argv, envp) == -1)
      errExit("execvpe");
  }

  if (wait(NULL) == -1)
    errExit("wait");
  if ((cpid = fork()) == -1)
    errExit("fork");
  else if (cpid == 0) {
    printf("\nfexecve:\n");
    int fd = 0;
    /*See `man fexecve` for why O_PATH*/
    if ((fd = open("/bin/ls", O_PATH)) == -1)
      errExit("open");
    if (fexecve(fd, argv, envp) == -1)
      errExit("fexecve");
  }
#endif

  return 0;
}
