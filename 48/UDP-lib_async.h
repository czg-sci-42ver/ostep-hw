#ifndef LIB_H
#define LIB_H
/*
This will make strsep fail.
i.e. "call to undeclared function 'strsep'; ISO C99 and later do not support implicit function declarations" in clang
*/
// #define _POSIX_C_SOURCE 201112L    // getnameinfo >= 201112L, sem_timedwait >= 200112L, clock_gettime >= 199309L

#include <sys/types.h>     // some historical (BSD) implementations required it
#include <sys/socket.h>    // socket(), bind(), connect(), sendto(), recvfrom(), AF_UNSPEC, SOCK_DGRAM
#include <netdb.h>         // getaddrinfo(), getnameinfo(), AI_PASSIVE
#include <unistd.h>        // close()
#include <stdlib.h>        // exit(), EXIT_FAILURE
#include <stdio.h>         // printf(), fprintf(), perror
// #include <string.h>        // strncpy()
#include <fcntl.h>         // For O_* constants
#include <sys/stat.h>      // For mode constants
#include <semaphore.h>
#include <time.h>          // timespec, clock_gettime
#include <errno.h>
#include <assert.h>
#include <string.h>        // memset(), strlen()

#define ASYNC
#ifdef ASYNC
#define ASYNC_SEQ
#include <liburing.h>
struct io_uring ring;
#define LISTEN_BACKLOG 2
#define handle_error(msg)                                                      \
  do {                                                                         \
    perror(msg);                                                               \
    exit(EXIT_FAILURE);                                                        \
  } while (0)
#endif

#ifdef ASYNC_SEQ
#define NUM_BUFFER_SIZE 4
#define CONTENT_SIZE (SMALL_BUFFER_SIZE-NUM_BUFFER_SIZE)
#endif

#define CLOSE_IN_SERVER
#define TEST_BIG_FILE
#ifdef TEST_BIG_FILE
#define BUFFER_SIZE 65507
#define SMALL_BUFFER_SIZE 10 // not smaller than 4 and strlen(END_STR) which make `strncmp(buffer, "ack", 4)` always failure.
char BEGIN_STR[10]="0begin";
char END_STR[10]="end1";
#else
#define BUFFER_SIZE 65507
#endif
#define TIMEOUT_SECONDS 10

char * client_sem_name = "/client_sem";
sem_t * client_sem;
struct timespec ts;
#define SEND_BEFORE_TIMER

#ifdef ASYNC
int seq_cnt;
int server_seq_cnt;
char send_restore_buff[LISTEN_BACKLOG][SMALL_BUFFER_SIZE*2];
struct user_data {
  char buf[BUFSIZ];
  int socket_fd;
  int file_fd;
  int index;
  int io_op;
  struct sockaddr * addr;
  socklen_t addrlen;
};

struct user_data data_arr[LISTEN_BACKLOG];
void prep_connect(struct io_uring *ring, int sfd,struct sockaddr * addr, socklen_t addrlen) {
  struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
  if (sqe == NULL)
    handle_error("io_uring_get_sqe");
  /*
  maybe not needed
  */
  data_arr[seq_cnt].socket_fd=sfd;
  data_arr[seq_cnt].addr=addr;
  data_arr[seq_cnt].addrlen=addrlen;

  io_uring_prep_connect(sqe, sfd, addr, addrlen);
  data_arr[seq_cnt].io_op = IORING_OP_CONNECT;
  io_uring_sqe_set_data(sqe, &data_arr[seq_cnt]);
  if (io_uring_submit(ring) < 0)
    handle_error("io_uring_submit");
}
#endif

