/*
$ rm test.txt;pagesize=4096;offset=596;for i in $(seq ${pagesize});do echo -n 'a'>>test.txt;done;echo -n 'aaa'>>test.txt;for i in $(seq ${offset});do echo -n 'b'>>test.txt;done
$ ./pzip test.txt 
connect between pages with a
4099a596b%                                   
*/
#include "thread_helper.h"
#include <arpa/inet.h> // htonl
#include <fcntl.h>     // open, O_* constants
#include <stdio.h>     // fwrite, fprintf
#include <stdlib.h>    // exit, malloc
#include <sys/fcntl.h>
#include <sys/mman.h> // mmap, munmap
#include <sys/stat.h> // fstat, mode constants
#include <sys/types.h>
#include <unistd.h> // sysconf, close

// #define DEBUG_ECHO
#define ECHO_NUM
#define DEBUG_PROCESS
/*
here `ALLOW_MULTIPLE_JOBS` only allow multiple pending jobs but not concurrent because they are all in the main thread.
*/
#define ALLOW_MULTIPLE_JOBS
#define ALLOW_MULTIPLE_JOBS_DEBUG

/*
// http://www.catb.org/esr/structure-packing/
based on the largest size TODO read after 6. Bitfields
See
> Because s only needs to be 2-byte aligned
*/
/*
// https://en.wikipedia.org/wiki/Data_structure_alignment
1. always from big to small size See "MixedData After compilation in 32-bit x86 machine"
> As long as the memory word size is at least as large as the largest primitive data type supported by the computer,
2. https://en.wikipedia.org/wiki/Data_structure_alignment#Typical_alignment_of_C_structs_on_x86
*/
typedef struct result {
  struct result *next;
  int count;
  char character;
  char pad[sizeof(struct result *) - sizeof(int) - 1]; // align to 8 bytes
} Result;

typedef struct work {
  long long chunk_size;
  char *addr;
  Result *results;
} Work;

typedef struct files {
  int fd;
  off_t size;
  char pad[sizeof(off_t) - sizeof(int)];
} Files;

static long long use_ptr = 0, fill_ptr = 0, chunks = 0;
static sem_t mutex, empty, full;

static Result *create_result(int count, char character) {
  Result *result = malloc(sizeof(Result));
  if (result == NULL)
    handle_error("malloc");
  result->count = count;
  result->character = character;
  result->next = NULL;
  return result;
}

static void *compress(void *arg) {
  Work *works = (Work *)arg;

  while (1) {
    // use semaphore instead of mutex and condition variables
    // because workers will wait for the mutex and
    // pthread_mutex_lock() is not a cancellation point,
    // therefore the main thread can't join the workers
    Sem_wait(&full);
    Sem_wait(&mutex);

    // get work
    Work *current_work = &works[use_ptr];
    #ifdef ALLOW_MULTIPLE_JOBS_DEBUG
    printf("use_ptr: %lld\n",use_ptr);
    #endif
    use_ptr = (use_ptr + 1) % chunks;

    // do work
    /*
    different from wzip, Result is all written at the end instead of written one by one.
    TODO the count is separated by page unit, needing condensed at the last
    */
    Result *head = NULL;
    Result *previous_result = NULL;
    char previous_character = '\0';
    int previous_count = 0;
    for (long long i = 0; i < current_work->chunk_size; i++) {
      char character = current_work->addr[i];
      if (character == previous_character) {
        previous_count++;
      } else {
        if (previous_count != 0) {
          Result *last_result =
              create_result(previous_count, previous_character);
          if (previous_result != NULL)
            previous_result->next = last_result;
          previous_result = last_result;
          /*
          only assign once.
          */
          if (head == NULL)
            head = previous_result;
        }
        previous_count = 1;
        previous_character = character;
      }
    }
    if (head == NULL) {
      // same characters
      current_work->results = create_result(previous_count, previous_character);
    } else {
      current_work->results = head;
      /*
      This either store the one character by `previous_count=1`
      or `previous_count>1` count `previous_character`.
      */
      previous_result->next = create_result(previous_count, previous_character);
    }

    Sem_post(&mutex);
    #ifdef ALLOW_MULTIPLE_JOBS
    #else
    Sem_post(&empty);
    #endif
  }
}

