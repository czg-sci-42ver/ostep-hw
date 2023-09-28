/*
1. > bold bits are message
i.e. ascii of “A”
2. `good_crc = good_crc ^ poly;`
because 
>                         00001110000110110
>                         00000000000000000
*/
#include <stdio.h>     // fprintf, perror
#include <stdlib.h>    // exit
#include <fcntl.h>     // open
#include <unistd.h>    // read
#include "common.h"

#define CRC_LOG
#ifdef srecord_log
/*
By checking `file=crc.txt;./crc.out ${file} > crc_1021.log;./crc_srecord_log.out ${file} > crc_11021.log;diff crc_1021.log crc_11021.log`,
here the top bit '1' will be offsetted by `0xffff`,
so it seems to not take the top bit in account just like only manipulating with 16-bit instead of 17-bit.
then both `0x1021` (16-bit) and `0x11021` (17-bit) is ok.
*/
#define POLY 0x11021   // 0001 0000 0010 0001
#else
#define POLY 0x1021   // 0001 0000 0010 0001
#endif
// The poly is 16 bit long instead of 17, because the result of the first bit after xor is always zero
// That's equivalent to check the first bit is one then move left one bit and xor the next 16 bits.

int main(int argc, char *argv[]) {
    #ifdef srecord_log
    unsigned int crc = 0xffff, ch[1];
    #else
    unsigned short crc = 0xffff, ch[1];
    #endif
    int i = 0, xor_flag = 0, fd, count;
    char * pathname = "";

    if (argc != 2) {
        fprintf(stderr, "Usage: %s [filepath]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    pathname = argv[1];
    double t = Time_GetSeconds();

    if ((fd = open(pathname, O_RDONLY)) == -1)
        handle_error("open");
    /*
    the exclusion of '\n' is to ensure `echo ... > ...` works although `echo -n` is better. 
    */
    #ifdef EXCLUDE_NEWLINE
    while ((count = read(fd, ch, 1)) != -1 && count != 0 && ch[0]!='\n') {    // \0
    #else
    while ((count = read(fd, ch, 1)) != -1 && count != 0) {    // \0
    #endif
        unsigned short v = 0x80;
        for(size_t j = 0; j < 8; j++) {
            if (crc & 0x8000)
                xor_flag = 1;
            else
                xor_flag = 0;

            crc = crc << 1;

            // Append next bit of message to end of CRC if it is not zero.
            // If it is zero, it's already appended at the above line.
            if (ch[0] & v)
                crc += 1;

            if (xor_flag)
                crc = crc ^ POLY;
            #ifdef CRC_LOG
            printf("crc:%X\n",crc);
            #endif
            v = v >> 1;
        }
        i++;
    }
    printf("read %d chars; before Augment:%X\n",i,crc);

    // Augment the message by appending 16 zero bits to the end of it.
    for (size_t j = 0; j < 16; j++) {
        if (crc & 0x8000)
            xor_flag = 1;
        else
            xor_flag = 0;

        crc = crc << 1;

        if (xor_flag)
            crc = crc ^ POLY;
    }

    printf("16-bit CRC: 0x%X\n", crc);
    printf("time(seconds): %f\n", Time_GetSeconds() - t);
    close(fd);

    return 0;
}
