#include <cerrno>
#include <cstring>
#include <fs/fs_table.h>
#include <fs/ipc.h>
#include <libcaprese/cxx/id_map.h>
#include <map>

caprese::id_map<file_system_info> fs_table;

namespace {
  std::map<std::string, id_cap_t> fs_table_by_path;

  struct file_info {
    id_cap_t    fs_id;
    std::string path;
  };

  caprese::id_map<file_info> file_table;
} // namespace

int mount_fs(id_cap_t id_cap, const std::string& root_path, endpoint_cap_t ep_cap) {
  if (fs_table.contains(id_cap) || fs_table_by_path.contains(root_path)) [[unlikely]] {
    return FS_CODE_E_ALREADY_MOUNTED;
  }

  fs_table.emplace(id_cap, file_system_info { id_cap, root_path, ep_cap });
  fs_table_by_path.emplace(root_path, id_cap);

  return FS_CODE_S_OK;
}

int unmount_fs(id_cap_t id_cap) {
  if (!fs_table.contains(id_cap)) [[unlikely]] {
    return FS_CODE_E_NOT_MOUNTED;
  }

  const file_system_info& fs_info = fs_table.at(id_cap);

  fs_table_by_path.erase(fs_info.root_path);
  fs_table.erase(id_cap);

  unwrap_sysret(sys_cap_destroy(id_cap));

  return FS_CODE_S_OK;
}

bool mounted_fs(const std::string& root_path) {
  return fs_table_by_path.contains(root_path);
}

std::pair<decltype(fs_table)::iterator, std::string> find_fs(const std::string& path) {
  auto iter = fs_table_by_path.lower_bound(path);

  while (iter != fs_table_by_path.end()) {
    if (path.starts_with(iter->first.c_str())) {
      ++iter;
      continue;
    } else {
      break;
    }
  }
  --iter;

  if (iter == fs_table_by_path.end()) {
    return std::pair<decltype(fs_table)::iterator, std::string>(fs_table.end(), "");
  }

  auto res_iter = fs_table.find(iter->second);
  if (res_iter == fs_table.end()) {
    return std::pair<decltype(fs_table)::iterator, std::string>(fs_table.end(), "");
  } else {
    return std::pair<decltype(fs_table)::iterator, std::string>(res_iter, path.substr(iter->first.size()));
  }
}

id_cap_t open_fs(const std::string& path) {
  auto result = find_fs(path);
  if (result.first == fs_table.end()) [[unlikely]] {
    errno = FS_CODE_E_NO_SUCH_FILE;
    return 0;
  }

  message_t* msg = new_ipc_message(sizeof(uintptr_t) * 2 + result.second.size() + 1);
  if (msg == nullptr) [[unlikely]] {
    errno = FS_CODE_E_FAILURE;
    return 0;
  }

  set_ipc_data(msg, 0, FS_MSG_TYPE_OPEN);
  set_ipc_cap(msg, 1, unwrap_sysret(sys_id_cap_copy(result.first->second.id_cap)), false);
  set_ipc_data_array(msg, 2, result.second.c_str(), result.second.size() + 1);

  if (sysret_failed(sys_endpoint_cap_call(result.first->second.ep_cap, msg))) [[unlikely]] {
    delete_ipc_message(msg);
    errno = FS_CODE_E_FAILURE;
    return 0;
  }

  if (get_ipc_data(msg, 0) != FS_CODE_S_OK) [[unlikely]] {
    delete_ipc_message(msg);
    errno = get_ipc_data(msg, 0);
    return 0;
  }

  id_cap_t fd = move_ipc_cap(msg, 1);
  if (unwrap_sysret(sys_cap_type(fd)) != CAP_ID) [[unlikely]] {
    sys_cap_destroy(fd);
    delete_ipc_message(msg);
    errno = FS_CODE_E_FAILURE;
    return 0;
  }

  if (file_table.contains(fd)) [[unlikely]] {
    sys_cap_destroy(fd);
    delete_ipc_message(msg);
    errno = FS_CODE_E_FAILURE;
    return 0;
  }

  file_table.emplace(fd, file_info { result.first->second.id_cap, path });

  delete_ipc_message(msg);

  return fd;
}