int
UDP_Open(char *hostName, char *port, int server)
{
    #ifdef ASYNC_SEQ
    assert(CONTENT_SIZE>0);
    #endif
    #ifdef ASYNC
    memset(send_restore_buff, 0, LISTEN_BACKLOG*sizeof(char)*SMALL_BUFFER_SIZE*2);
    /*
    only the client init.
    */
    seq_cnt=0;
    server_seq_cnt=0;
    if (!server && io_uring_queue_init(LISTEN_BACKLOG, &ring, IORING_SETUP_SQPOLL))
        handle_error("io_uring_queue_init"); 
    #endif
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    int sfd, s;
    
    memset(&hints, 0, sizeof(struct addrinfo));
    /*
    same as the `man 3 getaddrinfo` example.
    */
    hints.ai_family    = AF_UNSPEC;     /* Allow IPv4 or Ipv6 */
    /* Datagram socket (connectionless, unreliable 
       messages of a fixed maximum length) */
    hints.ai_socktype  = SOCK_DGRAM;
    if (server) {
        hints.ai_flags     = AI_PASSIVE;    /* For wildcard IP address */
        hints.ai_protocol  = 0;             /* Any protocal */
        hints.ai_canonname = NULL;
        hints.ai_addr      = NULL;
        hints.ai_next      = NULL;
    }else {
        hints.ai_flags = 0;
        hints.ai_protocol = 0;          /* Any protocol */
    }
    

    s = getaddrinfo(hostName, port, &hints, &result);
    if (s != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
        exit(EXIT_FAILURE);
    }

    /* getaddrinfo() returns a list of address structures.
       Try each address until we successfully bind(2) for 
       server or connect(2) for client. If socket(2) (or 
       bind(2) or connect(2)) fails, we (close the socket
       and) try the next address. */

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        sfd = socket(rp->ai_family, rp->ai_socktype,
                rp->ai_protocol);
        if (sfd == -1)
            continue;

        if (server && bind(sfd, rp->ai_addr, rp->ai_addrlen) == 0)
            break;                  /* Success */
        #ifdef ASYNC
        else if (!server){
            prep_connect(&ring, sfd,rp->ai_addr,rp->ai_addrlen);
            break;                  /* Success */
        }
        #else
        else if (!server && connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1)
            break;                  /* Success */
        #endif

        close(sfd);
    }

    if (rp == NULL) {               /* No address succeeded */
        char *error_message = server ? "Could not bind" : "Could not connect";
        fprintf(stderr, "%s\n", error_message);
        exit(EXIT_FAILURE);
    }

    freeaddrinfo(result);           /* No longer needed */
    #ifdef LOG_CLIENT_SEM
    printf("before init client_sem ptr:%p which is at %p\n",client_sem,&client_sem);
    #endif
    #ifdef SEND_BEFORE_TIMER
    /*
    `sem_open` default is shared between processes 
    https://stackoverflow.com/a/16893318/21294350 and https://blog.superpat.com/semaphores-on-linux-sem_init-vs-sem_open
    */
    #ifdef CLOSE_IN_SERVER
    if (server) {
        client_sem = sem_open(client_sem_name, O_CREAT, S_IRWXU, 0);
    }else {
        /*
        https://stackoverflow.com/a/8359403/21294350
        */
        client_sem = sem_open(client_sem_name, 0);
    }
    #else
    client_sem = sem_open(client_sem_name, O_CREAT, S_IRWXU, 0);
    #endif
    if (client_sem == SEM_FAILED) {
    #else
    if (!server && (client_sem = sem_open(client_sem_name, O_CREAT, S_IRWXU, 1)) == SEM_FAILED) {
    #endif
        perror("sem_open");
        exit(EXIT_FAILURE);
    }
    #ifdef LOG_CLIENT_SEM
    printf("init client_sem ptr:%p\n",client_sem);
    #endif

    return sfd;
}

#ifndef SEND_BEFORE_TIMER
ssize_t
UDP_Write(int sfd, char *buffer, int nread, struct sockaddr *peer_addr, int peer_addr_len)
{
    if (nread > BUFFER_SIZE) {
        fprintf(stderr, "Exceed max buffer size\n");
        exit(EXIT_FAILURE);
    }
    
    int s = 0;
    
    if (clock_gettime(CLOCK_REALTIME, &ts) == -1) {
        perror("clock_gettime");
        exit(EXIT_FAILURE);
    }
    ts.tv_sec += TIMEOUT_SECONDS;
    // wait ack
    errno = 0;
    int isAsk = 0;
    if ((isAsk = strncmp(buffer, "ack", 4)) != 0)
        /*
        avoid write collision and ensure the sequential writes.
        */
        s = sem_timedwait(client_sem, &ts);

    if (s == -1) {
        if (errno == ETIMEDOUT) {
            // retry
            return UDP_Write(sfd, buffer, nread, peer_addr, peer_addr_len);
        } else {
            perror("sem_timedwait");
            exit(EXIT_FAILURE);
        }
    } else {
        if (isAsk != 0)
            /*
            since receiver success, it is ok to send the next message.
            */
            sem_post(client_sem);
        /*
        sendto atomic, so it is ok to post before which allow overlap sendto.
        https://stackoverflow.com/a/1981439/21294350
        > in the case of multiple sends, the second will likely block until the first completes
        */
        return sendto(sfd, buffer, nread, 0, peer_addr, peer_addr_len);
    }
}
/*
https://gcc.gnu.org/onlinedocs/cpp/Defined.html
*/
#else
#if defined(ASYNC)
void prep_send_to(struct io_uring *ring, int* index,const char *message, size_t length);
#ifdef ASYNC_SEQ
void prep_send_to_suffix_num(struct io_uring *ring, int* index,const char *message, size_t length);
#endif
#endif

