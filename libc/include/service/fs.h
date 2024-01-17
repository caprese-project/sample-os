#ifndef LIBC_SERVICE_FS_H_
#define LIBC_SERVICE_FS_H_

#include <fs/ipc.h>
#include <libcaprese/cap.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

  id_cap_t fs_mount(endpoint_cap_t ep_cap, const char* root_path);
  void     fs_unmount(id_cap_t id_cap);
  bool     fs_mounted(const char* root_path);
  bool     fs_info(const char* path, struct fs_file_info* dst);
  bool     fs_create(const char* path, int type);
  bool     fs_remove(const char* path);
  id_cap_t fs_open(const char* path);
  void     fs_close(id_cap_t fd);
  ssize_t  fs_read(id_cap_t fd, void* buf, size_t count);
  ssize_t  fs_write(id_cap_t fd, const void* buf, size_t count);
  bool     fs_seek(id_cap_t fd, intptr_t offset, int whence);
  intptr_t fs_tell(id_cap_t fd);

#ifdef __cplusplus
} // extern "C"
#endif // __cplusplus

#endif // LIBC_SERVICE_FS_H_
