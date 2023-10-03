/*from mkfs.c*/
#include <fcntl.h>
#define stat xv6_stat

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
#include <unistd.h>

#define BITMAP_DEBUG
// #define NOT_MMAP_COPY
#define REREAD
#define KEEP_ORIGINAL_FILE
#define ALLOW_SAME_INPUT_OUTPUT
#define DEBUG_DUP_MMAP
/*
based on mkfs.c `wsect(i, zeroes);`
*/
#define IMG_SIZE (FSSIZE * BSIZE)
#ifdef BITMAP_DEBUG
// #define BITMAP_LOG
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

#define MAX_CHILDS 10
typedef struct dir_map {
  // char parent_name[DIRSIZ];
  // char child_names[MAX_CHILDS][DIRSIZ];
  uint child_inum[MAX_CHILDS];
  uint child_num;
  uint parent_inum;
} Dir_map;

/*
similar to the `iappend` structure.
*/
void read_inode_data(struct dinode inode, void *imgp, void *destp, int offset,
                     int size) {
  uint block_num = offset / BSIZE;
  /*
  this is due to fs.img small root inode block usage.
  */
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
  /*
  1. by `bcopy(p, buf + off - (fbn * BSIZE), n1);` which is based on block addr `buf`
  so here `imgp + addr * BSIZE` to get the block addr
  and then `offset % BSIZE` to get the relative address `off - (fbn * BSIZE)`.
  2. due to `n1 = min(n, (fbn + 1) * BSIZE - off);` where `de` size < `BSIZE`
    here use `size`.
  */
  memmove(destp, imgp + addr * BSIZE + offset % BSIZE, size);
}

void check_inode_type(int type) {
  if (type != 0 && type != T_DIR && type != T_FILE && type != T_DEV) {
    fprintf(stderr, "ERROR: bad inode.\n");
    // exit(EXIT_FAILURE);
  }
}

static uint data_end_block;

void check_address(uint addr, bool direct, uint data_start, uchar *bmap,
                   uchar bmap_mark[]) {
  if ((addr < data_start && addr != 0) || addr >= FSSIZE) {
    if (direct)
      fprintf(stderr, "ERROR: bad direct address in inode.\n");
    else
      fprintf(stderr, "ERROR: bad indirect address in inode.\n");
    // exit(EXIT_FAILURE);
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
  here I keep the API although assigning many times is not efficient.
  */
  data_end_block=index;
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
    // exit(EXIT_FAILURE);
  }
  if (bmap_mark[index / 8] & b) { // error 7 & 8
    if (direct)
      fprintf(stderr, "ERROR: direct address used more than once.\n");
    else
      fprintf(stderr, "ERROR: indirect address used more than once.\n");
    // exit(EXIT_FAILURE);
  }
  bmap_mark[index / 8] |= b;
}

/*
here keep the API although having unnecessary overheads.
*/
uint
xint(uint x)
{
  uint y;
  uchar *a = (uchar*)&y;
  a[0] = x;
  a[1] = x >> 8;
  a[2] = x >> 16;
  a[3] = x >> 24;
  return y;
}

#define min(a, b) ((a) < (b) ? (a) : (b))

#define DE_SIZE 16
void rsect(void *imgp,void *buf,uint sec,uint to_read_num){
  memmove(buf, imgp + sec * BSIZE,
                  to_read_num);
}

void wsect(void *imgp,void *buf,uint sec,uint to_write_num){
  memmove(imgp + sec * BSIZE,buf,
                  to_write_num);
}

