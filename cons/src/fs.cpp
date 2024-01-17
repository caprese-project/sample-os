#include <cons/directory.h>
#include <cons/file.h>
#include <cons/fs.h>
#include <cons/server.h>
#include <crt/global.h>
#include <cstdlib>
#include <libcaprese/cxx/id_map.h>
#include <optional>
#include <service/apm.h>
#include <service/fs.h>

namespace {
  std::optional<directory>                      root_directory;
  caprese::id_map<std::reference_wrapper<file>> open_files;

  std::optional<std::reference_wrapper<directory>> create_directories(std::string_view path) {
    if (path.empty() || path[0] == '/') [[unlikely]] {
      return std::nullopt;
    }

    std::reference_wrapper<directory> cur = root_directory.value();

    while (!path.empty()) {
      if (cur.get().get_type() != directory_type::regular_directory) [[unlikely]] {
        return std::nullopt;
      }

      size_t           next_pos = path.find('/');
      std::string_view dirname  = path.substr(0, next_pos);

      if (dirname.empty()) [[unlikely]] {
        return std::nullopt;
      }

      if (!cur.get().contains(dirname)) {
        if (!cur.get().create_directory(dirname)) [[unlikely]] {
          return std::nullopt;
        }
      }

      cur = cur.get().get_dir(dirname);

      if (next_pos == std::string_view::npos || next_pos + 1 == path.size()) [[unlikely]] {
        path = "";
      } else {
        path = path.substr(next_pos + 1);
      }
    }

    return cur;
  }

  std::optional<std::reference_wrapper<directory>> find_directory(std::string_view path) {
    if (path.empty() || path[0] == '/') [[unlikely]] {
      return std::nullopt;
    }

    std::reference_wrapper<directory> cur = root_directory.value();

    while (!path.empty()) {
      if (cur.get().get_type() != directory_type::regular_directory) [[unlikely]] {
        return std::nullopt;
      }

      size_t           next_pos = path.find('/');
      std::string_view dirname  = path.substr(0, next_pos);

      if (dirname.empty()) [[unlikely]] {
        return std::nullopt;
      }

      if (!cur.get().contains(dirname)) [[unlikely]] {
        return std::nullopt;
      }

      cur = cur.get().get_dir(dirname);

      if (next_pos == std::string_view::npos || next_pos + 1 == path.size()) [[unlikely]] {
        path = "";
      } else {
        path = path.substr(next_pos + 1);
      }
    }

    return cur;
  }
} // namespace

bool cons_init() {
  if (root_directory) [[unlikely]] {
    return false;
  }

  root_directory.emplace("");

  auto result = create_directories("tty");
  if (!result) [[unlikely]] {
    return false;
  }

  directory& dir = result->get();

  endpoint_cap_t uart_ep_cap = apm_lookup("uart");
  if (uart_ep_cap == 0) [[unlikely]] {
    return false;
  }

  if (!dir.create_file("0", file_type::uart, uart_ep_cap)) [[unlikely]] {
    return false;
  }

  cons_id_cap = fs_mount(__this_ep_cap, "/cons");
  if (cons_id_cap == 0) [[unlikely]] {
    return false;
  }

  atexit([] { fs_unmount(cons_id_cap); });

  return true;
}

int cons_get_info(std::string_view path, fs_file_info& dst) {
  auto result = find_directory(path);
  if (!result) [[unlikely]] {
    return FS_CODE_E_NO_SUCH_FILE;
  }

  directory& dir = result->get();

  dst.file_type      = static_cast<uint8_t>(dir.get_type());
  dst.file_name_size = dir.get_name().size();
  dst.file_size      = 0;
  std::copy(dir.get_name().begin(), dir.get_name().end(), dst.file_name);

  return FS_CODE_S_OK;
}

int cons_open(std::string_view path, id_cap_t& fd) {
  auto result = find_directory(path);
  if (!result) [[unlikely]] {
    return FS_CODE_E_NO_SUCH_FILE;
  }

  directory& dir = result->get();
  if (dir.get_type() != directory_type::device_file) [[unlikely]] {
    return FS_CODE_E_TYPE;
  }

  fd = unwrap_sysret(sys_id_cap_create());
  open_files.emplace(fd, dir.get_file());

  return FS_CODE_S_OK;
}

int cons_close(id_cap_t fd) {
  if (!open_files.contains(fd)) [[unlikely]] {
    return FS_CODE_E_NO_SUCH_FILE;
  }

  open_files.erase(fd);

  return FS_CODE_S_OK;
}

int cons_read(id_cap_t fd, char* buffer, std::streamsize size, std::streamsize& act_size) {
  if (!open_files.contains(fd)) [[unlikely]] {
    return FS_CODE_E_NO_SUCH_FILE;
  }

  act_size = open_files.at(fd).get().read(buffer, size);

  return FS_CODE_S_OK;
}

int cons_write(id_cap_t fd, std::string_view data, std::streamsize& act_size) {
  if (!open_files.contains(fd)) [[unlikely]] {
    return FS_CODE_E_NO_SUCH_FILE;
  }

  act_size = open_files.at(fd).get().write(data);

  return FS_CODE_S_OK;
}
