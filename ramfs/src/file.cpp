#include <ramfs/file.h>
#include <utility>

file::file(std::string_view abs_path, std::vector<char>&& data): abs_path(abs_path), data(std::move(data)) { }

const std::string& file::get_abs_path() const {
  return abs_path;
}

std::string_view file::get_name() const {
  std::string_view path(abs_path);
  auto             pos = path.rfind('/');

  if (pos != std::string_view::npos) {
    path.remove_prefix(pos + 1);
  }

  return path;
}

std::streamsize file::size() const {
  return data.size();
}

std::streamsize file::read(std::streampos pos, char* buffer, std::streamsize size) {
  if (pos < 0 || static_cast<size_t>(pos) >= data.size()) {
    return 0;
  }

  const auto act_size = std::min(size, static_cast<std::streamsize>(data.size() - pos));
  std::copy_n(data.data() + pos, act_size, buffer);
  return act_size;
}

std::streamsize file::write(std::streampos pos, std::string_view data) {
  if (pos < 0 || static_cast<size_t>(pos) + data.size() >= this->data.size()) {
    this->data.resize(static_cast<size_t>(pos) + data.size());
  }

  std::copy_n(data.data(), data.size(), this->data.data() + pos);

  return data.size();
}