unsigned long wait_ack(int sfd, char *buffer, int nread, struct sockaddr *peer_addr, int peer_addr_len, int orig_ret);
void wait_ack_client(int sfd, char *buffer);

unsigned long
UDP_Write_server(int sfd, char *buffer, int nread, struct sockaddr *peer_addr, int peer_addr_len)
{
    #ifdef TEST_BIG_FILE
    int ret=0;
    if (nread>SMALL_BUFFER_SIZE) {
        sendto(sfd, BEGIN_STR, strlen(BEGIN_STR), 0, peer_addr, peer_addr_len);
        wait_ack(sfd, BEGIN_STR, strlen(BEGIN_STR), peer_addr, peer_addr_len, ret);
        int loop=nread/SMALL_BUFFER_SIZE;
        for (int i=0; i<loop; i++) {
            ret+=sendto(sfd, buffer, SMALL_BUFFER_SIZE, 0, peer_addr, peer_addr_len);
            ret = wait_ack(sfd, buffer, SMALL_BUFFER_SIZE, peer_addr, peer_addr_len, ret);
            buffer+=SMALL_BUFFER_SIZE;
        }
        /*
        recvfrom return count depends on the `sendto`, so here set the exact number.
        */
        int left_cnt =nread-ret;
        #ifdef LOG_SEND
        printf("to send %s",buffer);
        #endif
        /*
        here +1 is to send ending '\0'
        */
        ret+=sendto(sfd, buffer, left_cnt+1, 0, peer_addr, peer_addr_len);
        ret = wait_ack(sfd, buffer, left_cnt+1, peer_addr, peer_addr_len, ret);
        #ifdef LOG_SEND
        printf("will send END_STR\n");
        #endif
        sendto(sfd, END_STR, strlen(END_STR)+1, 0, peer_addr, peer_addr_len);
        wait_ack(sfd, END_STR, strlen(END_STR)+1, peer_addr, peer_addr_len, ret);
        #ifdef LOG_WRITE
        printf("write %d bytes when nread %d\n",ret,nread);
        #endif
        return ret>nread?nread:ret;
    } else {
        int ret=sendto(sfd, buffer, nread, 0, peer_addr, peer_addr_len);
        ret = wait_ack(sfd, buffer, nread, peer_addr, peer_addr_len, ret);
        return ret;
    }
    #else
    if (nread > BUFFER_SIZE) {
        fprintf(stderr, "Exceed max buffer size\n");
        exit(EXIT_FAILURE);
    }
    /*
    here assume `sendto` MT-safe by https://stackoverflow.com/a/1981439/21294350
    */
    int ret=sendto(sfd, buffer, nread, 0, peer_addr, peer_addr_len);
    ret = wait_ack(sfd, buffer, nread, peer_addr, peer_addr_len, ret);
    return ret;
    #endif    
}