void
iappend(struct dinode* inodes_ptr, void *imgp, uint save_dir_inum, void *xp, int n)
{
  char *p = (char*)xp;
  uint fbn, off, n1;
  struct dinode din;
  char buf[BSIZE];
  memset(buf, 0, BSIZE);
  uint indirect[NINDIRECT];
  uint x;

  din=inodes_ptr[save_dir_inum];
  off = din.size;
  printf("data_end_block:%d\n",data_end_block);
  // printf("append inum %d at off %d sz %d\n", inum, off, n);
  while(n > 0){
    fbn = off / BSIZE;
    assert(fbn < MAXFILE);
    if(fbn < NDIRECT){
      if(xint(din.addrs[fbn]) == 0){
        din.addrs[fbn] = xint(data_end_block++);
      }
      x = xint(din.addrs[fbn]);
    } else {
      if(xint(din.addrs[NDIRECT]) == 0){
        din.addrs[NDIRECT] = xint(data_end_block++);
      }
      rsect(imgp,indirect,din.addrs[NDIRECT],sizeof(indirect));           
      if(indirect[fbn - NDIRECT] == 0){
        indirect[fbn - NDIRECT] = xint(data_end_block++);
        printf("write indirect[%d] with %d at %d block\n",fbn - NDIRECT,data_end_block,din.addrs[NDIRECT]);
        wsect(imgp,indirect,din.addrs[NDIRECT],sizeof(indirect));
      }
      x = xint(indirect[fbn-NDIRECT]);
    }
    n1 = min(n, (fbn + 1) * BSIZE - off);
    rsect(imgp,buf,x,sizeof(buf));
    /*
    write data
    */
    printf("sizeof(buf):%ld\n",sizeof(buf));
    struct dirent*tmp=(struct dirent*)(buf + off - (fbn * BSIZE)-n1);
    printf("read last write with info (%d,%s) at %d block off: %d fbn:%d with data from imgp+%d*BSIZE+%d-%d\n"\
    ,tmp->inum,tmp->name,x,off - (fbn * BSIZE)-n1,fbn,x,off,(fbn * BSIZE)+n1);
    bcopy(p, buf + off - (fbn * BSIZE), n1);
    printf("write with info (%d,%s) at %d block off: %d fbn:%d\nresult (%d,%s)\n"\
    ,((struct dirent*)p)->inum,((struct dirent*)p)->name,x,off - (fbn * BSIZE),fbn\
    ,((struct dirent*)(buf + off - (fbn * BSIZE)))->inum,((struct dirent*)(buf + off - (fbn * BSIZE)))->name);
    wsect(imgp,buf,x,sizeof(buf));
    #ifdef REREAD
    char tmp_buf[BSIZE];
    rsect(imgp,tmp_buf,x,sizeof(tmp_buf));
    tmp=(struct dirent*)(tmp_buf + off - (fbn * BSIZE));
    printf("reread last write with info (%d,%s) at %d block off: %d fbn:%d with data from imgp+%d*BSIZE+%d-%d\n"\
    ,tmp->inum,tmp->name,x,off - (fbn * BSIZE),fbn,x,off,(fbn * BSIZE));
    #endif
    tmp=(struct dirent*)(buf + off - (fbn * BSIZE)+n1);
    printf("to write block with init info (%d,%s) at %d block off: %d fbn:%d\n"\
    ,tmp->inum,tmp->name,x,off+n1,fbn);
    struct dirent* tmp_dir;
    for (int i=32; i<272+1; i+=DE_SIZE) {
      tmp_dir=imgp+60*BSIZE+i-DE_SIZE;
      printf("data (%d,%s)",tmp_dir->inum,tmp_dir->name);
      fflush(stdout);
    }
    printf("\ninside iappend\n");
    n -= n1;
    off += n1;
    p += n1;
  }
  din.size = xint(off);
  inodes_ptr[save_dir_inum]=din;
  char tmp_buf[BSIZE];
  rsect(imgp,tmp_buf,save_dir_inum,sizeof(tmp_buf));
  printf("update to size %d with imgp related %d\n",inodes_ptr[save_dir_inum].size,((struct dinode*)(imgp + 32 * BSIZE+sizeof(struct dinode)*2))[save_dir_inum].size);
}

/*
img_ptr is to access blocks.
*/
void mov_lost_found(void *img_ptr,struct dinode* inodes_ptr,uint inum,uint save_dir){
  struct dirent de;
  bzero(&de, sizeof(de));
  /*
  here assume the machine use little-endian as x86.
  */
  de.inum = inum;
  /*
  1. > We will provide you with an xv6 image that has a number of in-use inodes that are not linked by any directory.
  TODO here temporarily not know the structure of collected inodes 
  (i.e. whether inum points to one inode which has `de` related with de.name)
  */
  strncpy(de.name, "lost_inodes", DIRSIZ);
  iappend(inodes_ptr,img_ptr,save_dir, &de, sizeof(de));
  printf("inside mov_lost_found\n");
  struct dirent* tmp_dir;
  for (int i=32; i<272+1; i+=DE_SIZE) {
    tmp_dir=img_ptr+60*BSIZE+i-DE_SIZE;
    printf("data (%d,%s)",tmp_dir->inum,tmp_dir->name);
    fflush(stdout);
  }
}

