// https://www.gnu.org/software/libc/manual/html_node/Server-Example.html from the ostep-hw README.md
/*
Added afterwards
*/
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <assert.h>
#include <fcntl.h>
/*
solve the book:
if you leave
this running for a long time, someone may figure out how to use it
to read all the files on your computer
*/
#define FORBIDDEN_FILES_NUM 10
char *forbidden_files[FORBIDDEN_FILES_NUM] = {"test1.txt","not_me.txt"};
// #define USE_AIO
#ifdef USE_AIO
#define AIO_DEBUG
// #define USE_AIO_SUSPEND
#define USE_SIGNAL 1
#include <aio.h>
#endif

#define SHOW_FD_DEBUG_OFFSET
#define DEBUG_LSOF_OFFSET

#define SIGNAL_CLEAR_FILE_CACHE
#ifdef SIGNAL_CLEAR_FILE_CACHE
#include <signal.h>
/*
the following 2 lines no use https://stackoverflow.com/a/8302589/21294350
*/
// #define _GNU_SOURCE 1
// #include <unistd.h>
#ifndef TEMP_FAILURE_RETRY
/*
1. here default return int
1.1 https://stackoverflow.com/a/48183460/21294350 _Generic still needs to take all conditions in account, and not use the compiler to decide.
2. https://stackoverflow.com/a/41446972/21294350 
3. typeof https://stackoverflow.com/a/53928817/21294350
*/
#define TEMP_FAILURE_RETRY(exp)            \
  ({                                       \
    typeof(exp) _rc;                     \
    do {                                   \
      _rc = (exp);                         \
    } while (_rc < 0 && errno == EINTR); \
    _rc;                                   \
  })
#endif

#define CACHE_SIZE 1024
#define CACHE_NUM 10
#define NAME_SIZE 20
typedef struct map {
  char content[CACHE_SIZE];
  char file_name[NAME_SIZE];
} file_map;
file_map file_cache[CACHE_NUM];
int file_cache_tail=0;
#endif

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

/*
check port usage
$ lsof -i:8080 # https://www.baeldung.com/linux/docker-address-already-in-use
$ netstat -tulpn # https://www.cyberciti.biz/faq/what-process-has-open-linux-port/
*/
#define PORT    8080
#define MAXMSG  512

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int
make_socket (uint16_t port)
{
  int sock;
  struct sockaddr_in name;

  /* Create the socket. */
  // https://stackoverflow.com/a/6737450/21294350 AF_INET also fine
  // #define AF_INET		PF_INET
  sock = socket (AF_INET, SOCK_STREAM, 0);
  if (sock < 0)
    {
      perror ("socket");
      exit (EXIT_FAILURE);
    }

  /* Give the socket a name. */
  name.sin_family = AF_INET;
  name.sin_port = htons (port);
  name.sin_addr.s_addr = htonl (INADDR_ANY);
  if (bind (sock, (struct sockaddr *) &name, sizeof (name)) < 0)
    {
      perror ("bind");
      exit (EXIT_FAILURE);
    }

  return sock;
}

#if USE_SIGNAL
/*
From aio_intel
notice original aio_flg init is wrong in example_2_orig.c.
*/
#include <signal.h>
static volatile int aio_flg = 0;
static void aio_CompletionRoutine(sigval_t sigval)
{
  aio_flg = 1;
}
#define WAIT { while (!aio_flg); aio_flg = 0; }
#endif

