#include <stdio.h>
#include <stdlib.h>

#ifndef N
#define N 10
#endif
int main(int argc, char *argv[]) {
	for (int i=0;i<N;i++){
		int *x = (int *) malloc(sizeof(int));
		*x = 1;
		printf("%d\n", *x);
	}
	return 0;
}
