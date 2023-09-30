#define _GNU_SOURCE         // NI_MAXHOST, NI_MAXSERV
#define _DARWIN_C_SOURCE    // NI_MAXHOST, NI_MAXSERV

#include "UDP-lib_async.h"
#include <assert.h>
#include <signal.h>


void free_sem(){
    printf("free\n");
    closeSem();
    exit(0);
}

int
main(int argc, char *argv[])
{
    struct sockaddr_storage peer_addr;
    socklen_t peer_addr_len;
    ssize_t nread;    // signed size
    char buf[BUFFER_SIZE];
    int sfd = UDP_Open(NULL, "10000", 1), s;
    assert(sfd > -1);

    #ifdef CLOSE_IN_SERVER
    /* Read datagrams and echo them back to sender */
    signal(SIGTERM, free_sem);
    signal(SIGINT, free_sem);
    #endif

    for (;;) {
        peer_addr_len = sizeof(struct sockaddr_storage);
        nread = UDP_Read(sfd, buf, (struct sockaddr *) &peer_addr, &peer_addr_len);
        if (nread == -1)
            continue;               /* Ignore failed request */

        char host[NI_MAXHOST], service[NI_MAXSERV];

        s = getnameinfo((struct sockaddr *) &peer_addr,
                        peer_addr_len, host, NI_MAXHOST,
                        service, NI_MAXSERV, NI_NUMERICSERV);
        if (s == 0)
            printf("Received %zd bytes from %s:%s: %s\n",
                    nread, host, service, buf);
        else
            fprintf(stderr, "getnameinfo: %s\n", gai_strerror(s));
    }
}