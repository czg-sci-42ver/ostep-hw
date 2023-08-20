#include <stdio.h>  // fread, fopen, fclose
#include <stdlib.h> // exit
#include <string.h>
#include <sys/wait.h>
#include <unistd.h> // fork

#define errExit(msg)                                                           \
  do {                                                                         \
    perror(msg);                                                               \
    exit(EXIT_FAILURE);                                                        \
  } while (0)

static void write_to_file(FILE *f, char *str) { // APUE 8.9
  char *ptr;
  int c;
  /*
  `!= 0` to avoid early ending of the combination of the two strings. 
  */
  for (ptr = str; (c = *ptr++) != 0;) {
    /*
    `man fread`: cast to an unsigned char, to stream.
    implies binary ascii.
    */
    if (fputc(c, f) != c)
      errExit("fputc");
    if (fflush(f) == EOF)
      errExit("fflush");
  }
}

int main() {
  FILE *f = fopen("./2.txt", "w+");
  if (f == NULL)
    errExit("fopen");

  pid_t cpid = fork();
  if (cpid < 0)
    errExit("fork");
  else if (cpid == 0) {
    write_to_file(f, "child says hello.\n");
  } else {
    write_to_file(f, "parent says goodbye.\n");

    if (wait(NULL) == -1)
      errExit("wait");
    /*
    explicit ending
    */
    fputc('\0', f);

    char buf[BUFSIZ];
    printf("file contents:\n");
    if (fseek(f, 0, SEEK_SET) == -1)
      errExit("fseek");
    fread(buf, BUFSIZ, 1, f);
    // for (size_t i=0; i< BUFSIZ; i++) {
    for (size_t i=0; i< strlen("child says hello.\n")+strlen("parent says goodbye.\n")+100; i++) {
      if (i>strlen("child says hello.\n")+strlen("parent says goodbye.\n")) {
        if(buf[i]!=0)
          printf("%ldth char:%d\n",i,buf[i]);
      } else {
        printf("%ldth char:%d\n",i,buf[i]);
      }
    }
    /*
    here probably ending with '\0' although not all chars in `buf` are this case if not explicitly assigning '\0' to them.
    */
    printf("%s", buf);
    fclose(f);
  }
  return 0;
}
