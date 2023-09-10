/*
Compared with Figure 31.6
*/
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include "common_threads.h"

sem_t * s; 
// #define USE_INIT
// #define USE_INIT_POINTER
#define USE_FORK

void *child(void *arg) {
    sleep(1);
    printf("child\n");
    // use semaphore here
    Sem_post(s);
    return NULL;
}

int main(int argc, char *argv[]) {
    #ifndef USE_FORK
    pthread_t p;
    #endif
    #ifndef USE_INIT
    char * semaphoreName = "/fork-join-semaphore";
    #endif
    printf("parent: begin\n");
    // init semaphore here
    #ifdef USE_INIT
    #ifdef USE_INIT_POINTER
    sem_t* ss;
    Sem_init(ss, 0, 0);
    s =ss;
    #else
    sem_t ss;
    Sem_init(&ss, 0, 0);
    s =&ss;
    #endif
    #else
    s = Sem_open(semaphoreName, 0);
    #endif
    #ifdef USE_FORK
    int rc = fork();
    if (rc < 0) {
        // fork failed; exit
        fprintf(stderr, "fork failed\n");
        exit(1);
    } else if (rc == 0) { // child (new process)
        child(&rc);
    } else {
    #else
    Pthread_create(&p, NULL, child, NULL);
    #endif
    printf("begin wait\n");
    // use semaphore here
    Sem_wait(s);
    #ifndef USE_INIT
    Sem_close(s);
    Sem_unlink(semaphoreName);
    #endif
    printf("parent: end\n");
    #ifdef USE_FORK
    }
    #endif
    return 0;
}

