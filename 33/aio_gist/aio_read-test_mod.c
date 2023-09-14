/*
No need for `-lrt` https://stackoverflow.com/a/56129651/21294350
although comment 
*/

/*
 *  * Copyright (c) 2004, Bull SA. All rights reserved.
 *   * Created by:  Laurent.Vivier@bull.net
 *    * This file is licensed under the GPL license.  For the full content
 *     * of this license, see the COPYING file at the top level of this
 *      * source tree.
 *       */

/*
 *  * assertion:
 *   *
 *    * aio_lio_opcode shall be ignored.
 *     *
 *      * method:
 *       *
 *        *      - write data to a file
 *         *      - fill in an aiocb with an LIO_WRITE aio_lio_opcode
 *          *      - call aio_read with this aiocb
 *           *      - check data is effectively read (ignoring aio_lio_opcode)
 *            */

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <aio.h>

#define CHECK_FD
// #define REOPEN
// #define USE_STATIC
#define USE_ENUM_COMPILE_CONST
#define USE_AIO
#define TARGET_STR "This is test"
char *filename = "test.txt";
#include <assert.h>

#define TNAME "aio_read/5-1.c"

int main() {
  char tmpfname[256];
#define BUF_SIZE 111
  char buf[BUF_SIZE]={0};
  char check[BUF_SIZE]={0};
  int fd;
  struct aiocb aiocb;
  int i;

  snprintf(tmpfname, sizeof(tmpfname), "pts_aio_read_5_1_%d", getpid());
  /*
   * remove the file for future O_CREAT|O_EXCL usage
   */
  unlink(tmpfname);

  #ifdef CHECK_FD
  /*
  https://stackoverflow.com/a/3082971/21294350
  MAXMSG is variable, so the compiler doesn't know the size at the compile time.
  */
  #ifdef USE_ENUM_COMPILE_CONST
  enum { MAXMSG = 255 };
  #elif USE_STATIC
  static const int MAXMSG =1024;
  #else
  const int MAXMSG =1024;
  #endif
  /*
  https://stackoverflow.com/a/55016661/21294350
  */
  #if defined(USE_ENUM_COMPILE_CONST) || defined(USE_STATIC)
  char buffer[MAXMSG]={0};
  #else
  char buffer[MAXMSG];
  memset( buffer, 0, MAXMSG*sizeof(char) );
  #endif
  fd = open(filename, O_RDONLY, S_IRUSR);
  if (read(fd,buffer,MAXMSG)==-1) {
    perror ("read file");
    return -1;
  }
  printf("before write, read %s\n",buffer);
  #endif

  #ifdef CHECK_FD
  /*
  https://stackoverflow.com/a/58681458/21294350 O_TRUNC
  */
  if((fd = open(filename, O_CREAT | O_RDWR | O_TRUNC, S_IRUSR | S_IWUSR))==-1){
    
  }
  #else
  fd = open(tmpfname, O_CREAT | O_RDWR | O_EXCL, S_IRUSR | S_IWUSR);
  #endif
  if (fd == -1) {
    printf(TNAME " Error at open(): %s\n", strerror(errno));
    exit(1);
  }
  #ifndef CHECK_FD
  unlink(tmpfname);
  #endif

  strcpy(buf, TARGET_STR);
  #ifdef CHECK_FD
  printf("read buf directly: %s\n",buf);
  #endif
  #ifdef CHECK_FD
  const int target_len = strlen(buf);
  if (write(fd, buf, target_len) != target_len) {
  #else
  if (write(fd, buf, BUF_SIZE) != BUF_SIZE) {
  #endif
    printf(TNAME " Error at write(): %s\n",
        strerror(errno));
    exit(1);
  }
  #ifdef CHECK_FD
  memset(check, 0, BUF_SIZE);
  #else
  memset(check, 0xaa, BUF_SIZE);
  #endif

  #ifdef USE_AIO
  memset(&aiocb, 0, sizeof(struct aiocb));
  aiocb.aio_fildes = fd;
  aiocb.aio_buf = check;
  aiocb.aio_nbytes = BUF_SIZE;
  aiocb.aio_lio_opcode = LIO_WRITE;

  if (aio_read(&aiocb) == -1) {
    printf(TNAME " Error at aio_read(): %s\n",
        strerror(errno));
    exit(2);
  }

  int err;
  int ret;

  /* Wait until end of transaction */
  while ((err = aio_error (&aiocb)) == EINPROGRESS);

  err = aio_error(&aiocb);
  ret = aio_return(&aiocb);

  if (err != 0) {
    printf(TNAME " Error at aio_error() : %s\n", strerror (err));
    close(fd);
    exit(2);
  }
  #ifdef CHECK_FD
  if (ret != target_len) {
  #else
  if (ret != BUF_SIZE) {
  #endif
    printf(TNAME " Error at aio_return()\n");
    close(fd);
    exit(2);
  }

  #ifdef CHECK_FD
  printf("read with check: %s\n",check);
  #endif

  /* check it */
  assert(strcmp(check, TARGET_STR)==0);
  #endif

  #ifdef CHECK_FD
  /*
  although `aio_read` has read, the file may not be written
  https://stackoverflow.com/a/2341236/21294350
  https://stackoverflow.com/a/3173139/21294350
  */
  assert(fflush(NULL)==0);
  assert(fsync(fd)==0);
  memset( buffer, 0, MAXMSG*sizeof(char) );
  /*
  TODO weird, REOPEN can access the contents.
  */
  #ifdef REOPEN
  int reopen_fd = open(filename, O_RDONLY, S_IRUSR);
  #endif
  #ifdef REOPEN
  if (read(reopen_fd,buffer,MAXMSG)==-1) {
  #else
  /*
  This enables to continue using read after the last write.
  https://stackoverflow.com/a/65144643/21294350
  https://stackoverflow.com/a/30437899/21294350
  */
  lseek(fd, 0, SEEK_SET);
  if (read(fd,buffer,MAXMSG)==-1) {
  #endif
    perror ("read file");
    return -1;
  }
  printf("read by `read` %s\n",buffer);
  #endif

  close(fd);
  #ifdef REOPEN
  close(reopen_fd);
  #endif
  printf ("Test PASSED\n");
  return 0;
}
