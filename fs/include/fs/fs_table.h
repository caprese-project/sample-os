#ifndef FS_FS_TABLE_H_
#define FS_FS_TABLE_H_

#include <libcaprese/cap.h>
#include <libcaprese/cxx/id_map.h>
#include <string>
#include <utility>

struct file_system_info {
  id_cap_t       id_cap;
  std::string    root_path;
  endpoint_cap_t ep_cap;
};

extern caprese::id_map<file_system_info> fs_table;

int                                                  mount_fs(id_cap_t id_cap, const std::string& root_path, endpoint_cap_t ep_cap);
int                                                  unmount_fs(id_cap_t id_cap);
bool                                                 mounted_fs(const std::string& root_path);
bool                                                 exists_fs(const std::string& path);
std::pair<decltype(fs_table)::iterator, std::string> find_fs(const std::string& path);
id_cap_t                                             open_fs(const std::string& path);
int                                                  close_fs(id_cap_t fd);
int                                                  read_fs(id_cap_t fd, void* buf, size_t size, size_t* act_size);
int                                                  write_fs(id_cap_t fd, const void* buf, size_t size, size_t* act_size);

#endif // FS_FS_TABLE_H_