unsigned long
UDP_Write(int sfd, char *buffer, int nread, struct sockaddr *peer_addr, int peer_addr_len)
{
    #ifdef TEST_BIG_FILE
    #ifdef ASYNC
    struct io_uring_cqe *cqe;
    struct user_data *data;
    #endif
    int ret=0,reach_end=0;
    printf("read %d bytes\n",nread);
    fflush(stdout);
    if (nread>SMALL_BUFFER_SIZE) {
        printf("transmit the message in pieces\n");
        #ifdef ASYNC
        int bound=nread/CONTENT_SIZE*CONTENT_SIZE;
        while (1) {
            if (io_uring_wait_cqe(&ring, &cqe))
                handle_error("io_uring_wait_cqe");
            if (cqe->res < 0) {
                fprintf(stderr, "I/O error: %s\n", strerror(-cqe->res));
                exit(EXIT_FAILURE);
            }
            data = (struct user_data *)io_uring_cqe_get_data(cqe);
            switch (data->io_op) {
                case IORING_OP_CONNECT:
                    /*
                    limit print str len
                    https://stackoverflow.com/a/63059241/21294350
                    https://stackoverflow.com/a/2239571/21294350
                    */
                    printf("send %.*s\n",(int)strlen(BEGIN_STR),BEGIN_STR);
                    fflush(stdout);
                    #ifdef ASYNC_SEQ
                    prep_send_to_suffix_num(&ring, &seq_cnt,BEGIN_STR,strlen(BEGIN_STR));
                    #else
                    prep_send_to(&ring, &seq_cnt,BEGIN_STR,strlen(BEGIN_STR)+1);
                    #endif
                    break;
                case IORING_OP_SEND:
                    /*
                    here not use cqe->res because it will contain the suffix length.
                    */
                    if (ret==bound) {
                        printf("send last str %.*s\n",CONTENT_SIZE,buffer);
                        fflush(stdout);
                        #ifdef ASYNC_SEQ
                        prep_send_to_suffix_num(&ring, &seq_cnt,buffer, nread-ret);
                        #else
                        prep_send_to(&ring, seq_cnt,buffer, nread-ret+1);
                        #endif
                    }else if (ret>bound) {
                        #ifdef ASYNC_SEQ
                        prep_send_to_suffix_num(&ring, &seq_cnt,END_STR, strlen(END_STR));
                        #else
                        prep_send_to(&ring, &seq_cnt,BEGIN_STR,strlen(BEGIN_STR)+1);
                        #endif
                        reach_end=1;
                    }else {
                        printf("send %dth str:%.*s\n",seq_cnt,CONTENT_SIZE,buffer);
                        fflush(stdout);
                        #ifdef ASYNC_SEQ
                        prep_send_to_suffix_num(&ring, &seq_cnt,buffer, CONTENT_SIZE);
                        #else
                        prep_send_to(&ring, seq_cnt,buffer, SMALL_BUFFER_SIZE);
                        #endif
                        buffer+=CONTENT_SIZE;
                        printf("after buffer forward %.*s\n",CONTENT_SIZE,buffer);
                    }
                    ret+=CONTENT_SIZE;
                    break;
                default:
                    printf("should not run this\n");
            }
            if (reach_end) {
                break;
            }
            
            #ifdef LOG_SEND
            printf("to send %s",buffer);
            #endif
            
            #ifdef LOG_SEND
            printf("will send END_STR\n");
            #endif
            
            #ifdef LOG_WRITE
            printf("write %d bytes when nread %d\n",ret,nread);
            #endif
            /*
            empty one slot of LISTEN_BACKLOG ones in the uring.
            */
            io_uring_cqe_seen(&ring, cqe);
        }
        return ret>nread?nread:ret;
    } else {
        int ret=sendto(sfd, buffer, nread, 0, peer_addr, peer_addr_len);
        ret = wait_ack(sfd, buffer, nread, peer_addr, peer_addr_len, ret);
        return ret;
    }
    #else
    if (nread > BUFFER_SIZE) {
        fprintf(stderr, "Exceed max buffer size\n");
        exit(EXIT_FAILURE);
    }
    /*
    here assume `sendto` MT-safe by https://stackoverflow.com/a/1981439/21294350
    */
    int ret=sendto(sfd, buffer, nread, 0, peer_addr, peer_addr_len);
    ret = wait_ack(sfd, buffer, nread, peer_addr, peer_addr_len, ret);
    return ret;
    #endif    
}
#endif

