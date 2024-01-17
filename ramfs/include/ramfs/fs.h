#ifndef RAMFS_FS_H_
#define RAMFS_FS_H_

#include <cstdint>
#include <fs/ipc.h>
#include <libcaprese/cap.h>
#include <string_view>

[[nodiscard]] bool ramfs_init(uintptr_t ramfs_va_base);

[[nodiscard]] int ramfs_get_info(std::string_view path, fs_file_info& dst);
[[nodiscard]] int ramfs_create(std::string_view path, int type);
[[nodiscard]] int ramfs_remove(std::string_view path);
[[nodiscard]] int ramfs_open(std::string_view path, id_cap_t& fd);
[[nodiscard]] int ramfs_close(id_cap_t fd);
[[nodiscard]] int ramfs_read(id_cap_t fd, char* buffer, std::streamsize size, std::streamsize& act_size);
[[nodiscard]] int ramfs_write(id_cap_t fd, std::string_view data, std::streamsize& act_size);
[[nodiscard]] int ramfs_seek(id_cap_t fd, std::streamoff off, std::ios_base::seekdir dir);
[[nodiscard]] int ramfs_tell(id_cap_t fd, std::streampos& pos);

#endif // RAMFS_FS_H_
