#include <assert.h>
#include <init/cpio.h>
#include <init/util.h>
#include <stdlib.h>
#include <string.h>

const char* cpio_find_file(const char* cpio, size_t cpio_size, const char* file_name, size_t* file_size) {
  assert(cpio != NULL);
  assert(file_name != NULL);
  assert(file_size != NULL);

  const char* ptr = cpio;
  while (ptr < cpio + cpio_size) {
    const struct cpio_newc_header* header = (const struct cpio_newc_header*)ptr;

    if (strncmp(header->magic, CPIO_MAGIC, sizeof(header->magic)) != 0) {
      return NULL;
    }

    char name_size_str[sizeof(header->namesize) + 1];
    memcpy(name_size_str, header->namesize, sizeof(header->namesize));
    name_size_str[sizeof(header->namesize)] = '\0';

    size_t name_size = strtoull(name_size_str, NULL, 16);

    char data_size_str[sizeof(header->filesize) + 1];
    memcpy(data_size_str, header->filesize, sizeof(header->filesize));
    data_size_str[sizeof(header->filesize)] = '\0';

    size_t data_size = strtoull(data_size_str, NULL, 16);

    const char* name = ptr + sizeof(struct cpio_newc_header);
    const char* data = ptr + round_up(sizeof(struct cpio_newc_header) + name_size, 4);

    if (strcmp(name, CPIO_EOF_TAG) == 0) {
      return NULL;
    } else if (strcmp(name, file_name) == 0) {
      *file_size = data_size;
      return data;
    } else {
      ptr = data + round_up(data_size, 4);
    }
  }

  return NULL;
}
