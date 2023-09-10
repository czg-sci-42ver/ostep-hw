#include "common_threads.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// If done correctly, each child should print their "before" message
// before either prints their "after" message. Test by adding sleep(1)
// calls in various locations.

// You likely need two semaphores to do this correctly, and some
// other integers to track things.

typedef struct __barrier_t {
  // add semaphores and other information here
  sem_t *mutex;
  sem_t *turnstile1;
  sem_t *turnstile2;
  int num_arrived;
  int num_threads;
} barrier_t;

// the single barrier we are using for this program
barrier_t b;
char *mutex_str = "/mutex";
char *turnstile1_str = "/turnstile1";
char *turnstile2_str = "/turnstile2";

void barrier_init(barrier_t *b, int num_threads) {
  // initialization code goes here
  b->mutex = Sem_open(mutex_str, 1);
  b->turnstile1 = Sem_open(turnstile1_str, 0);
  b->turnstile2 = Sem_open(turnstile2_str, 0);
  b->num_arrived = 0;
  b->num_threads = num_threads;
}
// #define DEBUG_POST
// #define LOG
#ifdef DEBUG_POST
int post=0;
#endif

void barrier(barrier_t *b) {
  // reusable barrier code goes here
  /*
  Sometimes weird all threads stuck at "Sem_wait(b->mutex);" when mutex init 1
  """
  __size = "\000\000\000\000Z\000\000\000\200", '\000' <repeats 22 times> # b->mutex init 1
  __size = "\000\000\000\000\000\000\000\000\200", '\000' <repeats 22 times> # b->turnstile1 init 0
  """
  after rebooting, the same code
  """
  __size = "\001\000\000\000\000\000\000\000\200", '\000' <repeats 22 times>, # works fine
  """
  */
  Sem_wait(b->mutex);
  b->num_arrived += 1;
  if (b->num_arrived == b->num_threads) {
    for (int i = 0; i < b->num_threads; i++){
      Sem_post(b->turnstile1);
      #ifdef DEBUG_POST
      post++;
      #endif
      #ifdef LOG
      printf("post\n");
      #endif
      #ifdef DEBUG_POST
      printf("post:%d\n",post);
      #endif
    }
  }
  #ifdef LOG
  printf("arrive:%d\n",b->num_arrived);
  #endif
  Sem_post(b->mutex);
  // sleep after post then begin wait at different time to test whether wait works.
  sleep(1);

  Sem_wait(b->turnstile1);
  #ifdef LOG
  printf("critical point\n");
  #endif

  // critical point between P1 and P2

  Sem_wait(b->mutex);
  b->num_arrived -= 1;
  if (b->num_arrived == 0) {
    for (int i = 0; i < b->num_threads; i++){
      /*
      Not use turnstile1 here, otherwise some threads get stuck at `Sem_wait(b->turnstile1)`
      and `num_arrived` can't be 0 to post more.
      */
      #ifdef DEBUG_POST
      Sem_post(b->turnstile1);
      post++;
      printf("post:%d\n",post);
      #else
      Sem_post(b->turnstile2);
      // Sem_wait(b->turnstile2);
      #endif
    }
  }
  Sem_post(b->mutex);
  #ifdef DEBUG_POST
  Sem_wait(b->turnstile1);
  #else
  Sem_wait(b->turnstile2);
  #endif
}

//
// XXX: don't change below here (just run it!)
//
typedef struct __tinfo_t {
  int thread_id;
} tinfo_t;

void *child(void *arg) {
  tinfo_t *t = (tinfo_t *)arg;
  printf("child %d: before\n", t->thread_id);
  barrier(&b);
  printf("child %d: after\n", t->thread_id);
  return NULL;
}

// run with a single argument indicating the number of
// threads you wish to create (1 or more)
int main(int argc, char *argv[]) {
  assert(argc == 2);
  int num_threads = atoi(argv[1]);
  assert(num_threads > 0);

  pthread_t p[num_threads];
  tinfo_t t[num_threads];

  printf("parent: begin\n");
  barrier_init(&b, num_threads);

  int i;
  for (i = 0; i < num_threads; i++) {
    t[i].thread_id = i;
    Pthread_create(&p[i], NULL, child, &t[i]);
  }

  for (i = 0; i < num_threads; i++)
    Pthread_join(p[i], NULL);

  printf("parent: end\n");
  Sem_close(b.mutex);
  Sem_close(b.turnstile1);
  Sem_close(b.turnstile2);
  Sem_unlink(mutex_str);
  Sem_unlink(turnstile1_str);
  Sem_unlink(turnstile2_str);
  return 0;
}
