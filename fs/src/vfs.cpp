#include <cerrno>
#include <fs/dir_stream.h>
#include <fs/vfs.h>
#include <functional>
#include <libcaprese/cxx/id_map.h>
#include <optional>
#include <utility>

namespace {
  std::optional<directory>    root_directory;
  caprese::id_map<dir_stream> dir_streams;
} // namespace

bool vfs_init() {
  if (root_directory) [[unlikely]] {
    return false;
  }

  root_directory.emplace("/");
  return true;
}

int vfs_mount(endpoint_cap_t ep_cap, std::string_view path, id_cap_t& fs_id) {
  if (path.empty() || path.front() != '/') [[unlikely]] {
    return FS_CODE_E_NO_SUCH_FILE;
  }

  path.remove_prefix(1);
  auto mnt_dir = root_directory->create_mount_point(path, ep_cap);
  if (!mnt_dir) {
    return FS_CODE_E_FAILURE;
  }

  fs_id = mnt_dir->get().get_fs_id();

  return FS_CODE_S_OK;
}

int vfs_unmount(std::string_view path) {
  if (path.empty() || path.front() != '/') [[unlikely]] {
    return FS_CODE_E_NO_SUCH_FILE;
  }

  path.remove_prefix(1);
  if (!root_directory->remove_mount_point(path)) {
    return FS_CODE_E_FAILURE;
  }

  return FS_CODE_S_OK;
}

int vfs_mounted(std::string_view path, bool* mounted) {
  if (path.empty() || path.front() != '/') [[unlikely]] {
    return FS_CODE_E_NO_SUCH_FILE;
  }

  path.remove_prefix(1);
  *mounted = root_directory->is_mounted(path);

  return FS_CODE_S_OK;
}

int vfs_get_info(std::string_view path, fs_file_info& dst) {
  if (path.empty() || path.front() != '/') [[unlikely]] {
    return FS_CODE_E_NO_SUCH_FILE;
  }

  path.remove_prefix(1);

  if (auto result = root_directory->find_mount_point(path)) {
    auto& [directory, subpath] = result.value();
    auto info                  = directory.get().get_info(subpath);

    if (!info) [[unlikely]] {
      return FS_CODE_E_FAILURE;
    }

    dst = info.value();

    return FS_CODE_S_OK;
  }

  if (auto result = root_directory->find_directory(path)) {
    if (!result->get().get_info(&dst)) [[unlikely]] {
      return FS_CODE_E_FAILURE;
    }

    return FS_CODE_S_OK;
  }

  return FS_CODE_E_NO_SUCH_FILE;
}

int vfs_create(std::string_view path, int type) {
  if (path.empty() || path.front() != '/') [[unlikely]] {
    return FS_CODE_E_NO_SUCH_FILE;
  }

  if (type < FS_FT_REG || type > FS_FT_DIR) [[unlikely]] {
    return FS_CODE_E_ILL_ARGS;
  }

  path.remove_prefix(1);
  auto result = root_directory->find_mount_point(path);
  if (!result) {
    return FS_CODE_E_NO_SUCH_FILE;
  }

  auto& [directory, subpath] = result.value();
  if (!directory.get().create(subpath, type)) {
    return FS_CODE_E_FAILURE;
  }

  return FS_CODE_S_OK;
}

int vfs_remove(std::string_view path) {
  if (path.empty() || path.front() != '/') [[unlikely]] {
    return FS_CODE_E_NO_SUCH_FILE;
  }

  path.remove_prefix(1);
  auto result = root_directory->find_mount_point(path);
  if (!result) {
    return FS_CODE_E_NO_SUCH_FILE;
  }

  auto& [directory, subpath] = result.value();
  if (!directory.get().remove(subpath)) {
    return FS_CODE_E_FAILURE;
  }

  return FS_CODE_S_OK;
}

int vfs_open(std::string_view path, id_cap_t& fd) {
  if (path.empty() || path.front() != '/') [[unlikely]] {
    return FS_CODE_E_NO_SUCH_FILE;
  }

  path.remove_prefix(1);

  if (auto result = root_directory->find_mount_point(path)) {
    auto& [directory, subpath] = result.value();
    fd                         = directory.get().open(subpath);
    if (fd == 0) {
      return FS_CODE_E_FAILURE;
    }

    return FS_CODE_S_OK;
  }

  if (auto result = root_directory->find_directory(path)) {
    fd = unwrap_sysret(sys_id_cap_create());
    dir_streams.emplace(fd, result->get());
    return FS_CODE_S_OK;
  }

  return FS_CODE_E_NO_SUCH_FILE;
}

int vfs_close(id_cap_t fd) {
  if (dir_streams.contains(fd)) {
    dir_streams.erase(fd);
    return FS_CODE_S_OK;
  }

  auto mnt = mount_point::find_mount_point(fd);
  if (!mnt) {
    return FS_CODE_E_NO_SUCH_FILE;
  }

  if (!mnt->get().close(fd)) {
    return FS_CODE_E_FAILURE;
  }

  return FS_CODE_S_OK;
}

int vfs_read(id_cap_t fd, char* buffer, std::streamsize size, std::streamsize& act_size) {
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

  auto mnt = mount_point::find_mount_point(fd);
  if (!mnt) {
    return FS_CODE_E_NO_SUCH_FILE;
  }

  act_size = mnt->get().read(fd, buffer, size);
  if (act_size < 0) {
    return FS_CODE_E_FAILURE;
  }

  return FS_CODE_S_OK;
}

int vfs_write(id_cap_t fd, std::string_view data, std::streamsize& act_size) {
  auto mnt = mount_point::find_mount_point(fd);
  if (!mnt) {
    return FS_CODE_E_NO_SUCH_FILE;
  }

  act_size = mnt->get().write(fd, data);
  if (act_size < 0) {
    return FS_CODE_E_FAILURE;
  }

  return FS_CODE_S_OK;
}

int vfs_seek(id_cap_t fd, std::streamoff offset, int whence) {
  auto mnt = mount_point::find_mount_point(fd);
  if (!mnt) {
    return FS_CODE_E_NO_SUCH_FILE;
  }

  if (!mnt->get().seek(fd, offset, whence)) {
    return FS_CODE_E_FAILURE;
  }

  return FS_CODE_S_OK;
}

int vfs_tell(id_cap_t fd, std::streampos& dst) {
  auto mnt = mount_point::find_mount_point(fd);
  if (!mnt) {
    return FS_CODE_E_NO_SUCH_FILE;
  }

  dst = mnt->get().tell(fd);
  if (dst < 0) {
    return FS_CODE_E_FAILURE;
  }

  return FS_CODE_S_OK;
}
