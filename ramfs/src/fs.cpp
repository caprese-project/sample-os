#include <crt/global.h>
#include <cstdlib>
#include <libcaprese/cxx/id_map.h>
#include <optional>
#include <ramfs/cpio.h>
#include <ramfs/dir_stream.h>
#include <ramfs/directory.h>
#include <ramfs/file.h>
#include <ramfs/file_stream.h>
#include <ramfs/fs.h>
#include <ramfs/server.h>
#include <service/fs.h>

namespace {
  std::optional<directory>     root_directory;
  caprese::id_map<file_stream> file_streams;
  caprese::id_map<dir_stream>  dir_streams;

  int get_file_info(file& file, fs_file_info& dst) {
    std::string_view name = file.get_name();

    dst.file_type                     = FS_FT_REG;
    dst.file_size                     = file.size();
    dst.file_name_size                = name.copy(dst.file_name, FS_FILE_NAME_SIZE_MAX - 1, 0);
    dst.file_name[dst.file_name_size] = '\0';

    return FS_CODE_S_OK;
  }

  int get_dir_info(directory& dir, fs_file_info& dst) {
    std::string_view name = dir.get_name();

    dst.file_type                     = FS_FT_DIR;
    dst.file_size                     = 0;
    dst.file_name_size                = name.copy(dst.file_name, FS_FILE_NAME_SIZE_MAX - 1, 0);
    dst.file_name[dst.file_name_size] = '\0';

    return FS_CODE_S_OK;
  }
} // namespace

bool ramfs_init(uintptr_t ramfs_va_base) {
  if (root_directory) [[unlikely]] {
    return false;
  }

  root_directory.emplace("");

  const char* cpio_data = reinterpret_cast<const char*>(ramfs_va_base);
  if (!load_cpio(*root_directory, cpio_data)) [[unlikely]] {
    return false;
  }

  ramfs_id_cap = fs_mount(__this_ep_cap, "/init");
  if (ramfs_id_cap == 0) [[unlikely]] {
    return false;
  }

  atexit([] { fs_unmount(ramfs_id_cap); });

  return true;
}

int ramfs_get_info(std::string_view path, fs_file_info& dst) {
  auto file = root_directory->find_file(path);
  if (file) {
    return get_file_info(*file, dst);
  }

  auto dir = root_directory->find_directory(path);
  if (dir) {
    return get_dir_info(*dir, dst);
  }

  return FS_CODE_E_NO_SUCH_FILE;
}

int ramfs_create(std::string_view path, int type) {
  if (type == FS_FT_REG) {
    if (!root_directory->create_file(path)) {
      return FS_CODE_E_FAILURE;
    }
  } else if (type == FS_FT_DIR) {
    if (!root_directory->create_directories(path)) {
      return FS_CODE_E_FAILURE;
    }
  } else {
    return FS_CODE_E_ILL_ARGS;
  }

  return FS_CODE_S_OK;
}

int ramfs_remove(std::string_view path) {
  if (!root_directory->remove(path)) {
    return FS_CODE_E_NO_SUCH_FILE;
  }

  return FS_CODE_S_OK;
}

int ramfs_open(std::string_view path, id_cap_t& fd) {
  auto file = root_directory->find_file(path);
  if (file) {
    fd = unwrap_sysret(sys_id_cap_create());
    file_streams.emplace(fd, *file);
    return FS_CODE_S_OK;
  }

  auto dir = root_directory->find_directory(path);
  if (dir) {
    fd = unwrap_sysret(sys_id_cap_create());
    dir_streams.emplace(fd, *dir);
    return FS_CODE_S_OK;
  }

  return FS_CODE_E_NO_SUCH_FILE;
}

int ramfs_close(id_cap_t fd) {
  if (file_streams.contains(fd)) {
    file_streams.erase(fd);
    return FS_CODE_S_OK;
  }

  if (dir_streams.contains(fd)) {
    dir_streams.erase(fd);
    return FS_CODE_S_OK;
  }

  return FS_CODE_E_ILL_ARGS;
}

int ramfs_read(id_cap_t fd, char* buffer, std::streamsize size, std::streamsize& act_size) {
  if (file_streams.contains(fd)) {
    act_size = file_streams.at(fd).read(buffer, size);

    if (size > act_size) [[unlikely]] {
      return FS_CODE_E_EOF;
    }

    return FS_CODE_S_OK;
  }

  if (dir_streams.contains(fd)) {
    if (size < static_cast<std::streamsize>(sizeof(fs_file_info))) [[unlikely]] {
      return FS_CODE_E_ILL_ARGS;
    }

    act_size = dir_streams.at(fd).read(reinterpret_cast<fs_file_info*>(buffer));

    if (act_size == 0) [[unlikely]] {
      return FS_CODE_E_EOF;
    }

    return FS_CODE_S_OK;
  }

  return FS_CODE_E_ILL_ARGS;
}

int ramfs_write(id_cap_t fd, std::string_view data, std::streamsize& act_size) {
  if (!file_streams.contains(fd)) [[unlikely]] {
    return FS_CODE_E_ILL_ARGS;
  }

  act_size = file_streams.at(fd).write(data);

  if (data.size() > static_cast<size_t>(act_size)) [[unlikely]] {
    return FS_CODE_E_EOF;
  }

  return FS_CODE_S_OK;
}

int ramfs_seek(id_cap_t fd, std::streamoff off, std::ios_base::seekdir dir) {
  if (!file_streams.contains(fd)) [[unlikely]] {
    return FS_CODE_E_ILL_ARGS;
  }

  if (!file_streams.at(fd).seek(off, dir)) {
    return FS_CODE_E_FAILURE;
  }

  return FS_CODE_S_OK;
}

int ramfs_tell(id_cap_t fd, std::streampos& pos) {
  if (!file_streams.contains(fd)) [[unlikely]] {
    return FS_CODE_E_ILL_ARGS;
  }

  pos = file_streams.at(fd).tell();

  return FS_CODE_S_OK;
}
