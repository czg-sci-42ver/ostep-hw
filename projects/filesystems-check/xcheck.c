#include "fs.h"
#include "param.h"
#include "stat.h"
#include <errno.h>
#include <math.h> // fmin
#include <stdbool.h>
#include <stdio.h>    // fopen, fclose, fileno
#include <stdlib.h>   // exit
#include <string.h>   // memmove, strcmp, memset
#include <sys/mman.h> // mmap, munmap
#include <assert.h>

#define BITMAP_DEBUG
/*
based on mkfs.c `wsect(i, zeroes);`
*/
#define IMG_SIZE (FSSIZE * BSIZE)
#ifdef BITMAP_DEBUG
#define BITMAP_LOG
/*
`int nbitmap = FSSIZE/(BSIZE*8) + 1;` bitmap blocks
and each block is `write(fsfd, buf, BSIZE)` bytes
so byte num -> `(FSSIZE/(BSIZE*8) + 1)*BSIZE`
*/
#define NBMAP (FSSIZE / 8+BSIZE)
#else
/*
TODO different from `nbitmap`
*/
#define NBMAP FSSIZE / 8
#endif

/*
similar to the `iappend` structure.
*/
void read_inode_data(struct dinode inode, void *imgp, void *destp, int offset,
                     int size) {
  uint block_num = offset / BSIZE;
  assert(block_num==0);
  uint addr = 0;
  if (block_num < NDIRECT)
    addr = inode.addrs[block_num];
  else {
    /*
    by ` fbn * BSIZE:0 off: 272` output in xv6 `make fs.img`, root is inside the 1st NDIRECT block.
    this should not happen.
    */
    printf("root inode has NINDIRECT\n");
    block_num -= NDIRECT;
    if (block_num < NINDIRECT) {
      uint indirect[NINDIRECT];
      memmove(indirect, imgp + inode.addrs[NDIRECT] * BSIZE, sizeof(indirect));
      addr = indirect[block_num];
    }
  }
  memmove(destp, imgp + addr * BSIZE + offset % BSIZE, BSIZE);
}

void check_inode_type(int type) {
  if (type != 0 && type != T_DIR && type != T_FILE && type != T_DEV) {
    fprintf(stderr, "ERROR: bad inode.\n");
    exit(EXIT_FAILURE);
  }
}

