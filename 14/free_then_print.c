#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    int *data = (int *) malloc(100);
    #ifdef PROBLEM_6
    free(data);
    #endif
    #ifndef PROBLEM_6
    free(&data[1]);
    #endif
    #ifdef PROBLEM_6
    printf("%d\n", data[0]);
    #endif
    return 0;
}
