#ifndef LIB_H
#define LIB_H
#define _POSIX_C_SOURCE 201112L    // getnameinfo >= 201112L, sem_timedwait >= 200112L, clock_gettime >= 199309L

#include <sys/types.h>     // some historical (BSD) implementations required it
#include <sys/socket.h>    // socket(), bind(), connect(), sendto(), recvfrom(), AF_UNSPEC, SOCK_DGRAM
#include <netdb.h>         // getaddrinfo(), getnameinfo(), AI_PASSIVE
#include <string.h>        // memset(), strlen()
#include <unistd.h>        // close()
#include <stdlib.h>        // exit(), EXIT_FAILURE
#include <stdio.h>         // printf(), fprintf(), perror
#include <string.h>        // strncpy()
#include <fcntl.h>         // For O_* constants
#include <sys/stat.h>      // For mode constants
#include <semaphore.h>
#include <time.h>          // timespec, clock_gettime
#include <errno.h>
#include <assert.h>

#define CLOSE_IN_SERVER
#define TEST_BIG_FILE
#ifdef TEST_BIG_FILE
#define BUFFER_SIZE 65507
#define SMALL_BUFFER_SIZE 5 // not smaller than 4 and strlen(END_STR) which make `strncmp(buffer, "ack", 4)` always failure.
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

int
UDP_Open(char *hostName, char *port, int server)
{
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
    hints.ai_flags     = AI_PASSIVE;    /* For wildcard IP address */
    hints.ai_protocol  = 0;             /* Any protocal */
    hints.ai_canonname = NULL;
    hints.ai_addr      = NULL;
    hints.ai_next      = NULL;

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
        else if (!server && connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1)
            break;                  /* Success */

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
#else
unsigned long wait_ack(int sfd, char *buffer, int nread, struct sockaddr *peer_addr, int peer_addr_len, int orig_ret);
unsigned long
UDP_Write(int sfd, char *buffer, int nread, struct sockaddr *peer_addr, int peer_addr_len)
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
#endif

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

ssize_t
UDP_Read(int sfd, char *buffer, struct sockaddr *peer_addr, socklen_t *peer_addr_len)
{
    memset(buffer, 0, BUFFER_SIZE);
    ssize_t recv_bytes = recvfrom(sfd, buffer, BUFFER_SIZE, 0, peer_addr, peer_addr_len);
    #ifdef TEST_BIG_FILE
    char tmp_buf[SMALL_BUFFER_SIZE];
    int recv_cnt=0;
    if (recv_bytes > 0) {
        if (strncmp(buffer, BEGIN_STR, strlen(BEGIN_STR)+1) == 0) {
            recv_bytes=0;
            memset(buffer, 0, BUFFER_SIZE);
            while (1) {
                printf("Send ack\n");
                UDP_Write(sfd, "ack", 4, peer_addr, *peer_addr_len);    // send ask   
                recv_cnt = recvfrom(sfd, tmp_buf, SMALL_BUFFER_SIZE, 0, peer_addr, peer_addr_len);
                if (strncmp(tmp_buf, END_STR, strlen(END_STR)+1) != 0){
                    recv_bytes+=recv_cnt;
                    printf("cat %s\n",tmp_buf);
                    strcat(buffer, tmp_buf);
                }else {
                    printf("end str\n");
                    break;
                }   
            }
            printf("Send ack\n");
            UDP_Write(sfd, "ack", 4, peer_addr, *peer_addr_len);    // send ask   
        }
        if (strncmp(buffer, "ack", 4) != 0) {
            printf("Send ack\n");
            UDP_Write(sfd, "ack", 4, peer_addr, *peer_addr_len);    // send ask
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