void check_address(uint addr, bool direct, uint data_start, uchar *bmap,
                   uchar bmap_mark[]) {
  if ((addr < data_start && addr != 0) || addr >= FSSIZE) {
    if (direct)
      fprintf(stderr, "ERROR: bad direct address in inode.\n");
    else
      fprintf(stderr, "ERROR: bad indirect address in inode.\n");
    exit(EXIT_FAILURE);
  }

  if (addr == 0)
    return;
  // error 5
  /*
  `assert(used < BSIZE*8);` where `used` is from block 0 so we should use directly the index although data is from
  and `freeblock = nmeta;` is from data_start
  */
  // uint index = addr - data_start;
  uint index = addr;
  #ifdef BITMAP_LOG
  printf("set index: %d\n",index);
  #else
  #endif
  /*
  by `buf[i/8] = buf[i/8] | (0x1 << (i%8));`
  */
  uint b = 0x1 << (index % 8);
  #ifdef BITMAP_LOG
  printf("set b: %x\n",b);
  #endif
  if (!(bmap[index / 8] & b)) {
    fprintf(stderr,
            "ERROR: address used by inode but marked free in bitmap.\n");
    exit(EXIT_FAILURE);
  }
  if (bmap_mark[index / 8] & b) { // error 7 & 8
    if (direct)
      fprintf(stderr, "ERROR: direct address used more than once.\n");
    else
      fprintf(stderr, "ERROR: indirect address used more than once.\n");
    exit(EXIT_FAILURE);
  }
  bmap_mark[index / 8] |= b;
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    fprintf(stderr, "Usage: xcheck <file_system_image>\n");
    exit(EXIT_FAILURE);
  }
  FILE *img_file = fopen(argv[1], "r");
  if (img_file == NULL) {
    if (errno == ENOENT)
      fprintf(stderr, "image not found.\n");
    else
      fprintf(stderr, "fopen failed\n");
    exit(EXIT_FAILURE);
  }
  int fd = fileno(img_file);
  if (fd == -1) {
    fprintf(stderr, "fileno failed\n");
    exit(EXIT_FAILURE);
  }
  void *imgp = mmap(NULL, IMG_SIZE, PROT_READ, MAP_PRIVATE, fd, 0);
  if (imgp == NULL) {
    fprintf(stderr, "mmap failed\n");
    exit(EXIT_FAILURE);
  }
  if (fclose(img_file) == EOF) {
    fprintf(stderr, "fclose failed\n");
    exit(EXIT_FAILURE);
  }
  struct superblock sb;
  /*
  by `wsect(1, buf)`
  */
  memmove(&sb, imgp + BSIZE, sizeof(sb));
  int data_start = FSSIZE - sb.nblocks; // i.e. start at the `nmeta` location.
  struct dinode inodes[sb.ninodes];
  /*
  1. access inode by `#define IBLOCK(i, sb)     ((i) / IPB + sb.inodestart)`
  2. > Note that the version of xv6 we're using does not include the logging feature described in the book; you can safely ignore the parts that pertain to that.
  although the version used by me contains the log.
  */
  memmove(inodes, imgp + sb.inodestart * BSIZE, sizeof(inodes));
  uchar bmap[NBMAP];
  uchar bmap_mark[NBMAP] = {0};
  uchar inode_dir[sb.ninodes];
  /*
  maybe `nlink`
  */
  uint dir_links[sb.ninodes];
  memset(inode_dir, 0, sizeof(inode_dir));
  memset(dir_links, 0, sizeof(dir_links));
  memmove(bmap, imgp + sb.bmapstart * BSIZE, sizeof(bmap));
  /*
  by `uint inum = freeinode++;`: just increment to access.
  */
  for (int i = ROOTINO; i < sb.ninodes; i++) {
    struct dinode inode = inodes[i];
    #ifdef BITMAP_LOG
    printf("check %dth inode\n",i);
    #endif
    check_inode_type(inode.type); // error 1

    // check inode address range, error 2
    /*
    i.e. "Bad blocks" as the chapter fsck says.
    */
    if (inode.type != 0) {
      /*
      by `rsect(xint(din.addrs[NDIRECT]), (char*)indirect);`
      */
      for (int j = 0; j < NDIRECT + 1; j++) {
        if (j < NDIRECT)
          check_address(inode.addrs[j], true, data_start, bmap, bmap_mark);
        else {
          uint indirect[NINDIRECT];
          check_address(inode.addrs[NDIRECT], false, data_start, bmap,
                        bmap_mark);
          /*
          `BSIZE` by `indirect[fbn - NDIRECT] = xint(freeblock++);`
          */                        
          memmove(indirect, imgp + inode.addrs[NDIRECT] * BSIZE,
                  sizeof(indirect));
          /*
          by `uint indirect[NINDIRECT];` in xv6 and `if (addr == 0)` here to skip unmodified ones.
          */
          for (int k = 0; k < NINDIRECT; k++){
            #ifdef BITMAP_LOG
            printf("check %dth indirect array\n",k);
            #endif
            check_address(indirect[k], false, data_start, bmap, bmap_mark);
          }
        }
      }
    }

    // check dir, error 3 & 4
    if (inode.type == T_DIR) {
      int count_dots = 0;
      bool root_exist = false;
      bool dir_error = false;
      struct dirent de;
      /*
      
      */
      for (int off = 0; off < inode.size; off += sizeof(de)) {
        read_inode_data(inode, imgp, &de, off, sizeof(de));
        if (strcmp(de.name, ".") == 0) {
          count_dots++;
          if (de.inum != i) {
            dir_error = true;
            break;
          }
        } else if (strcmp(de.name, "..") == 0) {
          count_dots++;
          if (i == ROOTINO && de.inum == ROOTINO)
            root_exist = true;
        }
        inode_dir[de.inum] = 1;
        if (inodes[de.inum].type == T_FILE)
          inodes[de.inum].nlink--;
        else if (inodes[de.inum].type == T_DIR && i != de.inum)
          dir_links[de.inum]++;
      }
      if (i == ROOTINO && !root_exist) {
        fprintf(stderr, "ERROR: root directory does not exist.\n");
        exit(EXIT_FAILURE);
      }
      if (dir_error || count_dots != 2) {
        fprintf(stderr, "ERROR: directory not properly formatted.\n");
        exit(EXIT_FAILURE);
      }
    }
  }

  // error 6
  /*
  1. here skip the last fake used inode
  2. due to `for(i = 0; i < used; i++)` in xv6 assumes all the block before the data blocks are used
  although some inode blocks (or log, bitmap, etc.) may not be assigned data at all.
    so we only checks the data blocks
    - Also see `assert(inum<sb.bmapstart);` which also implies not all inode blocks are "assigned data at all."
    - `nmeta = 2 + nlog + ninodeblocks + nbitmap;` also shows not to care the internal fragmentation in the meta blocks
  */
  #ifdef BITMAP_DEBUG
  for (int j = (data_start/8+1)*8; j < NBMAP; j += 8) {
  #else
  for (int j = 0; j < NBMAP; j += 8) {
  #endif
    uint a = bmap[j / 8];
    uint b = bmap_mark[j / 8];
    if (a ^ b) {
      printf("a,b:%d,%d when j:%d\n",a,b,j);
      fprintf(stderr,
              "ERROR: bitmap marks block in use but it is not in use.\n");
      exit(EXIT_FAILURE);
    }
  }

  for (int j = ROOTINO; j < sb.ninodes; j++) {
    if (inodes[j].type != 0 && inode_dir[j] == 0) { // error 9
      fprintf(stderr,
              "ERROR: inode marked use but not found in a directory.\n");
      exit(EXIT_FAILURE);
    }
    if (inode_dir[j] == 1 && inodes[j].type == 0) { // error 10
      fprintf(stderr,
              "ERROR: inode referred to in directory but marked free.\n");
      exit(EXIT_FAILURE);
    }
    if (inodes[j].type == T_FILE && inodes[j].nlink > 0) { // error 11
      fprintf(stderr, "ERROR: bad reference count for file.\n");
      exit(EXIT_FAILURE);
    }
    if (inodes[j].type == T_DIR && dir_links[j] > 1) { // error 12
      fprintf(stderr,
              "ERROR: directory appears more than once in file system.\n");
      exit(EXIT_FAILURE);
    }
  }

  if (munmap(imgp, IMG_SIZE) == -1) {
    fprintf(stderr, "munmap failed\n");
    exit(EXIT_FAILURE);
  }
}