// Littleendian and Bigendian byte order illustrated
// https://dflund.se/~pi/endian.html
static void writeFile(int character_count, char *oldBuff) {
  // character_count = htonl(character_count); // write as network byte order
  /*
  same as wzip
  > a 4-byte integer followed by a character for each run
  */
  #ifdef DEBUG_ECHO
  int test=6;
  // fwrite(&test, sizeof(int), 1, stdout);
  fprintf(stdout, "%d", test);
  #endif
  #ifdef ECHO_NUM
  fprintf(stdout, "%d", character_count);
  #else
  fwrite(&character_count, sizeof(int), 1, stdout);
  #endif
  fwrite(oldBuff, sizeof(char), 1, stdout);
}

int main(int argc, char *argv[]) {
  /*
  See https://stackoverflow.com/questions/17096990/why-use-bzero-over-memset#comment24732677_17096990
  better use larger chunk to avoid many read/write syscalls.
  */
  long page_size = sysconf(_SC_PAGE_SIZE);
  #ifdef DEBUG_ECHO
  printf("pagesize:%ld\n",page_size);
  #endif

  if (argc <= 1) {
    fprintf(stdout, "pzip: file1 [file2 ...]\n");
    exit(EXIT_FAILURE);
  }

  // get_nprocs is GNU extension
  /*
  note 2 in README
  */
  long np = sysconf(_SC_NPROCESSORS_ONLN);
  pthread_t *threads = malloc(sizeof(pthread_t) * (unsigned long)np);
  if (threads == NULL)
    handle_error("malloc");

  Files *files = malloc(sizeof(Files) * (unsigned long)(argc - 1));
  if (files == NULL)
    handle_error("malloc");

  // count chunks number
  for (int i = 1; i < argc; i++) {
    int fd = open(argv[i], O_RDONLY);
    struct stat sb;
    if (fd == -1)
      handle_error("open");

    if (fstat(fd, &sb) == -1)
      handle_error("stat");

    files[i - 1].fd = fd;
    files[i - 1].size = sb.st_size;

    chunks += (sb.st_size / page_size + 1);
  }

  // init semaphores
  Sem_init(&mutex, 0, 1);
  // set empty to 1 to prevent main thread cancel
  // workers before they do the work
  Sem_init(&empty, 0, 1);
  Sem_init(&full, 0, 0);

  Work *works = malloc(sizeof(Work) * (unsigned long)chunks);
  if (works == NULL)
    handle_error("malloc");

  /*
  Here maybe each lock for each file is better.
  the empty and full ensures the consecutive processing between compress and jobs.
  */
  // create workers
  for (long i = 0; i < np; i++)
    Pthread_create(&threads[i], NULL, compress, works);

  // create jobs
  for (int i = 0; i < argc - 1; i++) {
    long long offset = 0;
    while (offset < files[i].size) {
      #ifdef ALLOW_MULTIPLE_JOBS
      #else
      Sem_wait(&empty);
      #endif
      /*
      although we can let each `works` thread use one independent `mutex` to manipulate with something like
      `works[fill_ptr-fill_ptr%np+thread_id]`. then `jobs` threads also need to be np instead of one here.
      */
      Sem_wait(&mutex);

      works[fill_ptr].chunk_size = page_size;
      #ifdef ALLOW_MULTIPLE_JOBS_DEBUG
      printf("job %d offset: %lld;fill_ptr:%lld\n",i,offset,fill_ptr);
      #endif
      if (offset + page_size > files[i].size)
        works[fill_ptr].chunk_size = files[i].size - offset;
      
      /*
      note 4 in README
      */
      char *addr = mmap(NULL, (size_t)works[fill_ptr].chunk_size, PROT_READ,
                        MAP_PRIVATE, files[i].fd, offset);
      if (addr == MAP_FAILED)
        handle_error("mmap");

      works[fill_ptr].addr = addr;
      works[fill_ptr].results = NULL;
      offset += page_size;
      /*
      TODO the % seems to be abundant by `chunks += (sb.st_size / page_size + 1);`.
      */
      fill_ptr = (fill_ptr + 1) % chunks;

      Sem_post(&mutex);
      Sem_post(&full);
    }
    close(files[i].fd);
  }

  // check jobs are done
  /*
  i.e. compress has finished the job by `Sem_post(&empty);`.
  */
  #ifdef ALLOW_MULTIPLE_JOBS
  #else
  Sem_wait(&empty);
  #endif
  /*
  Avoid to capture the posted mutex by the main thread jobs loop.
  */
  #ifndef ALLOW_MULTIPLE_JOBS
  Sem_wait(&mutex);
  #endif

  // kill and wait workers
  for (long i = 0; i < np; i++) {
    /*
    https://stackoverflow.com/a/12280247/21294350
    no signal is expected, so no `pthread_kill`
    `Pthread_cancel` not immediately cancel because "deferred (the default for new threads)" in `man pthread_cancel`
    */
    Pthread_cancel(threads[i]);
    Pthread_join(threads[i], NULL);
  }
  #ifdef ALLOW_MULTIPLE_JOBS
  Sem_wait(&mutex);
  #endif

  // final compress
  int last_count = 0;
  char last_character = '\0';
  for (long long i = 0; i < chunks; i++) {
    Result *result;
    result = works[i].results;
    /*
    take the very first, first, last, the last of whole special cases in account.
    */
    while (result != NULL) {
      /*
      here first and last need separate manipulation because the character sequence 
      may be splitted into different page units
      and they are manipulated together as one whole entity.
      */
      if (result == works[i].results &&
          result->next != NULL) { // first but not last result
        if (result->character == last_character) {
          /*
          connect the page boundary.
          */
          #ifdef DEBUG_PROCESS
          printf("connect between pages with %c\n",result->character);
          #endif
          writeFile(result->count + last_count, &result->character);
        } else {
          /*
          by `if (previous_count != 0) {` and `previous_count = 1`, last_count can't be 0
          so it only excludes the init state `int last_count = 0`.
          */
          if (last_count > 0)
            writeFile(last_count, &last_character);
          writeFile(result->count, &result->character);
        }
      } else if (result->next == NULL) {  // last result
        if (result != works[i].results) { // not first
          /*
          ensure the last chunk written at its manipulation process instead of at the next non-exist first result in the next page.
          */
          if (i == chunks - 1) {          // last chunk
            writeFile(result->count, &result->character);
          } else { // not last chunk
            last_character = result->character;
            last_count = result->count;
          }
        } else {                                     // first result
          /*
          1. then it needs to check whether same as the last page last character
          which is also done in above "// first but not last result".
          1.1 if the current last and first not overlaps (i.e. they are separated by other different characters),
          it implies not equal because the first will check
          whether equal before the last.
          */
          if (result->character == last_character) { // same
            if (i != chunks - 1) {                   // not last chunk
              last_count += result->count;
            } else {
              writeFile(result->count + last_count, &result->character);
            }
          } else { // not same
            if (last_count > 0)
              writeFile(last_count, &last_character);
            if (i != chunks - 1) {
              last_character = result->character;
              last_count = result->count;
            } else {
              writeFile(result->count, &result->character);
            }
          }
        }
      } else {
        writeFile(result->count, &result->character);
      }

      Result *tmp = result;
      result = result->next;
      free(tmp);
    }
    if (munmap(works[i].addr, (size_t)works[i].chunk_size) != 0)
      handle_error("munmap");
  }
  /*
  This seems to be no use, because no other threads request the mutex.
  */
  // Sem_post(&mutex);

  free(threads);
  free(files);
  free(works);
  Sem_destroy(&mutex);
  Sem_destroy(&full);
  Sem_destroy(&empty);

  return 0;
}
