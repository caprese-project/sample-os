#include <cerrno>
#include <cstring>
#include <fs/mount_point.h>
#include <libcaprese/syscall.h>

caprese::id_map<mount_point&> mount_point::fs_mount_point_table;
caprese::id_map<id_cap_t>     mount_point::fd_fs_table;

std::optional<std::reference_wrapper<mount_point>> mount_point::find_mount_point(id_cap_t fd) noexcept {
  if (!fd_fs_table.contains(fd)) [[unlikely]] {
    return std::nullopt;
  }

  id_cap_t fs_id = fd_fs_table.at(fd);
  if (!fs_mount_point_table.contains(fs_id)) [[unlikely]] {
    return std::nullopt;
  }

  return fs_mount_point_table.at(fs_id);
}

mount_point::mount_point(id_cap_t id, endpoint_cap_t ep) noexcept: fs_id(id), fs_ep(ep), mounted(false) { }

mount_point::~mount_point() noexcept {
  (void)this->unmount();

  if (this->fs_id != 0) {
    sys_cap_destroy(this->fs_id);
    this->fs_id = 0;
  }

  if (this->fs_ep != 0) {
    sys_cap_destroy(this->fs_ep);
    this->fs_ep = 0;
  }
}

bool mount_point::mount() noexcept {
  if (this->mounted) [[unlikely]] {
    return false;
  }

  this->mounted = true;
  fs_mount_point_table.emplace(this->fs_id, *this);

  return true;
}

bool mount_point::unmount() noexcept {
  if (!this->mounted) [[unlikely]] {
    return false;
  }

  this->mounted = false;
  fs_mount_point_table.erase(this->fs_id);

  return true;
}

id_cap_t mount_point::get_fs_id() const noexcept {
  return this->fs_id;
}

bool mount_point::is_mounted() const noexcept {
  return this->mounted;
}

std::optional<fs_file_info> mount_point::get_info(std::string_view path) noexcept {
  if (!this->mounted) [[unlikely]] {
    errno = FS_CODE_E_NOT_MOUNTED;
    return std::nullopt;
  }

  size_t     in_size  = sizeof(uintptr_t) * 2 + path.size() + 1;
  size_t     out_size = sizeof(uintptr_t) * 1 + sizeof(fs_file_info);
  message_t* msg      = new_ipc_message(std::max(in_size, out_size));
  if (msg == nullptr) [[unlikely]] {
    errno = FS_CODE_E_FAILURE;
    return std::nullopt;
  }

  set_ipc_data(msg, 0, FS_MSG_TYPE_INFO);
  set_ipc_cap(msg, 1, fs_id, true);
  set_ipc_data_strn(msg, 2, path.data(), path.size());

  if (sysret_failed(sys_endpoint_cap_call(fs_ep, msg))) [[unlikely]] {
    delete_ipc_message(msg);
    errno = FS_CODE_E_FAILURE;
    return std::nullopt;
  }

  if (get_ipc_data(msg, 0) != FS_CODE_S_OK) [[unlikely]] {
    auto err = get_ipc_data(msg, 0);
    delete_ipc_message(msg);
    errno = err;
    return std::nullopt;
  }

  fs_file_info* info = reinterpret_cast<fs_file_info*>(get_ipc_data_ptr(msg, 1));
  if (info == nullptr) [[unlikely]] {
    delete_ipc_message(msg);
    errno = FS_CODE_E_FAILURE;
    return std::nullopt;
  }

  std::optional result(*info);
  delete_ipc_message(msg);

  return result;
}

bool mount_point::create(std::string_view path, int type) noexcept {
  if (!this->mounted) [[unlikely]] {
    errno = FS_CODE_E_NOT_MOUNTED;
    return false;
  }

  message_t* msg = new_ipc_message(sizeof(uintptr_t) * 3 + path.size() + 1);
  if (msg == nullptr) [[unlikely]] {
    errno = FS_CODE_E_FAILURE;
    return false;
  }

  set_ipc_data(msg, 0, FS_MSG_TYPE_CREATE);
  set_ipc_cap(msg, 1, fs_id, true);
  set_ipc_data(msg, 2, type);
  set_ipc_data_array(msg, 3, path.data(), path.size());

  if (sysret_failed(sys_endpoint_cap_call(fs_ep, msg))) [[unlikely]] {
    delete_ipc_message(msg);
    errno = FS_CODE_E_FAILURE;
    return false;
  }

  if (get_ipc_data(msg, 0) != FS_CODE_S_OK) [[unlikely]] {
    auto err = get_ipc_data(msg, 0);
    delete_ipc_message(msg);
    errno = err;
    return false;
  }

  delete_ipc_message(msg);

  return true;
}

