#include <apm/elf.h>
#include <libcaprese/syscall.h>
#include <service/mm.h>

extern endpoint_cap_t mm_ep_cap;

static bool is_valid_elf_format(const void* data, size_t size) {
  if (sizeof(elf_header_t) > size) {
    return false;
  }

  const elf_header_t* header = (const elf_header_t*)data;

  if (header->magic[0] != ELF_MAGIC_0 || header->magic[1] != ELF_MAGIC_1 || header->magic[2] != ELF_MAGIC_2 || header->magic[3] != ELF_MAGIC_3) {
    return false;
  }

  if (header->xlen != ELF_XLEN_64) {
    return false;
  }

  if (header->endian != ELF_LITTLE_ENDIAN) {
    return false;
  }

  if (header->elf_header_version != ELF_CURRENT_VERSION) {
    return false;
  }

  if (header->machine != ELF_MACHINE_RISCV) {
    return false;
  }

  if (header->type != ELF_TYPE_EXECUTABLE) {
    return false;
  }

  const elf_program_header_t* program_headers = (const elf_program_header_t*)((uintptr_t)data + header->program_header_position);

  for (size_t i = 0; i < header->num_program_header_entries; ++i) {
    if (program_headers[i].segment_type != ELF_PH_SEGMENT_LOAD) {
      continue;
    }

    if (program_headers[i].file_size > program_headers[i].memory_size) {
      return false;
    }

    if (program_headers[i].offset + program_headers[i].file_size > size) {
      return false;
    }
  }

  return true;
}

bool elf_load(task_cap_t task, page_table_cap_t task_root_page_table_cap, const void* data, size_t size) {
  if (!is_valid_elf_format(data, size)) {
    return false;
  }

  (void)task;
  (void)task_root_page_table_cap;

  return false;
}