#if defined(ASYNC)
void prep_send_to(struct io_uring *ring, int* index,const char *message, size_t length) {
  int local_index=*index;
  struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
  if (sqe == NULL)
    handle_error("io_uring_get_sqe");
//   if (strncmp(message, END_STR, strlen(END_STR))) {
//     data_arr[local_index].io_op = IORING_OP_SEND; 
//   }else {
//     data_arr[local_index].io_op = IORING_OP_RECV;
//   }
  data_arr[local_index].io_op = IORING_OP_SEND;
  if (strncmp(message, "ack", 3)) {
    strncpy(send_restore_buff[local_index], message,strlen(message)+1);
  }
  printf("prep_send_to send %s with len %ld\n",message,length);
  io_uring_prep_sendto(sqe, data_arr[0].socket_fd, message,
                     length, 0,data_arr[0].addr,data_arr[0].addrlen);
  io_uring_sqe_set_data(sqe, &data_arr[local_index]);
  if (io_uring_submit(ring) < 0)
    handle_error("io_uring_submit");
  local_index++;
  if (local_index==LISTEN_BACKLOG) {
    fprintf(stderr,"too many requests\n");
    local_index=0;
    /*
    TODO ack all requests here
    */
    char fake_str[SMALL_BUFFER_SIZE]={0};
    /*
    here nread and orig_ret is not used.
    */
    wait_ack_client(data_arr[0].socket_fd, fake_str);
  }
  *index=local_index;
}
#endif

#ifdef ASYNC_SEQ
void prep_send_to_suffix_num(struct io_uring *ring, int* index,const char *message, size_t length){
    char tmp_buf[SMALL_BUFFER_SIZE*2]={0};
    char num_str[5]={0};
    strncpy(tmp_buf, message,length);
    sprintf(num_str, "_%d", *index);
    strncat(tmp_buf, num_str,strlen(num_str)+1);
    if(strlen(tmp_buf)+1>SMALL_BUFFER_SIZE){
        printf("msg:%s index:%d\n",message,*index);
        exit(1);
    }
    printf("to send tmp_buf %s\n",tmp_buf);
    prep_send_to(ring, index,tmp_buf, strlen(tmp_buf)+1);
}
typedef struct message_num{
    // char num_str[SMALL_BUFFER_SIZE];
    // char message_str[SMALL_BUFFER_SIZE];
    char *num_str;
    char *message_str;
} Message_num;
int check_server_seq(char *msg, Message_num *msg_struct,char *ack_str){
    msg_struct->num_str=msg;
    assert((msg_struct->message_str=strsep(&(msg_struct->num_str),"_"))!=NULL);
    /*
    See `man sprintf` 'CAVEATS' where src,dst should not overlap.
    */
    char *num_str=msg_struct->num_str;
    if (atoi(num_str)!=server_seq_cnt) {
        printf("order error\n");
        return -1;
    }
    // sprintf(suffix_str, "_%s", buffer);
    num_str-=1; // to inlcude '_'
    printf("append numstr:%s\n",num_str);
    strncat(ack_str, num_str,strlen(num_str)+1);
    return 0;
}
#endif
#endif

void check_cqe(struct io_uring_cqe *cqe){
    if (io_uring_wait_cqe(&ring, &cqe))
        handle_error("io_uring_wait_cqe");
    if (cqe->res < 0) {
        fprintf(stderr, "I/O error: %s\n", strerror(-cqe->res));
        exit(EXIT_FAILURE);
    }
}

unsigned long wait_ack(int sfd, char *buffer, int nread, struct sockaddr *peer_addr, int peer_addr_len, int orig_ret){
    int s = 0;
    if (clock_gettime(CLOCK_REALTIME, &ts) == -1) {
        perror("clock_gettime");
        exit(EXIT_FAILURE);
    }
    struct timespec target_ts=ts;
    assert(target_ts.tv_sec==ts.tv_sec);
    target_ts.tv_sec += TIMEOUT_SECONDS;
    // wait ack
    errno = 0;
    int isAsk = 0;
    #ifdef LOG_CLIENT_SEM
    printf("with client_sem ptr:%p\n",client_sem);
    #endif
    if ((isAsk = strncmp(buffer, "ack", 4)) != 0){
        s = sem_timedwait(client_sem, &target_ts);
        struct timespec end_ts;
        if (clock_gettime(CLOCK_REALTIME, &end_ts) == -1) {
            perror("clock_gettime");
            exit(EXIT_FAILURE);
        }
        printf("wait for %ld s\n",end_ts.tv_sec-ts.tv_sec);
    }

    if (s == -1) {
        if (errno == ETIMEDOUT) {
            // retry
            return UDP_Write(sfd, buffer, nread, peer_addr, peer_addr_len);
        } else {
            perror("sem_timedwait");
            exit(EXIT_FAILURE);
        }
    } else {
        if (isAsk == 0){
            printf("post %p\n",client_sem);
            /*
            since receiver success, it is ok to send the next message.
            */
            sem_post(client_sem);
        }
        return orig_ret;
    }
}

