#include <init/elf.h>
#include <init/mem.h>
#include <libcaprese/syscall.h>
#include <string.h>

extern char _init_end[];

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

static page_table_cap_t map_page_table(root_boot_info_t* root_boot_info, task_cap_t task, page_table_cap_t root_page_table_cap, size_t page_size, uintptr_t va) {
  page_table_cap_t page_table_cap = root_page_table_cap;

  for (int level = RISCV_MMU_SV39_MAX_PAGE; level > 0; --level) {
    mem_cap_t mem_cap = fetch_mem_cap(root_boot_info, false, true, true, false, page_size, page_size);
    if (mem_cap == 0) {
      return 0;
    }

    page_table_cap_t next_page_table_cap = unwrap_sysret(sys_mem_cap_create_page_table_object(mem_cap));

    unwrap_sysret(sys_page_table_cap_map_table(page_table_cap, get_page_table_index(va, level), next_page_table_cap));

    if (page_table_cap != root_page_table_cap) {
      unwrap_sysret(sys_task_cap_delegate_cap(task, page_table_cap));
    }
    page_table_cap = next_page_table_cap;
  }

  return page_table_cap;
}

bool elf_load(root_boot_info_t* root_boot_info, task_cap_t task, page_table_cap_t task_root_page_table_cap, const void* data, size_t size, uintptr_t* heap_root) {
  if (!is_valid_elf_format(data, size)) {
    return false;
  }

  const elf_header_t*         header          = (const elf_header_t*)data;
  const elf_program_header_t* program_headers = (const elf_program_header_t*)((uintptr_t)data + header->program_header_position);

  page_table_cap_t page_table_cap;

  const uintptr_t va_base = header->entry_position / MEGA_PAGE_SIZE * MEGA_PAGE_SIZE;

  page_table_cap = map_page_table(root_boot_info, task, task_root_page_table_cap, KILO_PAGE_SIZE, va_base);
  if (page_table_cap == 0) {
    return false;
  }

  uintptr_t heap_start = 0;

  for (size_t i = 0; i < header->num_program_header_entries; ++i) {
    if (program_headers[i].segment_type != ELF_PH_SEGMENT_LOAD) {
      continue;
    }

    if (va_base > program_headers[i].virtual_address) {
      return false;
    }

    if (program_headers[i].virtual_address + program_headers[i].memory_size - va_base > MEGA_PAGE_SIZE) {
      return false;
    }

    bool readable   = (program_headers[i].flags & ELF_PH_FLAG_READABLE) != 0;
    bool writable   = (program_headers[i].flags & ELF_PH_FLAG_WRITABLE) != 0;
    bool executable = (program_headers[i].flags & ELF_PH_FLAG_EXECUTABLE) != 0;

    uintptr_t va_start  = program_headers[i].virtual_address / KILO_PAGE_SIZE * KILO_PAGE_SIZE;
    uintptr_t va_end    = (program_headers[i].virtual_address + program_headers[i].memory_size + KILO_PAGE_SIZE - 1) / KILO_PAGE_SIZE * KILO_PAGE_SIZE;
    size_t    va_offset = program_headers[i].virtual_address % KILO_PAGE_SIZE;

    const char* section_data = (const char*)data + program_headers[i].offset;

    for (uintptr_t va = va_start; va < va_end; va += KILO_PAGE_SIZE) {
      mem_cap_t mem_cap = fetch_mem_cap(root_boot_info, false, readable, writable, executable, KILO_PAGE_SIZE, KILO_PAGE_SIZE);
      if (mem_cap == 0) {
        return false;
      }

      virt_page_cap_t virt_cap = unwrap_sysret(sys_mem_cap_create_virt_page_object(mem_cap, KILO_PAGE));

      size_t offset = va - va_start;

      if (offset < (program_headers[i].file_size + va_offset)) {
        size_t interval_start = va == va_start ? va_offset : 0;
        size_t interval_end   = KILO_PAGE_SIZE;

        if (offset + interval_end > (program_headers[i].file_size + va_offset)) {
          interval_end = (program_headers[i].file_size + va_offset) - offset;
        }

        unwrap_sysret(sys_page_table_cap_map_page(root_boot_info->page_table_caps[KILO_PAGE], get_page_table_index((uintptr_t)_init_end, KILO_PAGE), true, true, false, virt_cap));
        memcpy(_init_end + interval_start, section_data + offset, interval_end - interval_start);
        unwrap_sysret(sys_page_table_cap_remap_page(page_table_cap, get_page_table_index(va, KILO_PAGE), readable, writable, executable, virt_cap, root_boot_info->page_table_caps[KILO_PAGE]));
      } else {
        unwrap_sysret(sys_page_table_cap_map_page(page_table_cap, get_page_table_index(va, KILO_PAGE), readable, writable, executable, virt_cap));
      }

      unwrap_sysret(sys_task_cap_delegate_cap(task, virt_cap));
    }

    if (heap_start < va_end) {
      heap_start = va_end;
    }
  }

  unwrap_sysret(sys_task_cap_delegate_cap(task, page_table_cap));

  heap_start = (heap_start + MEGA_PAGE_SIZE - 1) / MEGA_PAGE_SIZE * MEGA_PAGE_SIZE;

  const uintptr_t stack_va = unwrap_sysret(sys_system_user_space_end()) - KILO_PAGE_SIZE;

  page_table_cap = map_page_table(root_boot_info, task, task_root_page_table_cap, KILO_PAGE_SIZE, stack_va);
  if (page_table_cap == 0) {
    return false;
  }

  mem_cap_t stack_mem_cap = fetch_mem_cap(root_boot_info, false, true, true, false, KILO_PAGE_SIZE, KILO_PAGE_SIZE);
  if (stack_mem_cap == 0) {
    return false;
  }

  virt_page_cap_t stack_virt_cap = unwrap_sysret(sys_mem_cap_create_virt_page_object(stack_mem_cap, KILO_PAGE));
  unwrap_sysret(sys_page_table_cap_map_page(page_table_cap, get_page_table_index(stack_va, KILO_PAGE), true, true, false, stack_virt_cap));

  unwrap_sysret(sys_task_cap_delegate_cap(task, stack_virt_cap));
  unwrap_sysret(sys_task_cap_delegate_cap(task, page_table_cap));

  unwrap_sysret(sys_task_cap_set_reg(task, REG_STACK_POINTER, stack_va + KILO_PAGE_SIZE));
  unwrap_sysret(sys_task_cap_set_reg(task, REG_PROGRAM_COUNTER, header->entry_position));

  task_cap_t copy_task = unwrap_sysret(sys_task_cap_copy(task));
  task_cap_t dst_task  = unwrap_sysret(sys_task_cap_transfer_cap(task, copy_task));

  unwrap_sysret(sys_task_cap_set_reg(task, REG_ARG_2, dst_task));
  unwrap_sysret(sys_task_cap_set_reg(task, REG_ARG_3, 0));

  if (heap_root) {
    *heap_root = heap_start;
  }

  return true;
}
