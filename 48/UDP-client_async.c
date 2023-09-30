#include "UDP-lib_async.h"

// #define LOG_SIZE

int
main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s host\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int sfd = UDP_Open(argv[1], "10000", 0);
    ssize_t nread;
    char buf[BUFFER_SIZE] = "first hello";
    #ifdef LOG_SIZE
    printf("sizeof(struct sockaddr_in6):%ld sizeof(struct sockaddr_storage):%ld\n",sizeof(struct sockaddr_in6),sizeof(struct sockaddr_storage));
    #endif

    for(size_t i = 0; i < 2; i++) {
        printf("Send: %s\n", buf);
        if (UDP_Write(sfd, buf, strlen(buf), NULL, 0) != strlen(buf)) {
            fprintf(stderr, "partial/failed write\n");
            exit(EXIT_FAILURE);
        }
        /*
        block until the server sends 'ack'
        */
        nread = UDP_Read(sfd, buf, NULL, 0);
        if (nread == -1) {
            perror("read");
            exit(EXIT_FAILURE);
        }

        printf("Received %zd bytes: %s\n", nread, buf);
        strncpy(buf, "hello hello", 12);
    }

    close(sfd);
    /*
    close in the server otherwise server can't modify the right named sem 
    by using the old sem which is stored in its memory space. (This can be seen with the `gdb` and `ls /dev/shm` help)
    */
    #ifndef CLOSE_IN_SERVER
    closeSem();
    #endif
    exit(EXIT_SUCCESS);
}
