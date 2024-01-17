#include <cstdlib>
#include <cstring>
#include <ramfs/cpio.h>

namespace {
  size_t round_up(size_t value, size_t alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
  }
} // namespace

bool load_cpio(directory& root_dir, const char* cpio_data) {
  const char* ptr = cpio_data;
  while (true) {
    const cpio_newc_header* header = (const cpio_newc_header*)ptr;

    if (strncmp(header->magic, CPIO_MAGIC, sizeof(header->magic)) != 0) {
      return false;
    }

    char name_size_str[sizeof(header->namesize) + 1];
    memcpy(name_size_str, header->namesize, sizeof(header->namesize));
    name_size_str[sizeof(header->namesize)] = '\0';

    size_t name_size = strtoull(name_size_str, nullptr, 16);

    char data_size_str[sizeof(header->filesize) + 1];
    memcpy(data_size_str, header->filesize, sizeof(header->filesize));
    data_size_str[sizeof(header->filesize)] = '\0';

    size_t data_size = strtoull(data_size_str, nullptr, 16);

    const char* name = ptr + sizeof(cpio_newc_header);
    const char* data = ptr + round_up(sizeof(cpio_newc_header) + name_size, 4);

    if (strcmp(name, CPIO_EOF_TAG) == 0) {
      break;
    } else {
      if (!root_dir.create_file(std::string_view(name, name_size - 1), std::vector(data, data + data_size))) [[unlikely]] {
        return false;
      }

      ptr = data + round_up(data_size, 4);
    }
  }

  return true;
}
