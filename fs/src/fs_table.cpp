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

  message_buffer_t buf;
  buf.cap_part_length  = 1;
  buf.data_part_length = 1 + (result.second.size() + 1 + sizeof(uintptr_t) - 1) / sizeof(uintptr_t);

  buf.data[0] = unwrap_sysret(sys_id_cap_copy(result.first->second.id_cap));

  buf.data[1]        = FS_MSG_TYPE_OPEN;
  char* c_path_start = reinterpret_cast<char*>(&buf.data[2]);
  memcpy(c_path_start, result.second.c_str(), result.second.size() + 1);

  if (sysret_failed(sys_endpoint_cap_call(result.first->second.ep_cap, &buf))) [[unlikely]] {
    errno = FS_CODE_E_FAILURE;
    return 0;
  }

  if (buf.cap_part_length != 1 || unwrap_sysret(sys_cap_type(buf.data[0])) != CAP_ID) [[unlikely]] {
    errno = FS_CODE_E_FAILURE;
    return 0;
  }

  if (file_table.contains(buf.data[0])) [[unlikely]] {
    errno = FS_CODE_E_FAILURE;
    return 0;
  }

  file_table.emplace(buf.data[0], file_info { result.first->second.id_cap, path });

  return buf.data[0];
}

int close_fs(id_cap_t id_cap) {
  if (!file_table.contains(id_cap)) [[unlikely]] {
    return FS_CODE_E_ILL_ARGS;
  }

  const file_info&        file_info = file_table.at(id_cap);
  const file_system_info& fs_info   = fs_table.at(file_info.fs_id);

  message_buffer_t buf;
  buf.cap_part_length  = 2;
  buf.data_part_length = 1;
  buf.data[0]          = unwrap_sysret(sys_id_cap_copy(fs_info.id_cap));
  buf.data[1]          = id_cap;
  buf.data[2]          = FS_MSG_TYPE_CLOSE;

  if (sysret_failed(sys_endpoint_cap_send_long(fs_info.ep_cap, &buf))) [[unlikely]] {
    errno = FS_CODE_E_FAILURE;
    return 0;
  }

  file_table.erase(id_cap);

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

  message_buffer_t msg_buf;
  size_t           read_size = 0;
  char*            ptr       = reinterpret_cast<char*>(buf);

  while (size > read_size) {
    msg_buf.cap_part_length  = 2;
    msg_buf.data[0]          = unwrap_sysret(sys_id_cap_copy(fs_info.id_cap));
    msg_buf.data[1]          = msg_buf_delegate(fd);
    msg_buf.data_part_length = 2;
    msg_buf.data[2]          = FS_MSG_TYPE_READ;
    msg_buf.data[3]          = size - read_size;

    if (sysret_failed(sys_endpoint_cap_call(fs_info.ep_cap, &msg_buf))) [[unlikely]] {
      errno = FS_CODE_E_FAILURE;
      return 0;
    }

    if (msg_buf.cap_part_length != 0 || msg_buf.data_part_length < 2) [[unlikely]] {
      errno = FS_CODE_E_FAILURE;
      return 0;
    }

    if (msg_buf.data[0] != FS_CODE_S_OK && msg_buf.data[0] != FS_CODE_E_EOF) {
      break;
    }

    size_t length = msg_buf.data[1];
    if (length == 0) [[unlikely]] {
      break;
    }

    memcpy(ptr + read_size, &msg_buf.data[2], length);
    read_size += length;

    if (msg_buf.data[0] == FS_CODE_E_EOF) {
      break;
    }
  }

  *act_size = read_size;

  return msg_buf.data[0];
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

  message_buffer_t msg_buf;
  size_t           written_size = 0;
  const char*      ptr          = reinterpret_cast<const char*>(buf);

  while (size > written_size) {
    msg_buf.cap_part_length  = 1;
    msg_buf.data[0]          = fd;
    msg_buf.data_part_length = 2 + (size - written_size + sizeof(uintptr_t) - 1) / sizeof(uintptr_t);
    msg_buf.data[1]          = FS_MSG_TYPE_WRITE;
    msg_buf.data[2]          = size - written_size;

    memcpy(&msg_buf.data[3], ptr + written_size, size - written_size);

    if (sysret_failed(sys_endpoint_cap_call(fs_info.ep_cap, &msg_buf))) [[unlikely]] {
      errno = FS_CODE_E_FAILURE;
      return 0;
    }

    if (msg_buf.cap_part_length != 0 || msg_buf.data_part_length < 2) [[unlikely]] {
      errno = FS_CODE_E_FAILURE;
      return 0;
    }

    if (msg_buf.data[0] != FS_CODE_S_OK) {
      break;
    }

    size_t length = msg_buf.data[1];
    if (length == 0) [[unlikely]] {
      break;
    }

    written_size += length;
  }

  *act_size = written_size;

  return msg_buf.data[0];
}