void wait_ack_client(int sfd, char *buffer){
    if (clock_gettime(CLOCK_REALTIME, &ts) == -1) {
        perror("clock_gettime");
        exit(EXIT_FAILURE);
    }
    struct timespec target_ts=ts;
    assert(target_ts.tv_sec==ts.tv_sec);
    target_ts.tv_sec += TIMEOUT_SECONDS;
    // wait ack
    errno = 0;
    int isAsk = 0;
    #ifdef LOG_CLIENT_SEM
    printf("with client_sem ptr:%p\n",client_sem);
    #endif
    #ifdef ASYNC_SEQ
    char tmp_buf[SMALL_BUFFER_SIZE],num_str[SMALL_BUFFER_SIZE],*num_ptr;
    int seq_ack_cnt=0,resend=0;
    struct io_uring_cqe *cqe=NULL;

    if ((isAsk = strncmp(buffer, "ack", 4)) != 0){
        while (1) {
            if (resend) {
                check_cqe(cqe);
            }
            /*
            same as UDP_Read.
            */
            printf("wait for ack\n");
            recvfrom(sfd, tmp_buf, SMALL_BUFFER_SIZE, 0, NULL,0);
            printf("ack %s\n",tmp_buf);
            num_ptr=tmp_buf;
            assert(strsep(&num_ptr,"_")!=(char *)NULL);
            sprintf(num_str, "%d", seq_ack_cnt);
            if (strncmp(num_str, num_ptr, strlen(num_ptr)+1)) {
                printf("resend %s\n",send_restore_buff[seq_ack_cnt]);
                resend=1;
                prep_send_to(&ring, &seq_cnt, send_restore_buff[seq_ack_cnt], strlen(send_restore_buff[seq_ack_cnt])+1);
            }
            seq_ack_cnt++;
        }
    }
    #else
    if ((isAsk = strncmp(buffer, "ack", 4)) != 0){
        s = sem_timedwait(client_sem, &target_ts);
        struct timespec end_ts;
        if (clock_gettime(CLOCK_REALTIME, &end_ts) == -1) {
            perror("clock_gettime");
            exit(EXIT_FAILURE);
        }
        printf("wait for %ld s\n",end_ts.tv_sec-ts.tv_sec);
    }
    if (s == -1) {
        if (errno == ETIMEDOUT) {
            // retry
            return UDP_Write(sfd, buffer, nread, peer_addr, peer_addr_len);
        } else {
            perror("sem_timedwait");
            exit(EXIT_FAILURE);
        }
    } else {
        if (isAsk == 0){
            printf("post %p\n",client_sem);
            /*
            since receiver success, it is ok to send the next message.
            */
            sem_post(client_sem);
        }
        return orig_ret;
    }
    #endif
}
int send_ack(char *ack_str,char *tmp_buf,Message_num* tmp_msg,int sfd, struct sockaddr *peer_addr, socklen_t *peer_addr_len,ssize_t *recv_bytes){
    #ifdef ASYNC_SEQ
    strncpy(ack_str, "ack", 4);
    if (check_server_seq(tmp_buf,tmp_msg,ack_str)) {
        *recv_bytes = recvfrom(sfd, tmp_buf, SMALL_BUFFER_SIZE, 0, peer_addr, peer_addr_len);
        return -1;
    }
    #endif
    printf("Send ack\n");
    #ifdef ASYNC_SEQ
    UDP_Write_server(sfd, ack_str, strlen(ack_str)+1, peer_addr, *peer_addr_len);  
    #else
    UDP_Write(sfd, "ack", 4, peer_addr, *peer_addr_len);    // send ask
    #endif
    return 0;
}

