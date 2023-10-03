#include<stdio.h>
#include<string.h>
int main()
{
    char array[5]={1,2,3,4,5};
    memmove(&array[1],&array[0],4);
    for(int i=0;i<5;i++)
    {
        printf("%d",array[i]);
    }
    printf("\n");

    memmove(&array[0],&array[1],4);
    for(int i=0;i<5;i++)
    {
        printf("%d",array[i]);
    }
    printf("\n");
}