bool mount_point::remove(std::string_view path) noexcept {
  if (!this->mounted) [[unlikely]] {
    errno = FS_CODE_E_NOT_MOUNTED;
    return false;
  }

  message_t* msg = new_ipc_message(sizeof(uintptr_t) * 2 + path.size() + 1);
  if (msg == nullptr) [[unlikely]] {
    errno = FS_CODE_E_FAILURE;
    return false;
  }

  set_ipc_data(msg, 0, FS_MSG_TYPE_REMOVE);
  set_ipc_cap(msg, 1, fs_id, true);
  set_ipc_data_strn(msg, 2, path.data(), path.size());

  if (sysret_failed(sys_endpoint_cap_call(fs_ep, msg))) [[unlikely]] {
    delete_ipc_message(msg);
    errno = FS_CODE_E_FAILURE;
    return false;
  }

  if (get_ipc_data(msg, 0) != FS_CODE_S_OK) [[unlikely]] {
    auto err = get_ipc_data(msg, 0);
    delete_ipc_message(msg);
    errno = err;
    return false;
  }

  delete_ipc_message(msg);

  return true;
}

id_cap_t mount_point::open(std::string_view path) noexcept {
  if (!this->mounted) [[unlikely]] {
    errno = FS_CODE_E_NOT_MOUNTED;
    return 0;
  }

  size_t     in_size  = sizeof(uintptr_t) * 2 + path.size() + 1;
  size_t     out_size = sizeof(uintptr_t) * 1 + sizeof(id_cap_t);
  message_t* msg      = new_ipc_message(std::max(in_size, out_size));
  if (msg == nullptr) [[unlikely]] {
    errno = FS_CODE_E_FAILURE;
    return 0;
  }

  set_ipc_data(msg, 0, FS_MSG_TYPE_OPEN);
  set_ipc_cap(msg, 1, fs_id, true);
  set_ipc_data_strn(msg, 2, path.data(), path.size());

  if (sysret_failed(sys_endpoint_cap_call(fs_ep, msg))) [[unlikely]] {
    delete_ipc_message(msg);
    errno = FS_CODE_E_FAILURE;
    return 0;
  }

  if (get_ipc_data(msg, 0) != FS_CODE_S_OK) [[unlikely]] {
    auto err = get_ipc_data(msg, 0);
    delete_ipc_message(msg);
    errno = err;
    return 0;
  }

  id_cap_t fd = move_ipc_cap(msg, 1);
  if (unwrap_sysret(sys_cap_type(fd)) != CAP_ID) [[unlikely]] {
    sys_cap_destroy(fd);
    delete_ipc_message(msg);
    errno = FS_CODE_E_FAILURE;
    return 0;
  }

  delete_ipc_message(msg);

  fd_fs_table.emplace(fd, fs_id);

  return fd;
}

bool mount_point::close(id_cap_t fd) noexcept {
  if (!this->mounted) [[unlikely]] {
    errno = FS_CODE_E_NOT_MOUNTED;
    return false;
  }

  if (!fd_fs_table.contains(fd) || unwrap_sysret(sys_id_cap_compare(fd_fs_table.at(fd), this->fs_id)) != 0) [[unlikely]] {
    errno = FS_CODE_E_ILL_ARGS;
    return false;
  }

  message_t* msg = new_ipc_message(sizeof(uintptr_t) * 2);
  if (msg == nullptr) [[unlikely]] {
    errno = FS_CODE_E_FAILURE;
    return false;
  }

  set_ipc_data(msg, 0, FS_MSG_TYPE_CLOSE);
  set_ipc_cap(msg, 1, fs_id, true);
  set_ipc_cap(msg, 2, fd, true);

  if (sysret_failed(sys_endpoint_cap_call(fs_ep, msg))) [[unlikely]] {
    delete_ipc_message(msg);
    errno = FS_CODE_E_FAILURE;
    return false;
  }

  if (get_ipc_data(msg, 0) != FS_CODE_S_OK) [[unlikely]] {
    auto err = get_ipc_data(msg, 0);
    delete_ipc_message(msg);
    errno = err;
    return false;
  }

  delete_ipc_message(msg);

  fd_fs_table.erase(fd);

  return true;
}

