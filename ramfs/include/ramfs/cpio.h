#ifndef RAMFS_CPIO_H_
#define RAMFS_CPIO_H_

#include <ramfs/directory.h>

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

constexpr const char CPIO_MAGIC[]   = "070701";
constexpr const char CPIO_EOF_TAG[] = "TRAILER!!!";

bool load_cpio(directory& root_dir, const char* cpio_data);

#endif // RAMFS_CPIO_H_
