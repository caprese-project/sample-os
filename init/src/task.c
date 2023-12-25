#include <crt/global.h>
#include <init/elf.h>
#include <init/task.h>
#include <libcaprese/syscall.h>
#include <mm/ipc.h>
#include <string.h>

bool create_task(task_context_t* ctx, mem_cap_fetcher_t fetch_mem_cap) {
  const size_t cap_space_size  = unwrap_sysret(sys_system_cap_size(CAP_CAP_SPACE));
  const size_t cap_space_align = unwrap_sysret(sys_system_cap_align(CAP_CAP_SPACE));

  const size_t page_table_size  = unwrap_sysret(sys_system_cap_size(CAP_PAGE_TABLE));
  const size_t page_table_align = unwrap_sysret(sys_system_cap_align(CAP_PAGE_TABLE));

  const size_t task_size  = unwrap_sysret(sys_system_cap_size(CAP_TASK));
  const size_t task_align = unwrap_sysret(sys_system_cap_align(CAP_TASK));

  const int max_page = get_max_page();
  if (max_page > TERA_PAGE) {
    return false;
  }

  mem_cap_t cap_space_mem_cap = fetch_mem_cap(cap_space_size, cap_space_align);
  if (cap_space_mem_cap == 0) {
    return false;
  }
  ctx->cap_space_cap = unwrap_sysret(sys_mem_cap_create_cap_space_object(cap_space_mem_cap));

  mem_cap_t root_page_table_mem_cap = fetch_mem_cap(page_table_size, page_table_align);
  if (root_page_table_mem_cap == 0) {
    return false;
  }
  ctx->page_table_caps[max_page][0] = unwrap_sysret(sys_mem_cap_create_page_table_object(root_page_table_mem_cap));

  mem_cap_t cap_space_page_table_mem_caps[TERA_PAGE];
  for (int level = 0; level < max_page; ++level) {
    cap_space_page_table_mem_caps[level] = fetch_mem_cap(page_table_size, page_table_align);
    if (cap_space_page_table_mem_caps[level] == 0) {
      return false;
    }
    ctx->cap_space_page_table_caps[level] = unwrap_sysret(sys_mem_cap_create_page_table_object(cap_space_page_table_mem_caps[level]));
  }
  for (int level = max_page; level < TERA_PAGE; ++level) {
    ctx->cap_space_page_table_caps[level] = 0;
  }

  mem_cap_t task_mem_cap = fetch_mem_cap(task_size, task_align);
  if (task_mem_cap == 0) {
    return false;
  }

  ctx->task_cap = unwrap_sysret(sys_mem_cap_create_task_object(task_mem_cap,
                                                               ctx->cap_space_cap,
                                                               ctx->page_table_caps[max_page][0],
                                                               ctx->cap_space_page_table_caps[0],
                                                               ctx->cap_space_page_table_caps[1],
                                                               ctx->cap_space_page_table_caps[2]));

  task_cap_t copy_task = unwrap_sysret(sys_task_cap_copy(ctx->task_cap));
  task_cap_t dst_task  = unwrap_sysret(sys_task_cap_transfer_cap(ctx->task_cap, copy_task));

  endpoint_cap_t dst_apm_ep = 0;
  if (__apm_ep_cap != 0) {
    endpoint_cap_t copy_apm_ep = unwrap_sysret(sys_endpoint_cap_copy(__apm_ep_cap));
    dst_apm_ep                 = unwrap_sysret(sys_task_cap_transfer_cap(ctx->task_cap, copy_apm_ep));
  }

  unwrap_sysret(sys_task_cap_set_reg(ctx->task_cap, REG_ARG_2, dst_task));   // __this_task_cap
  unwrap_sysret(sys_task_cap_set_reg(ctx->task_cap, REG_ARG_3, dst_apm_ep)); // __apm_task_cap

  return true;
}

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

bool load_elf(task_context_t* ctx, const void* elf_data, size_t elf_size, vmapper_t vmap) {
  static char buf[KILO_PAGE_SIZE];

  if (!is_valid_elf_format(elf_data, elf_size)) {
    return false;
  }

  const elf_header_t*         header          = (const elf_header_t*)elf_data;
  const elf_program_header_t* program_headers = (const elf_program_header_t*)((uintptr_t)elf_data + header->program_header_position);

  const uintptr_t va_base = header->entry_position / MEGA_PAGE_SIZE * MEGA_PAGE_SIZE;

  ctx->heap_root = 0;

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

    const char* section_data = (const char*)elf_data + program_headers[i].offset;

    int flags = 0;
    if (readable) {
      flags |= MM_VMAP_FLAG_READ;
    }
    if (writable) {
      flags |= MM_VMAP_FLAG_WRITE;
    }
    if (executable) {
      flags |= MM_VMAP_FLAG_EXEC;
    }

    for (uintptr_t va = va_start; va < va_end; va += KILO_PAGE_SIZE) {
      size_t offset = va - va_start;

      if (offset < (program_headers[i].file_size + va_offset)) {
        size_t interval_start = va == va_start ? va_offset : 0;
        size_t interval_end   = KILO_PAGE_SIZE;

        if (offset + interval_end > (program_headers[i].file_size + va_offset)) {
          interval_end = (program_headers[i].file_size + va_offset) - offset;
        }

        memset(buf, 0, interval_start);
        memcpy(buf + interval_start, section_data + offset, interval_end - interval_start);
        memset(buf + interval_end, 0, KILO_PAGE_SIZE - interval_end);
        vmap(ctx, flags, va, buf);
      } else {
        vmap(ctx, flags, va, NULL);
      }
    }

    if (ctx->heap_root < va_end) {
      ctx->heap_root = va_end;
    }
  }

  ctx->heap_root = (ctx->heap_root + MEGA_PAGE_SIZE - 1) / MEGA_PAGE_SIZE * MEGA_PAGE_SIZE;

  unwrap_sysret(sys_task_cap_set_reg(ctx->task_cap, REG_PROGRAM_COUNTER, header->entry_position));

  return true;
}

bool alloc_stack(task_context_t* ctx, vmapper_t vmap) {
  const uintptr_t stack_va = unwrap_sysret(sys_system_user_space_end()) - KILO_PAGE_SIZE;
  vmap(ctx, MM_VMAP_FLAG_READ | MM_VMAP_FLAG_WRITE, stack_va, NULL);
  unwrap_sysret(sys_task_cap_set_reg(ctx->task_cap, REG_STACK_POINTER, stack_va + KILO_PAGE_SIZE));
  return true;
}

bool delegate_all_caps(task_context_t* ctx) {
  for (int level = 0; level < TERA_PAGE; ++level) {
    for (int i = 0; i < 8; ++i) {
      if (ctx->page_table_caps[level][i] != 0) {
        unwrap_sysret(sys_task_cap_delegate_cap(ctx->task_cap, ctx->page_table_caps[level][i]));
      }
    }
  }

  for (int level = 0; level < TERA_PAGE; ++level) {
    if (ctx->cap_space_page_table_caps[level] != 0) {
      unwrap_sysret(sys_task_cap_delegate_cap(ctx->task_cap, ctx->cap_space_page_table_caps[level]));
    }
  }

  unwrap_sysret(sys_task_cap_delegate_cap(ctx->task_cap, ctx->cap_space_cap));

  return true;
}
