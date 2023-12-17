#include <apm/cpio.h>
#include <apm/ramfs.h>
#include <cassert>
#include <cstddef>
#include <cstring>
#include <string>

namespace {
  template<typename T, typename U>
  T round_up(T value, U align) {
    return (value + align - 1) / align * align;
  }
} // namespace

ramfs::ramfs(const char* begin, const char* end): _data(begin), _size(end - begin) {
  assert(begin != nullptr);
  assert(end != nullptr);
  assert(begin < end);
}

bool ramfs::has_file(const char* name) const {
  assert(name != nullptr);

  return find_file(name).first != nullptr;
}

std::istringstream ramfs::open_file(const char* name) const {
  assert(name != nullptr);

  auto [ptr, size] = find_file(name);
  if (ptr != nullptr) {
    return std::istringstream(std::string(ptr, size), std::istringstream::binary);
  } else {
    std::istringstream fail;
    fail.setstate(std::ios_base::failbit);
    return fail;
  }
}

std::pair<const char*, size_t> ramfs::find_file(const char* name) const {
  assert(name != nullptr);

  const char* ptr = _data;
  while (ptr < _data + _size) {
    const cpio_newc_header& header = *reinterpret_cast<const cpio_newc_header*>(ptr);

    if (strncmp(header.magic, "070701", 6)) [[unlikely]] {
      break;
    }

    size_t      name_size = std::stoull(std::string(header.namesize, sizeof(header.namesize)), nullptr, 16);
    std::string cur_name(ptr + sizeof(cpio_newc_header), name_size - 1); // The string length in cpio newc includes the null terminator.

    if (cur_name == "TRAILER!!!") [[unlikely]] {
      break;
    }

    size_t file_size = std::stoull(std::string(header.filesize, sizeof(header.filesize)), nullptr, 16);

    if (cur_name == name) {
      return std::pair<const char*, size_t> {
        ptr + round_up(sizeof(cpio_newc_header) + name_size, 4),
        file_size,
      };
    }

    ptr += round_up(sizeof(cpio_newc_header) + name_size, 4) + round_up(file_size, 4);
  }

  return std::pair<const char*, size_t> { nullptr, 0 };
}
