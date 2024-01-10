#ifndef CONS_FILE_H_
#define CONS_FILE_H_

#include <libcaprese/cap.h>
#include <string>

enum struct file_type {
  uart = 0,
};

bool cons_mkfile(const std::string& path, file_type type, endpoint_cap_t ep_cap);
bool cons_rmfile(const std::string& path);
bool cons_exists(const std::string& path);
int  cons_open(const std::string& path, id_cap_t* dst_fd);
int  cons_close(id_cap_t fd);
int  cons_read(id_cap_t fd, void* buf, size_t max_size, size_t* act_size);
int  cons_write(id_cap_t fd, const void* buf, size_t size, size_t* act_size);

#endif // CONS_FILE_H_