ssize_t
UDP_Read(int sfd, char *buffer, struct sockaddr *peer_addr, socklen_t *peer_addr_len)
{
    memset(buffer, 0, BUFFER_SIZE);
    #ifdef TEST_BIG_FILE
    char tmp_buf[SMALL_BUFFER_SIZE];
    #endif
    // ssize_t recv_bytes = recvfrom(sfd, buffer, BUFFER_SIZE, 0, peer_addr, peer_addr_len);
    ssize_t recv_bytes = recvfrom(sfd, tmp_buf, SMALL_BUFFER_SIZE, 0, peer_addr, peer_addr_len);
    printf("read tmp_buf %s\n",tmp_buf);
    #ifdef TEST_BIG_FILE
    #ifdef ASYNC
    // char suffix_str[SMALL_BUFFER_SIZE],msg_buf[SMALL_BUFFER_SIZE]={0};
    char ack_str[SMALL_BUFFER_SIZE];
    int has_content=0;
    #endif
    int recv_cnt=0;
    Message_num tmp_msg;
    /*
    the left write should only occur in the server.
    */
    if (recv_bytes > 0) {
        #ifdef ASYNC_SEQ
        if (strncmp(tmp_buf, BEGIN_STR, strlen(BEGIN_STR)) == 0) {
        #else
        if (strncmp(buffer, BEGIN_STR, strlen(BEGIN_STR)+1) == 0) {
        #endif
            recv_bytes=0;
            while (1) {
                printf("read begin_str %s\n",tmp_buf);
                fflush(stdout);
                printf("Send ack\n");
                #ifdef ASYNC_SEQ
                strncpy(ack_str, "ack", 4);
                if (check_server_seq(tmp_buf,&tmp_msg,ack_str)) {
                    recv_bytes = recvfrom(sfd, tmp_buf, SMALL_BUFFER_SIZE, 0, peer_addr, peer_addr_len);
                    return 0;
                }
                if (has_content) {
                    printf("cat %s\n",tmp_msg.message_str);
                    strcat(buffer, tmp_msg.message_str);
                }
                printf("send %s\n",ack_str);
                UDP_Write_server(sfd, ack_str, strlen(ack_str)+1, peer_addr, *peer_addr_len);    // send ask
                #else
                UDP_Write(sfd, "ack", 4, peer_addr, *peer_addr_len);    // send ask   
                #endif
                recv_cnt = recvfrom(sfd, tmp_buf, SMALL_BUFFER_SIZE, 0, peer_addr, peer_addr_len);
                #ifdef ASYNC_SEQ
                if (strncmp(tmp_buf, END_STR, strlen(END_STR)) != 0){
                #else
                if (strncmp(tmp_buf, END_STR, strlen(END_STR)+1) != 0){
                #endif
                    recv_bytes+=recv_cnt;
                    #ifdef ASYNC_SEQ
                    has_content=1;
                    #else
                    printf("cat %s\n",tmp_buf);
                    strcat(buffer, tmp_buf);
                    #endif
                }else {
                    #ifdef ASYNC_SEQ
                    has_content=0;
                    #endif
                    printf("end str\n");
                    break;
                }   
            }
            #ifdef ASYNC_SEQ
            strncpy(ack_str, "ack", 4);
            if (check_server_seq(tmp_buf,&tmp_msg,ack_str)) {
                recv_bytes = recvfrom(sfd, tmp_buf, SMALL_BUFFER_SIZE, 0, peer_addr, peer_addr_len);
                return 0;
            }
            #endif
            printf("Send ack\n");
            #ifdef ASYNC_SEQ
            UDP_Write_server(sfd, ack_str, strlen(ack_str)+1, peer_addr, *peer_addr_len);  
            #else
            UDP_Write(sfd, "ack", 4, peer_addr, *peer_addr_len);    // send ask
            #endif
        }else if (strncmp(tmp_buf, "ack", 4) != 0) {
            printf("not read begin_str; tmp_buf %s\n",tmp_buf);
            if (send_ack(ack_str,tmp_buf,&tmp_msg,sfd, peer_addr, peer_addr_len,&recv_bytes)) {
                return 0;
            }
        }
    }
    #else
    if (recv_bytes > 0 && strncmp(buffer, "ack", 4) != 0) {
        printf("Send ack\n");
        UDP_Write(sfd, "ack", 4, peer_addr, *peer_addr_len);    // send ask
    }
    #endif
    return recv_bytes;
}

void
closeSem() {
    if (sem_close(client_sem) != 0) {
        perror("sem_close");
        exit(EXIT_FAILURE);
    }
    if (sem_unlink(client_sem_name) != 0) {
        perror("sem_close");
        exit(EXIT_FAILURE);
    }
}
#endif