int
read_from_client (int filedes)
{
  char buffer[MAXMSG]={0};
  // memset(buffer, 0, MAXMSG);
  int nbytes;
  int fd;
  char content_buffer[MAXMSG]={0};
  #ifdef USE_AIO
  int err;
  /*
  must init with default 0 and optionally `aio_suspend` by https://stackoverflow.com/a/60161968/21294350
  */
  struct aiocb aio_inst = {0};
  aio_inst.aio_offset = 0;
  aio_inst.aio_buf = content_buffer;
  aio_inst.aio_nbytes = MAXMSG;
  #if USE_SIGNAL
  aio_inst.aio_sigevent.sigev_notify          = SIGEV_THREAD;
  aio_inst.aio_sigevent.sigev_notify_function = aio_CompletionRoutine;
  #endif
  #endif

  nbytes = read (filedes, buffer, MAXMSG);
  if (nbytes < 0)
    {
      /* Read error. */
      perror ("read");
      exit (EXIT_FAILURE);
    }
  else if (nbytes == 0)
    /* End-of-file. */
    return -1;
  else
    {
      /* Data read. */
      fprintf (stderr, "Server: got message: %s and len:%d\n", buffer,nbytes);
      /*
      "respond by reading the file into a buffer ..." as the book says
      */
      for (int i=1; i<FORBIDDEN_FILES_NUM; i++) {
        if (forbidden_files[i]!=NULL && strcmp(forbidden_files[i], buffer)==0)
          goto end;
      }
      printf("have checked forbidden_files\n");
      fflush(stdout);
      #ifdef SIGNAL_CLEAR_FILE_CACHE
      for (int i=0; i<CACHE_NUM; i++) {
        if (file_cache[i].file_name[0]==0) {
          printf("not find in cache\n");
          break;
        }
        if (!strcmp(file_cache[i].file_name, buffer)) {
          printf("find %s in %dth cache with contents %s",buffer,i,file_cache[i].content);
          assert(send(filedes, file_cache[i].content, strlen(file_cache[i].content), 0)!=-1);
          return 0;
        }
      }
      printf("have checked cache\n");
      #endif
      if ((fd=open(buffer, O_RDONLY, S_IRUSR))==-1) {
        perror ("open file");
        return -1;
      }
      printf("have opened the file\n");
      #ifdef SHOW_FD_DEBUG_OFFSET
      /*
      $ ./server_select.out
      ...
      file fd: 5
      ...
      # https://unix.stackexchange.com/questions/34751/how-to-find-out-the-file-offset-of-an-opened-file
      $ sudo lsof -d 5 -o 5 | grep -e "20946\|COMMAND"
      COMMAND     PID        USER   FD      TYPE             DEVICE SIZE/OFF    NODE NAME
      server_se 20946    czg_arch    5r      REG               0,26       14 7956471 /home/czg_arch/ostep-hw/33/test2.txt
      $ wc -m test2.txt 
      14 test2.txt
      */
      printf("pid: %d file fd: %d\n",getpid(),fd);
      #ifdef DEBUG_LSOF_OFFSET
      sleep(20);
      #endif
      #endif
      #ifdef SIGNAL_CLEAR_FILE_CACHE
      strcpy(file_cache[file_cache_tail].file_name, buffer);
      printf("finish file_name copy\n");
      #endif
      #ifdef USE_AIO
      aio_inst.aio_fildes = fd;
      /*
      TODO how to make the read of file asynchronous.
      */
      #ifdef AIO_DEBUG
      printf("before read, buffer: %s\n",content_buffer);
      #endif
      /*
      See aio_gist for how to use.
      mainly by EINPROGRESS.
      */
      if (aio_read(&aio_inst)==-1) {
        perror("read file init");
        return -1;
      }
      #ifdef USE_AIO_SUSPEND
      const struct aiocb * aiolist[1];
      aiolist[0] = &aio_inst;
      assert(aio_suspend(aiolist,1,NULL)!=-1);
      #elif USE_SIGNAL
      WAIT
      #else
      while ((err = aio_error (&aio_inst)) == EINPROGRESS){
        sleep(1);
      }
      #endif

      if ((err = aio_error(&aio_inst)) != 0) {
        perror("read file end");
        return -1;
      }

      if (aio_return(&aio_inst) ==-1) {
        perror("read file ret");
        return -1;
      }
      #ifdef AIO_DEBUG
      printf("after read, buffer: %s\n",content_buffer);
      #endif
      #else
      if (read(fd,content_buffer,MAXMSG)==-1) {
        perror ("read file");
        return -1;
      }
      #endif
      printf("finish read\n");
      assert(close(fd)!=-1);
      /*
      Notice: the file may contain the invisible character "\n"
      */
      #ifdef USE_AIO
      assert(send(filedes, content_buffer, strlen(content_buffer), 0)!=-1);
      #else
      assert(send(filedes, content_buffer, strlen(content_buffer), 0)!=-1);
      #endif
      printf("finish send\n");
      /*
      weird always stuck at if content_buffer is all 0:
      In file: /home/czg_arch/.cache/debuginfod_client/316d0d3666387f0e8fb98773f51aa1801027c5ab/source##usr##src##debug##glibc##glibc##misc##..##sysdeps##unix##sysv##linux##select.c
        68 #ifdef __ASSUME_TIME64_SYSCALLS
      ► 69   int r = SYSCALL_CANCEL (pselect6_time64, nfds, readfds, writefds, exceptfds,
        70                           pts64, NULL);
      maybe although send return, but it blocks the program.
      */
      #ifdef SIGNAL_CLEAR_FILE_CACHE
      // printf("inserted into %dth cache\n",file_cache_tail);
      printf("inserted into %dth cache with contents %s\n",file_cache_tail,content_buffer);
      strcpy(file_cache[file_cache_tail++].content, content_buffer);
      if (file_cache_tail==CACHE_NUM) {
        /*
        simple FIFO schedule.
        */
        file_cache_tail=0;
      }
      // printf("will return\n");
      /*
      if content_buffer all is 0, then printf won't print.
      so here `fflush` to print it always.
      */
      fflush(stdout);
      #endif
      return 0;
      end:
      fprintf(stderr, "file is forbidden\n");
      return -1;
    }
}

