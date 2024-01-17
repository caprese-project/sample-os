#ifndef CONS_FS_H_
#define CONS_FS_H_

#include <fs/ipc.h>
#include <libcaprese/cap.h>
#include <string_view>

[[nodiscard]] bool cons_init();

[[nodiscard]] int cons_get_info(std::string_view path, fs_file_info& dst);
[[nodiscard]] int cons_open(std::string_view path, id_cap_t& fd);
[[nodiscard]] int cons_close(id_cap_t fd);
[[nodiscard]] int cons_read(id_cap_t fd, char* buffer, std::streamsize size, std::streamsize& act_size);
[[nodiscard]] int cons_write(id_cap_t fd, std::string_view data, std::streamsize& act_size);

#endif // CONS_FS_H_
