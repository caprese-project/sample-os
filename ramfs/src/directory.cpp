#include <ramfs/directory.h>

directory::directory(std::string_view abs_path): abs_path(abs_path) { }

const std::string& directory::get_abs_path() const {
  return abs_path;
}

std::string_view directory::get_name() const {
  std::string_view path(abs_path);
  auto             pos = path.rfind('/');

  if (pos != std::string_view::npos) {
    path.remove_prefix(pos + 1);
  }

  return path;
}

bool directory::has_dir(std::string_view name) const {
  return dirs.contains(name);
}

bool directory::has_file(std::string_view name) const {
  return files.contains(name);
}

directory& directory::get_dir(std::string_view name) {
  return dirs.find(name)->second;
}

file& directory::get_file(std::string_view name) {
  return files.find(name)->second;
}

const directory& directory::get_dir(std::string_view name) const {
  return dirs.find(name)->second;
}

const file& directory::get_file(std::string_view name) const {
  return files.find(name)->second;
}

std::streamsize directory::read(std::streampos pos, fs_file_info* buffer) {
  size_t total_size = dirs.size() + files.size();

  if (pos < 0 || total_size <= static_cast<size_t>(pos)) [[unlikely]] {
    return 0;
  }

  if (static_cast<size_t>(pos) < dirs.size()) {
    auto iter = dirs.begin();
    std::advance(iter, static_cast<size_t>(pos));

    buffer->file_size      = 0;
    buffer->file_type      = FS_FT_DIR;
    buffer->file_name_size = iter->first.size();
    iter->first.copy(buffer->file_name, sizeof(buffer->file_name) - 1);
    buffer->file_name[buffer->file_name_size] = '\0';

    return 1;
  } else {
    auto iter = files.begin();
    std::advance(iter, static_cast<size_t>(pos) - dirs.size());

    buffer->file_size      = iter->second.size();
    buffer->file_type      = FS_FT_REG;
    buffer->file_name_size = iter->first.size();
    iter->first.copy(buffer->file_name, sizeof(buffer->file_name) - 1);
    buffer->file_name[buffer->file_name_size] = '\0';

    return 1;
  }
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

    if (!cur.get().has_dir(dirname)) [[unlikely]] {
      return std::nullopt;
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

std::optional<std::reference_wrapper<file>> directory::find_file(std::string_view path) {
  if (path.empty() || path.front() == '/' || path.back() == '/') [[unlikely]] {
    return std::nullopt;
  }

  std::reference_wrapper<directory> parent = *this;
  std::string_view                  name;

  auto pos = path.rfind('/');
  if (pos != path.npos) {
    name                                                      = path.substr(pos + 1);
    std::string_view                                 dir_path = path.substr(0, pos);
    std::optional<std::reference_wrapper<directory>> dir      = find_directory(dir_path);

    if (!dir) [[unlikely]] {
      return std::nullopt;
    }

    parent = dir.value();
  } else {
    name = path;
  }

  if (!parent.get().has_file(name)) [[unlikely]] {
    return std::nullopt;
  }

  return parent.get().get_file(name);
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

    if (cur.get().has_file(dirname)) [[unlikely]] {
      return std::nullopt;
    }

    if (!cur.get().has_dir(dirname)) {
      cur.get().dirs.emplace(dirname, path.substr(0, next_pos));
    }

    cur = cur.get().get_dir(dirname);

    if (next_pos == path.npos) [[unlikely]] {
      break;
    }

    pos = next_pos + 1;
  }

  return cur;
}

std::optional<std::reference_wrapper<file>> directory::create_file(std::string_view path, std::vector<char>&& data) {
  if (path.empty() || path.front() == '/' || path.back() == '/') [[unlikely]] {
    return std::nullopt;
  }

  std::reference_wrapper<directory> parent = *this;
  std::string_view                  name;

  auto pos = path.rfind('/');
  if (pos != path.npos) {
    name                                                      = path.substr(pos + 1);
    std::string_view                                 dir_path = path.substr(0, pos);
    std::optional<std::reference_wrapper<directory>> dir      = create_directories(dir_path);

    if (!dir) [[unlikely]] {
      return std::nullopt;
    }

    parent = dir.value();
  } else {
    name = path;
  }

  if (parent.get().has_file(name)) [[unlikely]] {
    return std::nullopt;
  }

  parent.get().files.emplace(name, file(path, std::move(data)));

  return parent.get().get_file(name);
}

bool directory::remove(std::string_view path) {
  if (path.empty() || path.front() == '/') [[unlikely]] {
    return false;
  }

  bool dir_only = false;
  if (path.back() == '/') {
    dir_only = true;
    path.remove_suffix(1);
  }

  std::reference_wrapper<directory> parent = *this;
  std::string_view                  name;

  auto pos = path.rfind('/');
  if (pos != path.npos) {
    name                                                      = path.substr(pos + 1);
    std::string_view                                 dir_path = path.substr(0, pos);
    std::optional<std::reference_wrapper<directory>> dir      = find_directory(dir_path);

    if (!dir) [[unlikely]] {
      return false;
    }

    parent = dir.value();
  } else {
    name = path;
  }

  // TODO: move to trash (ref_counter)

  if (parent.get().has_dir(name)) {
    parent.get().dirs.erase(name);
  } else if (!dir_only && parent.get().has_file(name)) {
    parent.get().files.erase(name);
  } else {
    return false;
  }

  return true;
}
