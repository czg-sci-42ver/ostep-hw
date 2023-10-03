/*
little-endian x86
*/
#include <sys/types.h>
#include <stdio.h>
typedef unsigned char uchar;
uint
xint(uint x)
{
  uint y;
  uchar *a = (uchar*)&y;
  
  uchar *b = (uchar*)&x;
  for (int i=0; i<4; i++) {
    printf("before mod, b[%d]: %x ",i,b[i]);
  }
  printf("\n");
  a[0] = x;
  a[1] = x >> 8;
  a[2] = x >> 16;
  a[3] = x >> 24;
  for (int i=0; i<4; i++) {
    printf("a[%d]: %x ",i,a[i]);
  }
  printf("\n");
  return y;
}
int main(){
  int test_num = 0x81234567;
  printf("xint(%x) -> %x\n",test_num,xint(test_num));
}