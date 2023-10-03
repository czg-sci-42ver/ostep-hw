#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <stdlib.h>
#define SIZE 100
int main(int argc, char *argv[]) {
  FILE *img_file;
  assert(argc==2);
  img_file=fopen(argv[1], "r+");
  int fd = fileno(img_file);
  struct stat mystat = {};
  if (fstat(fd,&mystat)) { perror("fstat"); exit(EXIT_FAILURE); };
  void *imgp = mmap(NULL, mystat.st_size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
  if (imgp == MAP_FAILED) {
    fprintf(stderr, "imgp mmap failed\n");
    // exit(EXIT_FAILURE);
  }
  char tmp[SIZE]={0};
  memmove(tmp, imgp+5, mystat.st_size-5);
  // for (int i=0; i<SIZE; i++) {
  //   printf("%c -> %c\n",tmp[i],((char*)(imgp+5))[i]);
  // }
  memset(tmp, 0, SIZE);
  memmove(imgp+5, tmp, mystat.st_size-5);
  for (int i=0; i<SIZE; i++) {
    printf("%d -> %d\n",tmp[i],((char*)(imgp+5))[i]);
  }
}