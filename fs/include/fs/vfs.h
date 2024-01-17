#ifndef FS_FILESYSTEM_H_
#define FS_FILESYSTEM_H_

#include <fs/directory.h>
#include <fs/ipc.h>
#include <string_view>

[[nodiscard]] bool vfs_init();

[[nodiscard]] int vfs_mount(endpoint_cap_t ep_cap, std::string_view path, id_cap_t& fs_id);
[[nodiscard]] int vfs_unmount(std::string_view path);
[[nodiscard]] int vfs_mounted(std::string_view path, bool* mounted);
[[nodiscard]] int vfs_get_info(std::string_view path, fs_file_info& dst);
[[nodiscard]] int vfs_create(std::string_view path, int type);
[[nodiscard]] int vfs_remove(std::string_view path);
[[nodiscard]] int vfs_open(std::string_view path, id_cap_t& fd);
[[nodiscard]] int vfs_close(id_cap_t fd);
[[nodiscard]] int vfs_read(id_cap_t fd, char* buffer, std::streamsize size, std::streamsize& act_size);
[[nodiscard]] int vfs_write(id_cap_t fd, std::string_view data, std::streamsize& act_size);
[[nodiscard]] int vfs_seek(id_cap_t fd, std::streamoff offset, int whence);
[[nodiscard]] int vfs_tell(id_cap_t fd, std::streampos& dst);

#endif // FS_FILESYSTEM_H_
