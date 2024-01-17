#include <cassert>
#include <cons/directory.h>

directory::directory(std::string_view abs_path): abs_path(abs_path), type(directory_type::regular_directory) { }

directory::directory(std::string_view abs_path, file_type type, endpoint_cap_t ep_cap): abs_path(abs_path), type(directory_type::device_file) {
  this->file.emplace(abs_path, type, ep_cap);
}

const std::string& directory::get_abs_path() const {
  return abs_path;
}

directory_type directory::get_type() const {
  return type;
}

bool directory::contains(std::string_view name) const {
  assert(type == directory_type::regular_directory);
  return subdirs.contains(name);
}

directory& directory::get_dir(std::string_view name) {
  assert(type == directory_type::regular_directory);
  assert(subdirs.contains(name));
  return subdirs.find(name)->second;
}

const directory& directory::get_dir(std::string_view name) const {
  assert(type == directory_type::regular_directory);
  assert(subdirs.contains(name));
  return subdirs.find(name)->second;
}

file& directory::get_file() {
  assert(type == directory_type::device_file);
  return file.value();
}

const file& directory::get_file() const {
  assert(type == directory_type::device_file);
  return file.value();
}

std::string_view directory::get_name() const {
  auto pos = abs_path.rfind('/');
  if (pos == std::string::npos) [[unlikely]] {
    return abs_path;
  }
  return abs_path.substr(abs_path.rfind('/') + 1);
}

bool directory::create_directory(std::string_view name) {
  assert(type == directory_type::regular_directory);

  if (subdirs.contains(name)) [[unlikely]] {
    return false;
  }

  if (name.empty() || name.contains('/')) [[unlikely]] {
    return false;
  }

  subdirs.emplace(name, abs_path + "/" + std::string(name));

  return true;
}

bool directory::create_file(std::string_view name, file_type type, endpoint_cap_t ep_cap) {
  assert(this->type == directory_type::regular_directory);

  if (subdirs.contains(name)) [[unlikely]] {
    return false;
  }

  if (name.empty() || name.contains('/')) [[unlikely]] {
    return false;
  }

  subdirs.emplace(name, directory(abs_path + "/" + std::string(name), type, ep_cap));

  return true;
}
