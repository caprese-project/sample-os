#ifndef LIBC_DIRENT_H_
#define LIBC_DIRENT_H_

#include <libcaprese/cap.h>

#define DT_UNKNOWN 0
#define DT_DIR     1
#define DT_REG     2

typedef uint32_t ino_t;

struct dirent {
  ino_t         d_ino;
  unsigned char d_type;
  char          d_name[256];
};

typedef struct {
  id_cap_t      fd;
  struct dirent entry;
} DIR;

DIR*           opendir(const char* name);
int            closedir(DIR* dp);
struct dirent* readdir(DIR* dp);

#endif // LIBC_DIRENT_H_
