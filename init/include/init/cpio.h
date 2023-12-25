#ifndef INIT_CPIO_H_
#define INIT_CPIO_H_

#include <stddef.h>

struct cpio_newc_header {
  char magic[6];
  char ino[8];
  char mode[8];
  char uid[8];
  char gid[8];
  char nlink[8];
  char mtime[8];
  char filesize[8];
  char devmajor[8];
  char devminor[8];
  char rdevmajor[8];
  char rdevminor[8];
  char namesize[8];
  char check[8];
};

#define CPIO_MAGIC   "070701"
#define CPIO_EOF_TAG "TRAILER!!!"

const char* cpio_find_file(const char* cpio, size_t cpio_size, const char* file_name, size_t* file_size);

#endif // INIT_CPIO_H_