std::streamsize mount_point::read(id_cap_t fd, char* buffer, std::streamsize size) noexcept {
  if (!this->mounted) [[unlikely]] {
    errno = FS_CODE_E_NOT_MOUNTED;
    return -1;
  }

  if (!fd_fs_table.contains(fd) || unwrap_sysret(sys_id_cap_compare(fd_fs_table.at(fd), this->fs_id)) != 0) [[unlikely]] {
    errno = FS_CODE_E_ILL_ARGS;
    return false;
  }

  if (buffer == nullptr) [[unlikely]] {
    errno = FS_CODE_E_ILL_ARGS;
    return -1;
  }

  if (size == 0) [[unlikely]] {
    return 0;
  }

  size_t     msg_buf_size = std::min<size_t>(size, FS_READ_MAX_SIZE);
  message_t* msg          = new_ipc_message(sizeof(uintptr_t) * 4 + msg_buf_size);
  if (msg == nullptr) [[unlikely]] {
    errno = FS_CODE_E_FAILURE;
    return -1;
  }

  char* ptr    = buffer;
  char* end    = buffer + size;
  int   result = FS_CODE_S_OK;

  while (ptr < end) {
    size_t len = std::min<size_t>(end - ptr, msg_buf_size);

    set_ipc_data(msg, 0, FS_MSG_TYPE_READ);
    set_ipc_cap(msg, 1, fs_id, true);
    set_ipc_cap(msg, 2, fd, true);
    set_ipc_data(msg, 3, len);

    if (sysret_failed(sys_endpoint_cap_call(fs_ep, msg))) [[unlikely]] {
      result = FS_CODE_E_FAILURE;
      break;
    }

    result = get_ipc_data(msg, 0);
    if (result != FS_CODE_S_OK && result != FS_CODE_E_EOF) [[unlikely]] {
      break;
    }

    size_t read_size = get_ipc_data(msg, 1);
    if (read_size == 0) [[unlikely]] {
      break;
    }

    const void* data = get_ipc_data_ptr(msg, 2);
    if (data == nullptr) [[unlikely]] {
      result = FS_CODE_E_FAILURE;
      break;
    }

    memcpy(ptr, data, read_size);
    ptr += read_size;

    if (result == FS_CODE_E_EOF) [[unlikely]] {
      break;
    }
  }

  delete_ipc_message(msg);

  if (result != FS_CODE_S_OK && result != FS_CODE_E_EOF) [[unlikely]] {
    errno = result;
    return -1;
  }

  return ptr - buffer;
}

