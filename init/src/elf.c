#include <init/elf.h>
#include <init/mem.h>
#include <libcaprese/syscall.h>
#include <string.h>

extern char _init_end[];

size_t elf_needed_pages(const void* data, size_t size) {
  if (sizeof(elf_header_t) > size) {
    return 0;
  }

  const elf_header_t* header = (const elf_header_t*)data;

  if (header->magic[0] != ELF_MAGIC_0 || header->magic[1] != ELF_MAGIC_1 || header->magic[2] != ELF_MAGIC_2 || header->magic[3] != ELF_MAGIC_3) {
    return 0;
  }

  if (header->xlen != ELF_XLEN_64) {
    return 0;
  }

  if (header->endian != ELF_LITTLE_ENDIAN) {
    return 0;
  }

  if (header->elf_header_version != ELF_CURRENT_VERSION) {
    return 0;
  }

  if (header->machine != ELF_MACHINE_RISCV) {
    return 0;
  }

  if (header->type != ELF_TYPE_EXECUTABLE) {
    return 0;
  }

  const elf_program_header_t* program_headers = (const elf_program_header_t*)((uintptr_t)data + header->program_header_position);
  const size_t                page_size       = unwrap_sysret(sys_system_page_size());

  size_t result = 0;

  for (size_t i = 0; i < header->num_program_header_entries; ++i) {
    if (program_headers[i].segment_type != ELF_PH_SEGMENT_LOAD) {
      continue;
    }

    if (program_headers[i].file_size > program_headers[i].memory_size) {
      return 0;
    }

    if (program_headers[i].offset + program_headers[i].file_size > size) {
      return 0;
    }

    result += (program_headers[i].memory_size + program_headers[i].alignment - 1) / program_headers[i].alignment * program_headers[i].alignment;
    result = (result + page_size - 1) / page_size * page_size;
  }

  return result / page_size;
}

bool elf_load(root_boot_info_t* root_boot_info, task_cap_t task, page_table_cap_t task_root_page_table_cap, const void* data) {
  const elf_header_t*         header          = (const elf_header_t*)data;
  const elf_program_header_t* program_headers = (const elf_program_header_t*)((uintptr_t)data + header->program_header_position);
  const size_t                page_size       = unwrap_sysret(sys_system_page_size());

  const uintptr_t va_base = header->entry_position / (512 * 0x1000) * (512 * 0x1000);

  page_table_cap_t page_table_cap = task_root_page_table_cap;
  for (int level = 1; level >= 0; --level) {
    mem_cap_t        mem_cap             = fetch_mem_cap(root_boot_info, false, true, true, false, page_size, page_size);
    page_table_cap_t next_page_table_cap = unwrap_sysret(sys_mem_cap_create_page_table_object(mem_cap));

    unwrap_sysret(sys_page_table_cap_map_table(page_table_cap, RISCV_MMU_GET_PAGE_TABLE_INDEX(va_base, level), next_page_table_cap));
    page_table_cap = next_page_table_cap;
  }

  for (size_t i = 0; i < header->num_program_header_entries; ++i) {
    if (program_headers[i].segment_type != ELF_PH_SEGMENT_LOAD) {
      continue;
    }

    if (program_headers[i].file_size > program_headers[i].memory_size) {
      return false;
    }

    bool      readable     = (program_headers[i].flags & ELF_PH_FLAG_READABLE) != 0;
    bool      writable     = (program_headers[i].flags & ELF_PH_FLAG_WRITABLE) != 0;
    bool      executable   = (program_headers[i].flags & ELF_PH_FLAG_EXECUTABLE) != 0;
    size_t    va_offset    = program_headers[i].virtual_address % page_size;
    size_t    mem_cap_size = (page_size - va_offset) % page_size + program_headers[i].memory_size;
    mem_cap_t mem_cap      = fetch_mem_cap(root_boot_info, false, readable, writable, executable, mem_cap_size, page_size);

    if (mem_cap == 0) {
      return false;
    }

    const char* section_data = (const char*)data + program_headers[i].offset;

    for (size_t offset = 0; offset < program_headers[i].file_size; offset += page_size) {
      virt_page_cap_t virt_cap = unwrap_sysret(sys_mem_cap_create_virt_page_object(mem_cap, KILO_PAGE));
      unwrap_sysret(sys_page_table_cap_map_page(root_boot_info->page_table_caps[KILO_PAGE], RISCV_MMU_GET_PAGE_TABLE_INDEX((uintptr_t)_init_end, KILO_PAGE), true, true, false, virt_cap));
      memcpy(_init_end, section_data + offset, page_size);
      unwrap_sysret(sys_page_table_cap_remap_page(page_table_cap,
                                                  RISCV_MMU_GET_PAGE_TABLE_INDEX(program_headers[i].virtual_address + offset, KILO_PAGE),
                                                  readable,
                                                  writable,
                                                  executable,
                                                  virt_cap,
                                                  root_boot_info->page_table_caps[KILO_PAGE]));
    }

    size_t file_offset = (program_headers[i].file_size + page_size - 1) / page_size * page_size;
    for (size_t offset = file_offset; offset < program_headers[i].memory_size; offset += page_size) {
      virt_page_cap_t virt_cap = unwrap_sysret(sys_mem_cap_create_virt_page_object(mem_cap, KILO_PAGE));
      unwrap_sysret(sys_page_table_cap_map_page(page_table_cap, RISCV_MMU_GET_PAGE_TABLE_INDEX(program_headers[i].virtual_address + offset, KILO_PAGE), readable, writable, executable, virt_cap));
    }
  }

  const uintptr_t stack_va = va_base + 512 * 0x1000 - page_size;

  mem_cap_t stack_mem_cap = fetch_mem_cap(root_boot_info, false, true, true, false, page_size, page_size);
  if (stack_mem_cap == 0) {
    return false;
  }

  virt_page_cap_t stack_virt_cap = unwrap_sysret(sys_mem_cap_create_virt_page_object(stack_mem_cap, KILO_PAGE));
  unwrap_sysret(sys_page_table_cap_map_page(page_table_cap, RISCV_MMU_GET_PAGE_TABLE_INDEX(stack_va, KILO_PAGE), true, true, false, stack_virt_cap));

  unwrap_sysret(sys_task_cap_set_reg(task, REG_STACK_POINTER, stack_va + page_size));
  unwrap_sysret(sys_task_cap_set_reg(task, REG_PROGRAM_COUNTER, header->entry_position));

  task_cap_t copy_task = unwrap_sysret(sys_task_cap_copy(task));
  task_cap_t dst_task = unwrap_sysret(sys_task_cap_transfer_cap(task, copy_task));

  unwrap_sysret(sys_task_cap_set_reg(task, ARCH_REG_S0, dst_task));

  return true;
}