int close_fs(id_cap_t fd) {
  if (!file_table.contains(fd)) [[unlikely]] {
    return FS_CODE_E_ILL_ARGS;
  }

  const file_info&        file_info = file_table.at(fd);
  const file_system_info& fs_info   = fs_table.at(file_info.fs_id);

  message_t* msg = new_ipc_message(3);
  if (msg == nullptr) [[unlikely]] {
    errno = FS_CODE_E_FAILURE;
    return 0;
  }

  set_ipc_data(msg, 0, FS_MSG_TYPE_CLOSE);
  set_ipc_cap(msg, 1, unwrap_sysret(sys_id_cap_copy(fs_info.id_cap)), false);
  set_ipc_cap(msg, 2, fd, true);

  if (sysret_failed(sys_endpoint_cap_send_long(fs_info.ep_cap, msg))) [[unlikely]] {
    set_ipc_data(msg, 2, 0);
    sys_cap_revoke(fd);
    delete_ipc_message(msg);
    errno = FS_CODE_E_FAILURE;
    return 0;
  }

  file_table.erase(fd);
  sys_cap_destroy(fd);
  delete_ipc_message(msg);

  return FS_CODE_S_OK;
}

int read_fs(id_cap_t fd, void* buf, size_t size, size_t* act_size) {
  if (!file_table.contains(fd)) [[unlikely]] {
    return FS_CODE_E_ILL_ARGS;
  }

  if (buf == nullptr) [[unlikely]] {
    errno = FS_CODE_E_ILL_ARGS;
    return 0;
  }

  if (size == 0) [[unlikely]] {
    *act_size = 0;
    return FS_CODE_S_OK;
  }

  const file_info&        file_info = file_table.at(fd);
  const file_system_info& fs_info   = fs_table.at(file_info.fs_id);

  message_t* msg       = new_ipc_message(0x1000);
  size_t     read_size = 0;
  char*      ptr       = reinterpret_cast<char*>(buf);
  int        result    = FS_CODE_S_OK;

  while (size > read_size) {
    size_t len = std::min(size - read_size, msg->header.payload_capacity - sizeof(uintptr_t) * 4);

    set_ipc_data(msg, 0, FS_MSG_TYPE_READ);
    set_ipc_cap(msg, 1, unwrap_sysret(sys_id_cap_copy(fs_info.id_cap)), false);
    set_ipc_cap(msg, 2, fd, true);
    set_ipc_data(msg, 3, len);

    if (sysret_failed(sys_endpoint_cap_call(fs_info.ep_cap, msg))) [[unlikely]] {
      result = FS_CODE_E_FAILURE;
      break;
    }

    result = get_ipc_data(msg, 0);
    if (result != FS_CODE_S_OK && result != FS_CODE_E_EOF) [[unlikely]] {
      break;
    }

    size_t length = get_ipc_data(msg, 1);
    if (length == 0) [[unlikely]] {
      break;
    }

    memcpy(ptr + read_size, get_ipc_data_ptr(msg, 2), length);
    read_size += length;

    if (result == FS_CODE_E_EOF) [[unlikely]] {
      break;
    }
  }

  *act_size = read_size;

  delete_ipc_message(msg);

  return result;
}

int write_fs(id_cap_t fd, const void* buf, size_t size, size_t* act_size) {
  if (!file_table.contains(fd)) [[unlikely]] {
    return FS_CODE_E_ILL_ARGS;
  }

  if (buf == nullptr) [[unlikely]] {
    errno = FS_CODE_E_ILL_ARGS;
    return 0;
  }

  if (size == 0) [[unlikely]] {
    *act_size = 0;
    return FS_CODE_S_OK;
  }

  const file_info&        file_info = file_table.at(fd);
  const file_system_info& fs_info   = fs_table.at(file_info.fs_id);

  message_t*  msg          = new_ipc_message(0x1000);
  size_t      written_size = 0;
  const char* ptr          = reinterpret_cast<const char*>(buf);
  int         result       = FS_CODE_S_OK;

  while (size > written_size) {
    size_t len = std::min(size - written_size, msg->header.payload_capacity - sizeof(uintptr_t) * 4);

    set_ipc_data(msg, 0, FS_MSG_TYPE_WRITE);
    set_ipc_cap(msg, 1, unwrap_sysret(sys_id_cap_copy(fs_info.id_cap)), false);
    set_ipc_cap(msg, 2, fd, true);
    set_ipc_data(msg, 3, len);
    set_ipc_data_array(msg, 4, ptr + written_size, len);

    if (sysret_failed(sys_endpoint_cap_call(fs_info.ep_cap, msg))) [[unlikely]] {
      result = FS_CODE_E_FAILURE;
      break;
    }

    result = get_ipc_data(msg, 0);

    if (result != FS_CODE_S_OK) [[unlikely]] {
      break;
    }

    size_t length = get_ipc_data(msg, 1);
    if (length == 0) [[unlikely]] {
      break;
    }

    written_size += length;
  }

  *act_size = written_size;

  delete_ipc_message(msg);

  return result;
}
