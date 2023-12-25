#ifndef LIBC_SERVICE_FS_H_
#define LIBC_SERVICE_FS_H_

#include <fs/ipc.h>
#include <libcaprese/cap.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

  id_cap_t fs_mount(endpoint_cap_t ep_cap, const char* root_path);
  void     fs_unmount(id_cap_t id_cap);
  bool     fs_mounted(const char* root_path);
  id_cap_t fs_open(const char* path);
  void     fs_close(id_cap_t fd);
  ssize_t  fs_read(id_cap_t fd, void* buf, size_t count);
  ssize_t  fs_write(id_cap_t fd, const void* buf, size_t count);

#ifdef __cplusplus
} // extern "C"
#endif // __cplusplus

#endif // LIBC_SERVICE_FS_H_
