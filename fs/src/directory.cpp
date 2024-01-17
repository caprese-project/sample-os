#include <cassert>
#include <fs/directory.h>
#include <libcaprese/syscall.h>
#include <utility>

directory::directory(std::string_view abs_path): abs_path(abs_path), type(directory_type::regular_directory) { }

const std::string& directory::get_abs_path() const {
  return abs_path;
}

directory_type directory::get_type() const {
  return type;
}

std::string_view directory::get_name() const {
  std::string_view path(abs_path);
  auto             pos = path.rfind('/');

  if (pos != std::string_view::npos) {
    path.remove_prefix(pos + 1);
  }

  return path;
}

bool directory::contains(std::string_view name) const {
  return subdirs.contains(name);
}

directory& directory::get_dir(std::string_view name) {
  assert(subdirs.contains(name));
  return subdirs.find(name)->second;
}

const directory& directory::get_dir(std::string_view name) const {
  assert(subdirs.contains(name));
  return subdirs.find(name)->second;
}

bool directory::mount(endpoint_cap_t ep_cap) {
  if (mnt) [[unlikely]] {
    return false;
  }

  sysret_t sysret = sys_id_cap_create();
  if (sysret_failed(sysret)) [[unlikely]] {
    return false;
  }

  id_cap_t fs_id = sysret.result;
  mnt.emplace(fs_id, ep_cap);

  if (!mnt->mount()) [[unlikely]] {
    return false;
  }

  type = directory_type::mount_point;

  return true;
}

id_cap_t directory::get_fs_id() const {
  if (!mnt || !mnt->is_mounted()) [[unlikely]] {
    return 0;
  }

  return mnt->get_fs_id();
}

bool directory::get_info(fs_file_info* buffer) const {
  buffer->file_size = 0;

  switch (this->type) {
    case directory_type::regular_directory:
      buffer->file_type = FS_FT_DIR;
      break;
    case directory_type::mount_point:
      buffer->file_type = FS_FT_MNT;
      break;
    default:
      return false;
  }

  std::string_view name  = get_name();
  buffer->file_name_size = name.size();
  name.copy(buffer->file_name, sizeof(buffer->file_name) - 1);
  buffer->file_name[buffer->file_name_size] = '\0';

  return true;
}

std::streamsize directory::read(std::streampos pos, fs_file_info* buffer) {
  if (mnt) [[unlikely]] {
    return 0;
  }

  if (pos < 0 || subdirs.size() <= static_cast<size_t>(pos)) [[unlikely]] {
    return 0;
  }

  auto iter = subdirs.begin();
  std::advance(iter, static_cast<size_t>(pos));

  if (!iter->second.get_info(buffer)) [[unlikely]] {
    return 0;
  }

  return sizeof(fs_file_info);
}

std::optional<std::reference_wrapper<directory>> directory::find_directory(std::string_view path) {
  if (path.empty()) {
    return *this;
  }

  if (path.front() == '/') [[unlikely]] {
    return std::nullopt;
  }

  if (path.back() == '/') {
    path.remove_suffix(1);
  }

  std::reference_wrapper<directory> cur = *this;

  while (!path.empty()) {
    size_t           next_pos = path.find('/');
    std::string_view dirname  = path.substr(0, next_pos);

    if (dirname.empty()) [[unlikely]] {
      return std::nullopt;
    }

    if (!cur.get().contains(dirname)) [[unlikely]] {
      return cur;
    }

    cur = cur.get().get_dir(dirname);

    if (next_pos == std::string_view::npos) [[unlikely]] {
      path = "";
    } else {
      path = path.substr(next_pos + 1);
    }
  }

  return cur;
}

std::optional<std::pair<std::reference_wrapper<directory>, std::string_view>> directory::find_mount_point(std::string_view path) {
  if (path.empty()) {
    if (this->mnt) {
      return std::pair(std::ref(*this), path);
    } else {
      return std::nullopt;
    }
  }

  if (path.front() == '/') [[unlikely]] {
    return std::nullopt;
  }

  if (path.back() == '/') {
    path.remove_suffix(1);
  }

  std::optional<std::pair<std::reference_wrapper<directory>, std::string_view>> result;

  std::reference_wrapper<directory> cur = *this;

  if (cur.get().get_type() == directory_type::mount_point) {
    result.emplace(cur, path);
  }

  while (!path.empty()) {
    size_t           next_pos = path.find('/');
    std::string_view dirname  = path.substr(0, next_pos);

    if (dirname.empty()) [[unlikely]] {
      return std::nullopt;
    }

    if (!cur.get().contains(dirname)) [[unlikely]] {
      break;
    }

    cur = cur.get().get_dir(dirname);

    if (next_pos == std::string_view::npos) [[unlikely]] {
      path = "";
    } else {
      path = path.substr(next_pos + 1);
    }

    if (cur.get().type == directory_type::mount_point) {
      result.emplace(cur, path);
    }
  }

  return result;
}

