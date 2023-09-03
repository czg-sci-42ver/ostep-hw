#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sched.h>
#include <x86intrin.h>

#define USE_RDTSC
/*
https://stackoverflow.com/a/13772771/21294350 from https://stackoverflow.com/posts/8602336/timeline#history_d09d162d-de80-4827-a640-5e5fd96079d7
This is also the example of gcc doc https://gcc.gnu.org/onlinedocs/gcc/Machine-Constraints.html.
*/
#include <stdint.h>
/*
https://stackoverflow.com/a/48394316/21294350 and [Benchmark_IA_64]
`gcc -I /usr/lib/modules/6.4.12-arch1-1/build/arch/x86/include /usr/lib/modules/6.4.12-arch1-1/build/include 1.c -o 1.out`
can't be used when
```bash
$ sudo find / -name "preempt.h"                                                                                       
find: ‘/proc/1419/task/1419/net’: Invalid argument
find: ‘/proc/1419/net’: Invalid argument
/usr/lib/modules/6.4.12-arch1-1/build/arch/x86/include/asm/preempt.h
/usr/lib/modules/6.4.12-arch1-1/build/include/asm-generic/preempt.h
/usr/lib/modules/6.4.12-arch1-1/build/include/linux/preempt.h
...
```
*/
// #include <linux/preempt.h>
// #include <linux/irqflags.h>
static inline uint64_t rdtsc(){
    unsigned int lo,hi;
    /*
    1. if only check the execution time but not effects of "loads and stores", no need `sfence`
    2. Also see C29 description and https://stackoverflow.com/a/51907627/21294350.
    3. notice the overheads of fence and rdtsc, if just comparison, we can ignore the overheads
    4. compared with [Benchmark_IA_64] 3.2.2, here we will take the ending _mm_lfence() in the 1st call of 
    `rdtsc` and the starting _mm_lfence() in the 2rd call of `rdtsc` in account, 
    but it will keep "the variance of variances and the variance of the minimum values" 0
    because it avoids reorder -> no dynamic measure.
        So it can conform the low value in 3.3.1
    */
    // preempt_disable();
    _mm_lfence();
    __asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi));
    _mm_lfence();
    return ((uint64_t)hi << 32) | lo;
}

int
main(int argc, char *argv[]) {
    // measure system call
    int fd = open("./1.txt", O_CREAT | O_WRONLY | O_TRUNC, S_IRWXU), nloops = 1000000;

    /*
    This should be moved into "#else" if the 2rd `gettimeofday` is modified too.
    */
    struct timeval start, end;
    #ifdef USE_RDTSC
    for (int i=0;i<4;i++)
        rdtsc(); // warm-up as [Benchmark_IA_64] does.
    uint64_t start_cycle=rdtsc(),end_cycle;
    #else
    gettimeofday(&start, NULL);
    #endif
    for (size_t i = 0; i < nloops; i++) {
        read(fd, NULL, 0);
    }
    #ifdef USE_RDTSC
    end_cycle = rdtsc();
    #else
    gettimeofday(&end, NULL);
    #endif
    /*
    This implies the book:
    > Measure back-to-back calls to gettimeofday() to learn something about how precise the timer re-ally is;
    same as https://stackoverflow.com/a/69099861/21294350 .
    */
    #ifdef USE_RDTSC
    printf("system call: %lf cycles\n",(double)(end_cycle-start_cycle)/nloops);
    #else
    printf("system call: %f microseconds\n\n", (float) (end.tv_sec * 1000000 + end.tv_usec - start.tv_sec * 1000000 - start.tv_usec) / nloops);
    #endif
    close(fd);

    // measure context switch
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(0, &set);

    int first_pipefd[2], second_pipefd[2];
    if (pipe(first_pipefd) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }
    if (pipe(second_pipefd) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    pid_t cpid = fork();

    if (cpid == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    } else if (cpid == 0) {    // child
        if (sched_setaffinity(getpid(), sizeof(cpu_set_t), &set) == -1) {
            exit(EXIT_FAILURE);
        }

        for (size_t i = 0; i < nloops; i++) {
            read(first_pipefd[0], NULL, 0);
            write(second_pipefd[1], NULL, 0);
        }
    } else {           // parent
        if (sched_setaffinity(getpid(), sizeof(cpu_set_t), &set) == -1) {
            exit(EXIT_FAILURE);
        }

        gettimeofday(&start, NULL);
        for (size_t i = 0; i < nloops; i++) {
            write(first_pipefd[1], NULL, 0);
            read(second_pipefd[0], NULL, 0);
        }
        gettimeofday(&end, NULL);
        printf("context switch: %f microseconds\n", (float) (end.tv_sec * 1000000 + end.tv_usec - start.tv_sec * 1000000 - start.tv_usec) / nloops);
    }
    return 0;
}