#ifdef SIGNAL_CLEAR_FILE_CACHE
void reset_cache(){
  printf("reset cache\n");
  for (int i=0; i<CACHE_NUM; i++) {
    memset(file_cache[i].file_name, 0, NAME_SIZE);
    memset(file_cache[i].content, 0, CACHE_SIZE);
  }
  file_cache_tail=0;
}
#endif

int
main (void)
{
  extern int make_socket (uint16_t port);
  int sock;
  fd_set active_fd_set, read_fd_set;
  int i;
  struct sockaddr_in clientname;
  size_t size;
  struct timeval time_of_day;
  /* https://linuxhint.com/gettimeofday_c_language/ */
  struct tm *info;
  char buffer[64];
  #ifdef SIGNAL_CLEAR_FILE_CACHE
  printf("pid: %d\n",getpid());
  reset_cache();
  // signal(SIGUSR1,reset_cache);
  struct sigaction new_action, old_action;
  new_action.sa_handler = reset_cache;
  sigemptyset (&new_action.sa_mask);
  /*
  https://www.gnu.org/software/libc/manual/html_node/Flags-for-Sigaction.html#index-SA_005fRESTART
  */
  new_action.sa_flags = SA_RESTART;
  sigaction (SIGUSR1, &new_action, &old_action);
  #endif
  sigset_t block_set;
  sigfillset(&block_set);
  sigdelset(&block_set, SIGINT);
  sigdelset(&block_set, SIGUSR1);

  /* Create the socket and set it up to accept connections. */
  sock = make_socket (PORT);
  if (listen (sock, 1) < 0)
    {
      perror ("listen");
      exit (EXIT_FAILURE);
    }

  /* Initialize the set of active sockets. */
  FD_ZERO (&active_fd_set);
  FD_SET (sock, &active_fd_set);

  while (1)
    {
      /* Block until input arrives on one or more active sockets. */
      read_fd_set = active_fd_set;
      /*
      1. The `FD_SETSIZE` implies "accept multiple connections"
      2. Also can use select with sigprocmask similar to csapp. https://stackoverflow.com/a/46047380/21294350
      3. TEMP_FAILURE_RETRY https://www.gnu.org/software/libc/manual/html_node/Flags-for-Sigaction.html#index-SA_005fRESTART
      */
      if (TEMP_FAILURE_RETRY(pselect (FD_SETSIZE, &read_fd_set, NULL, NULL, NULL,&block_set)) < 0)
        {
          perror ("select");
          #ifdef SIGNAL_CLEAR_FILE_CACHE
          /*
          1. inspired by https://stackoverflow.com/a/4959731/21294350 
          by https://stackoverflow.com/questions/4959524/when-to-check-for-eintr-and-repeat-the-function-call#comment105871242_59795677
          select/poll always has `EINTR` independent of whether SA_RESTART. 
          Also see `man pselect` "implementation‐defined"
          > it is implementation‐defined whether the function restarts or returns with [EINTR]

          example: https://unix.stackexchange.com/a/600353/568529
          */
          if (errno==EINTR) {
            printf("interrupted by some signal\n");
            exit (EXIT_FAILURE);
          }
          #endif
          exit (EXIT_FAILURE);
        }

      /* Service all the sockets with input pending. */
      for (i = 0; i < FD_SETSIZE; ++i)
        if (FD_ISSET (i, &read_fd_set))
          {
            if (i == sock)
              {
                /* Connection request on original socket. */
                int new;
                size = sizeof (clientname);
                /*
                The `new` corresponds to the new established connection fd by `connect`.
                */
                new = accept (sock,
                              (struct sockaddr *) &clientname,
                              (socklen_t *)&size);
                if (new < 0)
                  {
                    perror ("accept");
                    exit (EXIT_FAILURE);
                  }
                /*
                1. https://stackoverflow.com/a/1705899/21294350
                inet_ntop and  inet_ntoa both MT‐Safe locale, i.e. they are dependent on `locale` (https://stackoverflow.com/a/65590509/21294350)
                from `man attributes`, `const:locale` ensures they safe, so just use it as the normal MT‐Safe.
                2. why sin_port is different from PORT.
                See csapp "Figure 11.21"
                */
                fprintf (stderr,
                         "Server: connect from host %s, port %d converted from %d.\n",
                         inet_ntoa (clientname.sin_addr),
                         ntohs (clientname.sin_port),clientname.sin_port);
                FD_SET (new, &active_fd_set);
                /*
                send time responsing to the request.
                */
                gettimeofday( &time_of_day, NULL);
                info = localtime(&(time_of_day.tv_sec));
                strcpy(buffer, asctime (info));
                /*
                create a new socket 
                */
                // assert(send(new, buffer, strlen(buffer), 0)!=-1);
              }
            else
              {
                /*
                See csapp Figure 11.14, due to connect(...) in connection.h , a new fd is used to transfer data.
                */
                /* Data arriving on an already-connected socket. */
                if (read_from_client (i) < 0)
                  {
                    close (i);
                    FD_CLR (i, &active_fd_set);
                  }
              }
          }
    }
}
