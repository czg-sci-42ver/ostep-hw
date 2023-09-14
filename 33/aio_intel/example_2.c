#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#define HANDLE int

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
 
double do_compute(double A, double B, int arr_len)
{
  int i;
  double   res = 0;
  double  *xA = malloc(arr_len * sizeof(double));
  double  *xB = malloc(arr_len * sizeof(double));
  if ( !xA || !xB )
    abort();
for (i = 0; i < arr_len; i++) {
xA[i] = sin(A);
xB[i] = cos(B);
res = res + xA[i]*xA[i];
}
  free(xA);
  free(xB);
  return res;
}

// icl -c do_compute.c
// icl aio_sample2.c do_compute.obj
// aio_sample2.exe

#define DIM_X   123
#define DIM_Y    70
double  aio_dat[DIM_Y] = {0};
double  aio_dat_tmp[DIM_Y];
static volatile int aio_flg = 0;

#include <aio.h>
typedef struct aiocb  aiocb_t;
aiocb_t               my_aio;
#define WAIT { while (!aio_flg); aio_flg = 0; }
#define aio_OPEN(_fname )\
open (_fname,\
    O_CREAT, S_IRWXU)
// CreateFile(_fname,                       \
//            GENERIC_READ | GENERIC_WRITE, \
//            FILE_SHARE_READ,              \
//            NULL,                         \
//            OPEN_ALWAYS,                  \
//            FILE_ATTRIBUTE_NORMAL,        \
//            NULL)

static void aio_CompletionRoutine(sigval_t sigval)
{
    aio_flg = 1;
}

int main()
{
    double do_compute(double A, double B, int arr_len);
    int      i, j, res;
    char    *fname = "aio_sample2.dat";
    HANDLE   aio_fildes = aio_OPEN(fname);

    my_aio.aio_fildes = aio_fildes;
    my_aio.aio_nbytes = sizeof(aio_dat_tmp);
    my_aio.aio_sigevent.sigev_notify          = SIGEV_THREAD;
    my_aio.aio_sigevent.sigev_notify_function = aio_CompletionRoutine;

    /*
    ** writing
    */
    my_aio.aio_offset = -1;
    printf("Writing\n");
    for (i = 0; i < DIM_X; i++) {
        for (j = 0; j < DIM_Y; j++)
            aio_dat[j] = do_compute(i, j, DIM_X);
        WAIT;
        my_aio.aio_buf = memcpy(aio_dat_tmp, aio_dat, sizeof(aio_dat_tmp));
        res = aio_write(&my_aio);
        if (res) {printf("res!=0\n");abort();}
    }

    //
    // flushing
    //
    printf("Flushing\n");
    WAIT;
    res = aio_fsync(O_SYNC, &my_aio);
    if (res) {printf("res!=0\n");abort();}
    WAIT;

    //
    // reading
    //
    printf("Reading\n");
    my_aio.aio_offset = 0;
    my_aio.aio_buf    = (volatile char*)aio_dat_tmp;
    for (i = 0; i < DIM_X; i++) {
        aio_read(&my_aio);
        for (j = 0; j < DIM_Y; j++)
            aio_dat[j] = do_compute(i, j, DIM_X);
        WAIT;
        res = aio_return(&my_aio);
        if (res != sizeof(aio_dat)) {
            printf("aio_read() did read %d bytes, expecting %d bytes\n", res, sizeof(aio_dat));
        }

        for (j = 0; j < DIM_Y; j++)
            if ( aio_dat[j] != aio_dat_tmp[j] )
                {printf("ERROR: aio_dat[j] != aio_dat_tmp[j]\n I=%d J=%d\n", i, j); abort();}
        my_aio.aio_offset += my_aio.aio_nbytes;
    }

    // CloseHandle(aio_fildes);
    close(aio_fildes);

    printf("\nDone\n");

return 0;
}