std::streamsize mount_point::write(id_cap_t fd, std::string_view data) noexcept {
  if (!this->mounted) [[unlikely]] {
    errno = FS_CODE_E_NOT_MOUNTED;
    return -1;
  }

  if (!fd_fs_table.contains(fd) || unwrap_sysret(sys_id_cap_compare(fd_fs_table.at(fd), this->fs_id)) != 0) [[unlikely]] {
    errno = FS_CODE_E_ILL_ARGS;
    return -1;
  }

  if (data.empty()) [[unlikely]] {
    return 0;
  }

  size_t     msg_buf_size = std::min<size_t>(data.size(), FS_WRITE_MAX_SIZE);
  message_t* msg          = new_ipc_message(sizeof(uintptr_t) * 4 + msg_buf_size);
  if (msg == nullptr) [[unlikely]] {
    errno = FS_CODE_E_FAILURE;
    return -1;
  }

  const char* ptr    = data.data();
  const char* end    = data.data() + data.size();
  int         result = FS_CODE_S_OK;

  while (ptr < end) {
    size_t len = std::min<size_t>(end - ptr, msg_buf_size);

    set_ipc_data(msg, 0, FS_MSG_TYPE_WRITE);
    set_ipc_cap(msg, 1, fs_id, true);
    set_ipc_cap(msg, 2, fd, true);
    set_ipc_data(msg, 3, len);
    set_ipc_data_array(msg, 4, ptr, len);

    if (sysret_failed(sys_endpoint_cap_call(fs_ep, msg))) [[unlikely]] {
      result = FS_CODE_E_FAILURE;
      break;
    }

    result = get_ipc_data(msg, 0);
    if (result != FS_CODE_S_OK && result != FS_CODE_E_EOF) [[unlikely]] {
      break;
    }

    size_t write_size = get_ipc_data(msg, 1);
    if (write_size == 0) [[unlikely]] {
      break;
    }

    ptr += write_size;
  }

  delete_ipc_message(msg);

  if (result != FS_CODE_S_OK && result != FS_CODE_E_EOF) [[unlikely]] {
    errno = result;
    return -1;
  }

  return ptr - data.data();
}

bool mount_point::seek(id_cap_t fd, std::streamoff offset, int whence) noexcept {
  if (!this->mounted) [[unlikely]] {
    errno = FS_CODE_E_NOT_MOUNTED;
    return false;
  }

  if (!fd_fs_table.contains(fd) || unwrap_sysret(sys_id_cap_compare(fd_fs_table.at(fd), this->fs_id)) != 0) [[unlikely]] {
    errno = FS_CODE_E_ILL_ARGS;
    return false;
  }

  message_t* msg = new_ipc_message(sizeof(uintptr_t) * 4);
  if (msg == nullptr) [[unlikely]] {
    errno = FS_CODE_E_FAILURE;
    return false;
  }

  set_ipc_data(msg, 0, FS_MSG_TYPE_SEEK);
  set_ipc_cap(msg, 1, fs_id, true);
  set_ipc_cap(msg, 2, fd, true);
  set_ipc_data(msg, 3, offset);
  set_ipc_data(msg, 4, whence);

  if (sysret_failed(sys_endpoint_cap_call(fs_ep, msg))) [[unlikely]] {
    delete_ipc_message(msg);
    errno = FS_CODE_E_FAILURE;
    return false;
  }

  if (get_ipc_data(msg, 0) != FS_CODE_S_OK) [[unlikely]] {
    auto err = get_ipc_data(msg, 0);
    delete_ipc_message(msg);
    errno = err;
    return false;
  }

  delete_ipc_message(msg);

  return true;
}

std::streampos mount_point::tell(id_cap_t fd) noexcept {
  if (!this->mounted) [[unlikely]] {
    errno = FS_CODE_E_NOT_MOUNTED;
    return -1;
  }

  if (!fd_fs_table.contains(fd) || unwrap_sysret(sys_id_cap_compare(fd_fs_table.at(fd), this->fs_id)) != 0) [[unlikely]] {
    errno = FS_CODE_E_ILL_ARGS;
    return -1;
  }

  message_t* msg = new_ipc_message(sizeof(uintptr_t) * 2);
  if (msg == nullptr) [[unlikely]] {
    errno = FS_CODE_E_FAILURE;
    return -1;
  }

  set_ipc_data(msg, 0, FS_MSG_TYPE_TELL);
  set_ipc_cap(msg, 1, fs_id, true);
  set_ipc_cap(msg, 2, fd, true);

  if (sysret_failed(sys_endpoint_cap_call(fs_ep, msg))) [[unlikely]] {
    delete_ipc_message(msg);
    errno = FS_CODE_E_FAILURE;
    return -1;
  }

  if (get_ipc_data(msg, 0) != FS_CODE_S_OK) [[unlikely]] {
    auto err = get_ipc_data(msg, 0);
    delete_ipc_message(msg);
    errno = err;
    return -1;
  }

  std::streampos pos = get_ipc_data(msg, 1);
  delete_ipc_message(msg);

  return pos;
}