std::optional<std::reference_wrapper<directory>> directory::create_directories(std::string_view path) {
  if (path.empty() || path.front() == '/') [[unlikely]] {
    return std::nullopt;
  }

  if (path.back() == '/') {
    path.remove_suffix(1);
  }

  std::reference_wrapper<directory> cur = *this;
  size_t                            pos = 0;

  while (true) {
    size_t           next_pos = path.find('/', pos);
    std::string_view dirname  = path.substr(pos, next_pos);

    if (dirname.empty()) [[unlikely]] {
      return std::nullopt;
    }

    if (!cur.get().contains(dirname)) [[unlikely]] {
      cur.get().subdirs.emplace(dirname, path.substr(0, next_pos));
    }

    cur = cur.get().get_dir(dirname);

    if (next_pos == path.npos) [[unlikely]] {
      break;
    }

    pos = next_pos + 1;
  }

  return cur;
}

std::optional<std::reference_wrapper<directory>> directory::create_mount_point(std::string_view path, endpoint_cap_t ep) {
  if (path.empty()) {
    if (!mount(ep)) [[unlikely]] {
      return std::nullopt;
    }

    return *this;
  }

  std::optional<std::reference_wrapper<directory>> dir = create_directories(path);
  if (!dir) [[unlikely]] {
    return std::nullopt;
  }

  if (!dir->get().mount(ep)) [[unlikely]] {
    return std::nullopt;
  }

  return dir;
}

bool directory::remove_mount_point(std::string_view path) {
  if (path.empty()) {
    if (type != directory_type::mount_point) [[unlikely]] {
      return false;
    }

    if (!mnt->unmount()) [[unlikely]] {
      return false;
    }

    mnt.reset();

    return true;
  }

  auto result = find_mount_point(path);
  if (!result) [[unlikely]] {
    return false;
  }

  if (!result->second.empty()) [[unlikely]] {
    return false;
  }

  auto& dir = result->first.get();

  if (dir.type != directory_type::mount_point) [[unlikely]] {
    return false;
  }

  if (!dir.mnt->unmount()) [[unlikely]] {
    return false;
  }

  dir.mnt.reset();

  return true;
}

bool directory::is_mounted(std::string_view path) {
  if (path.empty()) {
    return type == directory_type::mount_point && mnt->is_mounted();
  }

  auto result = find_mount_point(path);
  if (!result) [[unlikely]] {
    return false;
  }

  if (!result->second.empty()) [[unlikely]] {
    return false;
  }

  auto& dir = result->first.get();

  return dir.type == directory_type::mount_point && dir.mnt->is_mounted();
}

std::optional<fs_file_info> directory::get_info(std::string_view path) noexcept {
  if (this->type != directory_type::mount_point) [[unlikely]] {
    return std::nullopt;
  }

  return this->mnt->get_info(path);
}

bool directory::create(std::string_view path, int type) noexcept {
  if (this->type != directory_type::mount_point) [[unlikely]] {
    return false;
  }

  return this->mnt->create(path, type);
}

bool directory::remove(std::string_view path) noexcept {
  if (this->type != directory_type::mount_point) [[unlikely]] {
    return false;
  }

  return this->mnt->remove(path);
}

id_cap_t directory::open(std::string_view path) noexcept {
  if (this->type != directory_type::mount_point) [[unlikely]] {
    return false;
  }

  return this->mnt->open(path);
}

bool directory::close(id_cap_t id) noexcept {
  if (this->type != directory_type::mount_point) [[unlikely]] {
    return false;
  }

  return this->mnt->close(id);
}

std::streamsize directory::read(id_cap_t id, char* buffer, std::streamsize size) noexcept {
  if (this->type != directory_type::mount_point) [[unlikely]] {
    return false;
  }

  return this->mnt->read(id, buffer, size);
}

std::streamsize directory::write(id_cap_t id, std::string_view data) noexcept {
  if (this->type != directory_type::mount_point) [[unlikely]] {
    return false;
  }

  return this->mnt->write(id, data);
}

bool directory::seek(id_cap_t id, std::streamoff offset, int whence) noexcept {
  if (this->type != directory_type::mount_point) [[unlikely]] {
    return false;
  }

  return this->mnt->seek(id, offset, whence);
}

std::streampos directory::tell(id_cap_t id) noexcept {
  if (this->type != directory_type::mount_point) [[unlikely]] {
    return false;
  }

  return this->mnt->tell(id);
}
