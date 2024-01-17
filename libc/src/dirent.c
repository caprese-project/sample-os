#include <dirent.h>
#include <service/fs.h>
#include <stdlib.h>
#include <string.h>

DIR* opendir(const char* name) {
  struct fs_file_info info;
  if (!fs_info(name, &info)) {
    return NULL;
  }

  if (info.file_type != FS_FT_DIR) {
    return NULL;
  }

  id_cap_t fd = fs_open(name);
  if (fd == 0) {
    return NULL;
  }

  DIR* dp = malloc(sizeof(DIR));
  if (dp == NULL) {
    fs_close(fd);
    return NULL;
  }

  dp->fd = fd;
  return dp;
}

int closedir(DIR* dp) {
  fs_close(dp->fd);
  free(dp);
  return 0;
}

struct dirent* readdir(DIR* dp) {
  struct fs_file_info info;
  if (!fs_read(dp->fd, &info, sizeof(info))) {
    return NULL;
  }

  dp->entry.d_ino = 0;

  switch (info.file_type) {
    case FS_FT_DIR:
      dp->entry.d_type = DT_DIR;
      break;
    case FS_FT_REG:
      dp->entry.d_type = DT_REG;
      break;
    default:
      dp->entry.d_type = DT_UNKNOWN;
      break;
  }

  strncpy(dp->entry.d_name, info.file_name, sizeof(dp->entry.d_name));

  return &dp->entry;
}
