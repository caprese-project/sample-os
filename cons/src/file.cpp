#include <cons/file.h>
#include <cons/uart_file.h>
#include <fs/ipc.h>
#include <libcaprese/cxx/id_map.h>
#include <libcaprese/syscall.h>
#include <map>

namespace {
  struct file {
    file_type      type;
    endpoint_cap_t ep_cap;
    void (*putc)(endpoint_cap_t ep_cap, char ch);
    char (*getc)(endpoint_cap_t ep_cap);
  };

  std::map<std::string, file>  file_table;
  caprese::id_map<std::string> open_files;
} // namespace

bool cons_mkfile(const std::string& path, file_type type, endpoint_cap_t ep_cap) {
  if (file_table.contains(path)) [[unlikely]] {
    return false;
  }

  if (ep_cap == 0 || unwrap_sysret(sys_cap_type(ep_cap)) != CAP_ENDPOINT) [[unlikely]] {
    return false;
  }

  file f;
  f.type   = type;
  f.ep_cap = ep_cap;

  switch (type) {
    case file_type::uart:
      f.putc = uart_putc;
      f.getc = uart_getc;
      break;
    default:
      return false;
  }

  file_table.emplace(path, f);

  return true;
}

bool cons_rmfile(const std::string& path) {
  if (!file_table.contains(path)) [[unlikely]] {
    return false;
  }

  file_table.erase(path);

  return true;
}

int cons_open(const std::string& path, id_cap_t* dst_fd) {
  if (!file_table.contains(path)) [[unlikely]] {
    return FS_CODE_E_NO_SUCH_FILE;
  }

  if (dst_fd == NULL) [[unlikely]] {
    return FS_CODE_E_ILL_ARGS;
  }

  id_cap_t fd = unwrap_sysret(sys_id_cap_create());
  if (fd == 0) [[unlikely]] {
    return FS_CODE_E_FAILURE;
  }

  open_files.emplace(fd, path);

  *dst_fd = fd;

  return FS_CODE_S_OK;
}

int cons_close(id_cap_t fd) {
  if (unwrap_sysret(sys_cap_type(fd)) != CAP_ID) [[unlikely]] {
    return FS_CODE_E_ILL_ARGS;
  }

  if (!open_files.contains(fd)) [[unlikely]] {
    return FS_CODE_E_ILL_ARGS;
  }

  open_files.erase(fd);

  return FS_CODE_S_OK;
}

int cons_read(id_cap_t fd, void* buf, size_t max_size, size_t* act_size) {
  if (unwrap_sysret(sys_cap_type(fd)) != CAP_ID) [[unlikely]] {
    return FS_CODE_E_ILL_ARGS;
  }

  if (!open_files.contains(fd)) [[unlikely]] {
    return FS_CODE_E_ILL_ARGS;
  }

  std::string path = open_files.at(fd);

  if (!file_table.contains(path)) [[unlikely]] {
    return FS_CODE_E_NO_SUCH_FILE;
  }

  file& f = file_table.at(path);

  if (buf == NULL) [[unlikely]] {
    return FS_CODE_E_ILL_ARGS;
  }

  char* cbuf = static_cast<char*>(buf);

  for (size_t i = 0; i < max_size; ++i) {
    char ch = -1;

    while (ch == static_cast<char>(-1)) {
      ch = f.getc(f.ep_cap);
    }

    cbuf[i] = ch;

    if (ch == '\n') [[unlikely]] {
      *act_size = i + 1;
      return FS_CODE_S_OK;
    }
  }

  *act_size = max_size;

  return FS_CODE_S_OK;
}

int cons_write(id_cap_t fd, const void* buf, size_t size, size_t* act_size) {
  if (unwrap_sysret(sys_cap_type(fd)) != CAP_ID) [[unlikely]] {
    return FS_CODE_E_ILL_ARGS;
  }

  if (!open_files.contains(fd)) [[unlikely]] {
    return FS_CODE_E_ILL_ARGS;
  }

  std::string path = open_files.at(fd);

  if (!file_table.contains(path)) [[unlikely]] {
    return FS_CODE_E_NO_SUCH_FILE;
  }

  file& f = file_table.at(path);

  if (buf == NULL) [[unlikely]] {
    return FS_CODE_E_ILL_ARGS;
  }

  const char* cbuf = static_cast<const char*>(buf);

  for (size_t i = 0; i < size; ++i) {
    f.putc(f.ep_cap, cbuf[i]);
  }

  *act_size = size;

  return FS_CODE_S_OK;
}