void trace_parent(void *img_ptr,struct dinode* inodes_ptr,int parent_inum,int leaf_dir_in_subtree){
  struct dinode inode=inodes_ptr[parent_inum];
  struct dirent de;
  int ret=0;
  for (int off = 0; off < inode.size; off += sizeof(de)) {
    read_inode_data(inode, img_ptr, &de, off, sizeof(de));
    /*
    find a loop.
    1. here allow symbolic child link points to leaf_dir_in_subtree
    */
    if (strcmp(de.name, ".") == 0 && de.inum==leaf_dir_in_subtree) {
      fprintf(stderr, "ERROR: inaccessible directory exists.\n");
      // exit(EXIT_FAILURE);
    }
    if (strcmp(de.name, "..") == 0) {
      trace_parent(img_ptr, inodes_ptr, de.inum, leaf_dir_in_subtree);
    }
  }
}

int main(int argc, char *argv[]) {
  if (argc != 2 && argc != 3) {
    fprintf(stderr, "Usage: xcheck (-r) <file_system_image>\n");
    // exit(EXIT_FAILURE);
  }
  FILE *img_file;
  char repair=0;
  if (argc==2) {
    img_file=fopen(argv[1], "r+");
  }else {
    if (strncmp(argv[1], "-r", 3)) {
      fprintf(stderr, "need -r\n");
      // exit(EXIT_FAILURE);
    }
    #ifndef ALLOW_SAME_INPUT_OUTPUT 
    assert(strstr(argv[2], "fs.img.repair")==NULL);
    #endif
    repair=1;
    img_file=fopen(argv[2], "r+");
  }
  if (img_file == NULL) {
    if (errno == ENOENT)
      fprintf(stderr, "image not found.\n");
    else
      fprintf(stderr, "fopen failed\n");
    // exit(EXIT_FAILURE);
  }
  int fd = fileno(img_file);
  if (fd == -1) {
    fprintf(stderr, "fileno failed\n");
    // exit(EXIT_FAILURE);
  }
  void *imgp = mmap(NULL, IMG_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
  if (imgp == MAP_FAILED) {
    fprintf(stderr, "imgp mmap failed\n");
    // exit(EXIT_FAILURE);
  }
  void *origin_imgp = calloc(IMG_SIZE,1);
  memmove(origin_imgp, imgp, IMG_SIZE);

  #ifdef KEEP_ORIGINAL_FILE
  char target_file[200]={0};
  strncpy(target_file, argv[2], strlen(argv[2]));
  #ifndef ALLOW_SAME_INPUT_OUTPUT 
  strncat(target_file,".repair",strlen(".repair")+1);
  #endif
  int file_fd=open(target_file, O_RDWR|O_CREAT|O_TRUNC, 0666);
  assert(file_fd!=-1);
  ftruncate(file_fd,IMG_SIZE);
  #ifdef DEBUG_DUP_MMAP
  struct superblock sb;
  memmove(&sb, imgp + BSIZE, sizeof(sb));
  printf("ninodes %d\n",sb.ninodes);
  #endif
  char *dst;
  if ((dst = mmap(NULL, IMG_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, file_fd, 0))==MAP_FAILED) {
    fprintf(stderr, "mmap failed\n");
  }
  assert(dst!=imgp);
  memmove(dst, imgp, IMG_SIZE);
  if (msync(dst, IMG_SIZE, MS_SYNC) == -1)
  {
    perror("Could not sync the file to disk");
  }
  if (memcmp(dst, imgp, IMG_SIZE)) {
    printf("img has been repaired\n");
  }
  #ifdef DEBUG_DUP_MMAP
  memmove(&sb, imgp + BSIZE, sizeof(sb));
  printf("ninodes %d\n",sb.ninodes);
  #endif
  if (munmap(imgp, IMG_SIZE) == -1) {
    fprintf(stderr, "munmap failed\n");
    // exit(EXIT_FAILURE);
  }
  imgp=dst;
  #endif
  #ifndef DEBUG_DUP_MMAP
  struct superblock sb;
  #endif
  /*
  by `wsect(1, buf)`
  */
  memmove(&sb, imgp + BSIZE, sizeof(sb));
  int data_start = FSSIZE - sb.nblocks; // i.e. start at the `nmeta` location.
  struct dinode inodes[sb.ninodes];
  printf("ninodes %d\n",sb.ninodes);
  Dir_map maps[sb.ninodes];
  memset(maps, 0, sb.ninodes*sizeof(Dir_map));
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
  
  uint lost_found_dir_inum=0;
  char target_dir_str[DIRSIZ]="lost_found";

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
            printf("check %dth indirect array with block index %d\n",k,indirect[k]);
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
      printf("find dir inode %d size:%d\n",i,inode.size);
      for (int off = 0; off < inode.size; off += sizeof(de)) {
        read_inode_data(inode, imgp, &de, off, sizeof(de));
        if (strcmp(de.name, ".") == 0) {
          count_dots++;
          /*
          > the . entry points to the directory itself.
          */
          if (de.inum != i) {
            dir_error = true;
            break;
          }
          /*
          assume /lost_found traverse begins latter than root.
          */
          if (lost_found_dir_inum==de.inum) {
            lost_found_dir_inum=de.inum;
            printf("lost_found inum:%d addr[0]:%d\n",de.inum,inode.addrs[0]);
          }
          /*
          >  contains . and .. entries
          */
        } else if (strcmp(de.name, "..") == 0) {
          count_dots++;
          if (i == ROOTINO && de.inum == ROOTINO)
            root_exist = true;
          else{
            int find=0;
            /*
            TODO since 'find dir inode' only has root 1, so here no test run.
            */
            for (int child_index=0; child_index<maps[de.inum].child_num; child_index++) {
              if (maps[de.inum].child_inum[child_index]==i) {
                find=1;
              }
            }
            if(find==0){
              fprintf(stderr, "ERROR: parent directory mismatch.\n");
              // exit(EXIT_FAILURE);
            }
            /*
            trace to the root
            */
            trace_parent(inodes,imgp,de.inum,i);
          }
        }
        /*
        > at least one directory
        */
        inode_dir[de.inum] = 1;
        /*
        `din.nlink = xshort(1);` and no other reference in mkfs
        when compiled by `./mkfs fs.img README $(UPROGS)`.
        */
        if(de.inum !=0 && inodes[de.inum].nlink!=1){
          printf("inum %d nlink %d\n",de.inum,inodes[de.inum].nlink);
        }
        /*
        error 11
        */
        if (inodes[de.inum].type == T_FILE)
          inodes[de.inum].nlink--;
        /*
        error 12
        child dirs
        here assume parent allocated before child, so the parent inode met first.
        */
        else if (inodes[de.inum].type == T_DIR && i != de.inum){
          dir_links[de.inum]++;
          maps[i].child_inum[maps[i].child_num++]=de.inum;
          if (strncmp(de.name, target_dir_str, strlen(target_dir_str))==0) {
            lost_found_dir_inum=de.inum;
            // printf("lost_found inum:%d addr[0]:%d\n",de.inum,inode.addrs[0]);
            printf("lost_found inum:%d\n",de.inum);
          }
        }
      }
      if (i == ROOTINO && !root_exist) {
        fprintf(stderr, "ERROR: root directory does not exist.\n");
        // exit(EXIT_FAILURE);
      }
      if (dir_error || count_dots != 2) {
        fprintf(stderr, "ERROR: directory not properly formatted.\n");
        // exit(EXIT_FAILURE);
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
      // exit(EXIT_FAILURE);
    }
  }

  for (int j = ROOTINO; j < sb.ninodes; j++) {
    /*
    here not use bitmap due to non-strict `balloc(freeblock);`
    See "assumes all the block before the data blocks are used".
    */
    if (inodes[j].type != 0 && inode_dir[j] == 0) { // error 9
      fprintf(stderr,
              "ERROR: inode marked use but not found in a directory.\n");
      /*
      based on `iappend(rootino, &de, sizeof(de));`.
      */
      if (repair) {
        printf("begin repair\n");
        mov_lost_found(imgp,inodes,j,lost_found_dir_inum);
        printf("\nafter each repair\n");
        struct dirent* tmp_dir;
        for (int i=32; i<272+1; i+=DE_SIZE) {
          tmp_dir=imgp+60*BSIZE+i-DE_SIZE;
          printf("data (%d,%s)",tmp_dir->inum,tmp_dir->name);
          fflush(stdout);
        }
      }
      // exit(EXIT_FAILURE);
    }
    if (inode_dir[j] == 1 && inodes[j].type == 0) { // error 10
      fprintf(stderr,
              "ERROR: inode referred to in directory but marked free.\n");
      // exit(EXIT_FAILURE);
    }
    if (inodes[j].type == T_FILE && inodes[j].nlink > 0) { // error 11
      fprintf(stderr, "ERROR: bad reference count for file.\n");
      // exit(EXIT_FAILURE);
    }
    if (inodes[j].type == T_DIR && dir_links[j] > 1) { // error 12
      fprintf(stderr,
              "ERROR: directory appears more than once in file system.\n");
      // exit(EXIT_FAILURE);
    }
  }
  #ifndef KEEP_ORIGINAL_FILE
  int file_fd=0;
  #endif
  if (repair) {
    // memmove(imgp + sb.inodestart * BSIZE,inodes, sizeof(inodes));
    /*
    avoid nlink duplicate decrement.
    */
    void *inode_target=imgp + sb.inodestart * BSIZE+sizeof(struct dinode)*lost_found_dir_inum;
    void *inode_src=inodes+lost_found_dir_inum;
    if(inodes+lost_found_dir_inum==&inodes[2]){
      printf("lost_found_dir_inum %d",lost_found_dir_inum);
    }
    printf("byte offset %ld\n",(void*)(&inodes[2].size)-(void*)(&inodes[2]));
    printf("check size %d with imgp related %d\n",inodes[2].size,((struct dinode*)(imgp + 32 * BSIZE+sizeof(struct dinode)*2))->size);
    printf("inodestart %d lost_found_dir_inum:%d\n",sb.inodestart,lost_found_dir_inum);
    // assert(memcmp(inode_target, inode_src, sizeof(struct dinode))!=0);
    memmove(inode_target,inode_src, sizeof(struct dinode));
    if (memcmp(origin_imgp, imgp, IMG_SIZE)) {
      printf("img has been repaired\n");
    }
    struct dirent* tmp_dir;
    for (int i=32; i<272+1; i+=DE_SIZE) {
      tmp_dir=imgp+60*BSIZE+i-DE_SIZE;
      printf("data (%d,%s)",tmp_dir->inum,tmp_dir->name);
      fflush(stdout);
    }
    printf("\n");
    #ifndef KEEP_ORIGINAL_FILE
    /*same as mkfs.c*/
    file_fd=open("fs.img.repair", O_RDWR|O_CREAT|O_TRUNC, 0666);
    assert(file_fd!=-1);
    #ifdef NOT_MMAP_COPY
    /*
    not use sizeof which may stop at ending 0.
    */
    write(file_fd, imgp, IMG_SIZE);
    #else
    /*
    https://stackoverflow.com/a/14242446/21294350
    */
    ftruncate(file_fd,IMG_SIZE);
    char *dst;
    if ((dst = mmap(NULL, IMG_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, file_fd, 0))==MAP_FAILED) {
      fprintf(stderr, "mmap failed\n");
      // exit(EXIT_FAILURE);
    }
    /*
    this will "bus error" maybe
    1. too big files https://stackoverflow.com/questions/212466/what-is-a-bus-error-is-it-different-from-a-segmentation-fault#comment50767046_212585
    2. align https://stackoverflow.com/a/212585/21294350
    - https://stackoverflow.com/a/26259596/21294350
    */
    memcpy (dst, imgp, IMG_SIZE);
    for (int i=32; i<272+1; i+=DE_SIZE) {
      tmp_dir=(struct dirent*)(dst+60*BSIZE+i-DE_SIZE);
      printf("dst data (%d,%s)",tmp_dir->inum,tmp_dir->name);
      fflush(stdout);
    }
    printf("\n");
    /*
    https://gist.github.com/marcetcheverry/991042 or https://en.wikipedia.org/wiki/Mmap
    */
    if (msync(dst, IMG_SIZE, MS_SYNC) == -1)
    {
        perror("Could not sync the file to disk");
    }
    /*
    msync no use for writing to the file.
    */
    // assert(write(file_fd, dst, IMG_SIZE)!=-1);
    /*
    https://stackoverflow.com/a/22679209/21294350
    still use write
    */
    if (munmap(dst, IMG_SIZE) == -1) {
      fprintf(stderr, "munmap failed\n");
      // exit(EXIT_FAILURE);
    }
    #endif
    #endif
  }
  #ifdef KEEP_ORIGINAL_FILE
  if (msync(imgp, IMG_SIZE, MS_SYNC) == -1)
  {
      perror("Could not sync the file to disk");
  }
  assert(write(file_fd, imgp, IMG_SIZE)!=-1);
  #endif
  

  if (munmap(imgp, IMG_SIZE) == -1) {
    fprintf(stderr, "munmap failed\n");
    // exit(EXIT_FAILURE);
  }
  if (fclose(img_file) == EOF) {
    fprintf(stderr, "fclose failed\n");
    // exit(EXIT_FAILURE);
  }
  free(origin_imgp);
  assert(close(file_fd)!=-1);
}
