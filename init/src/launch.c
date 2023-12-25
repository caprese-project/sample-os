#include <crt/global.h>
#include <init/launch.h>
#include <init/task.h>
#include <init/util.h>
#include <internal/branch.h>
#include <service/apm.h>
#include <service/mm.h>
#include <stdlib.h>
#include <string.h>

static mem_cap_t early_fetch_mem_cap(size_t size, size_t alignment) {
  root_boot_info_t* root_boot_info = (root_boot_info_t*)__init_context.__arg_regs[0];

  size_t index        = 0;
  size_t mem_cap_size = 0;

  for (size_t i = 0; i < root_boot_info->num_mem_caps; ++i) {
    bool device = unwrap_sysret(sys_mem_cap_device(root_boot_info->caps[root_boot_info->mem_caps_offset + i]));
    if (device) {
      continue;
    }

    uintptr_t phys_addr = unwrap_sysret(sys_mem_cap_phys_addr(root_boot_info->caps[root_boot_info->mem_caps_offset + i]));
    uintptr_t mem_size  = unwrap_sysret(sys_mem_cap_size(root_boot_info->caps[root_boot_info->mem_caps_offset + i]));
    uintptr_t end       = phys_addr + mem_size;

    __if_unlikely (phys_addr <= root_boot_info->arch_info.dtb_start && root_boot_info->arch_info.dtb_start < end) {
      continue;
    }

    __if_unlikely (phys_addr <= root_boot_info->arch_info.dtb_end && root_boot_info->arch_info.dtb_end < end) {
      continue;
    }

    uintptr_t used_size = unwrap_sysret(sys_mem_cap_used_size(root_boot_info->caps[root_boot_info->mem_caps_offset + i]));

    uintptr_t base_addr = phys_addr + used_size;
    if (alignment > 0) {
      base_addr = (base_addr + alignment - 1) / alignment * alignment;
    }
    __if_unlikely (base_addr >= end) {
      continue;
    }
    size_t rem_size = mem_size - (base_addr - phys_addr);

    if (rem_size >= size) {
      if (mem_cap_size == 0 || mem_cap_size > mem_size) {
        mem_cap_size = mem_size;
        index        = i;
      }
    }
  }

  __if_unlikely (mem_cap_size == 0) {
    return 0;
  }

  return root_boot_info->caps[root_boot_info->mem_caps_offset + index];
}

static void early_vmap(task_context_t* ctx, int flags, uintptr_t va, const void* data) {
  const size_t page_table_size  = unwrap_sysret(sys_system_cap_size(CAP_PAGE_TABLE));
  const size_t page_table_align = unwrap_sysret(sys_system_cap_align(CAP_PAGE_TABLE));

  root_boot_info_t* root_boot_info = (root_boot_info_t*)__init_context.__arg_regs[0];
  const int         max_page       = get_max_page();

  page_table_cap_t page_table_cap = ctx->page_table_caps[max_page][0];
  for (int level = max_page - 1; level >= KILO_PAGE; --level) {
    uintptr_t va_base = get_page_table_base_addr(va, level);

    int i = 0;
    for (; i < 8; ++i) {
      if (ctx->page_table_caps[level][i] == 0) {
        break;
      }
      uintptr_t mapped_va = unwrap_sysret(sys_page_table_cap_virt_addr_base(ctx->page_table_caps[level][i]));
      if (mapped_va == va_base) {
        break;
      }
    }

    __if_unlikely (i == 8) {
      abort();
    }

    if (ctx->page_table_caps[level][i] == 0) {
      mem_cap_t mem_cap = early_fetch_mem_cap(page_table_size, page_table_align);
      __if_unlikely (mem_cap == 0) {
        abort();
      }

      page_table_cap_t next_page_table_cap = unwrap_sysret(sys_mem_cap_create_page_table_object(mem_cap));
      unwrap_sysret(sys_page_table_cap_map_table(page_table_cap, get_page_table_index(va_base, level + 1), next_page_table_cap));

      ctx->page_table_caps[level][i] = next_page_table_cap;
    }

    page_table_cap = ctx->page_table_caps[level][i];
  }

  mem_cap_t mem_cap = early_fetch_mem_cap(KILO_PAGE_SIZE, KILO_PAGE_SIZE);
  __if_unlikely (mem_cap == 0) {
    abort();
  }

  bool readable   = flags & MM_VMAP_FLAG_READ;
  bool writable   = flags & MM_VMAP_FLAG_WRITE;
  bool executable = flags & MM_VMAP_FLAG_EXEC;

  virt_page_cap_t virt_page_cap = unwrap_sysret(sys_mem_cap_create_virt_page_object(mem_cap, readable, writable, executable, KILO_PAGE));

  if (data == NULL) {
    unwrap_sysret(sys_page_table_cap_map_page(page_table_cap, get_page_table_index(va, KILO_PAGE), readable, writable, executable, virt_page_cap));
  } else {
    unwrap_sysret(sys_page_table_cap_map_page(root_boot_info->page_table_caps[KILO_PAGE], get_page_table_index(root_boot_info->root_task_end_address, KILO_PAGE), true, true, false, virt_page_cap));
    memcpy((void*)root_boot_info->root_task_end_address, data, KILO_PAGE_SIZE);
    unwrap_sysret(sys_page_table_cap_remap_page(page_table_cap, get_page_table_index(va, KILO_PAGE), readable, writable, executable, virt_page_cap, root_boot_info->page_table_caps[KILO_PAGE]));
  }
}

static endpoint_cap_t early_create_ep_cap() {
  mem_cap_t ep_mem_cap = early_fetch_mem_cap(unwrap_sysret(sys_system_cap_size(CAP_ENDPOINT)), unwrap_sysret(sys_system_cap_align(CAP_ENDPOINT)));
  __if_unlikely (ep_mem_cap == 0) {
    abort();
  }
  return unwrap_sysret(sys_mem_cap_create_endpoint_object(ep_mem_cap));
}

static void vmap(task_context_t* ctx, int flags, uintptr_t va, const void* data) {
  if (data == NULL) {
    __if_unlikely (mm_vmap(ctx->mm_id_cap, KILO_PAGE, flags, va) == 0) {
      abort();
    }
  } else {
    root_boot_info_t* root_boot_info = (root_boot_info_t*)__init_context.__arg_regs[0];

    uintptr_t src_va = mm_vmap(__mm_id_cap, KILO_PAGE, flags | MM_VMAP_FLAG_WRITE, root_boot_info->root_task_end_address);
    __if_unlikely (src_va == 0) {
      abort();
    }
    memcpy((void*)src_va, data, KILO_PAGE_SIZE);
    __if_unlikely (mm_vremap(__mm_id_cap, ctx->mm_id_cap, flags, src_va, va) == 0) {
      abort();
    }
  }
}

static void create_and_setup_task(task_context_t* ctx, const char* name) {
  __if_unlikely (!create_task(ctx, mm_fetch)) {
    abort();
  }

  const int max_page = get_max_page();

  ctx->mm_id_cap = mm_attach(ctx->task_cap, ctx->page_table_caps[max_page][0], MM_STACK_DEFAULT, MM_TOTAL_DEFAULT, 4 * KILO_PAGE_SIZE);
  __if_unlikely (ctx->mm_id_cap == 0 || unwrap_sysret(sys_cap_type(ctx->mm_id_cap)) != CAP_ID) {
    abort();
  }

  size_t      elf_size;
  const char* elf_data = ramfs_find(name, &elf_size);

  __if_unlikely (elf_data == NULL) {
    abort();
  }

  __if_unlikely (!load_elf(ctx, elf_data, elf_size, vmap)) {
    abort();
  }

  id_cap_t mm_id_copy = unwrap_sysret(sys_id_cap_copy(ctx->mm_id_cap));

  unwrap_sysret(sys_task_cap_set_reg(ctx->task_cap, REG_ARG_4, copy_ep_cap_and_transfer(ctx->task_cap, __mm_ep_cap)));
  unwrap_sysret(sys_task_cap_set_reg(ctx->task_cap, REG_ARG_5, unwrap_sysret(sys_task_cap_transfer_cap(ctx->task_cap, mm_id_copy))));
}

static virt_page_cap_t find_virt_page_cap(uintptr_t va_base) {
  root_boot_info_t* root_boot_info = (root_boot_info_t*)__init_context.__arg_regs[0];

  for (size_t i = 0; i < root_boot_info->num_virt_page_caps; ++i) {
    virt_page_cap_t virt_page_cap = root_boot_info->caps[root_boot_info->virt_page_caps_offset + i];
    __if_unlikely (unwrap_sysret(sys_cap_type(virt_page_cap)) != CAP_VIRT_PAGE) {
      continue;
    }

    uintptr_t virt_addr = unwrap_sysret(sys_virt_page_cap_virt_addr(virt_page_cap));
    __if_unlikely (virt_addr == va_base) {
      return virt_page_cap;
    }
  }

  return 0;
}

void launch_mm(task_context_t* ctx) {
  root_boot_info_t* root_boot_info = (root_boot_info_t*)__init_context.__arg_regs[0];

  __if_unlikely (!create_task(ctx, early_fetch_mem_cap)) {
    abort();
  }

  size_t      mm_size;
  const char* mm_file = ramfs_find("mm", &mm_size);

  __if_unlikely (mm_file == NULL) {
    abort();
  }

  __if_unlikely (!load_elf(ctx, mm_file, mm_size, early_vmap)) {
    abort();
  }

  __if_unlikely (!alloc_stack(ctx, early_vmap)) {
    abort();
  }

  __if_unlikely (!delegate_all_caps(ctx)) {
    abort();
  }

  endpoint_cap_t init_mm_ep_cap = early_create_ep_cap();

  unwrap_sysret(sys_task_cap_set_reg(ctx->task_cap, REG_ARG_0, copy_ep_cap_and_transfer(ctx->task_cap, init_mm_ep_cap)));
  unwrap_sysret(sys_task_cap_set_reg(ctx->task_cap, REG_ARG_1, ctx->heap_root));

  unwrap_sysret(sys_task_cap_resume(ctx->task_cap));

  message_buffer_t msg_buf;
  msg_buf.cap_part_length  = 0;
  msg_buf.data_part_length = 0;

  push_cap(&msg_buf, unwrap_sysret(sys_task_cap_copy(root_boot_info->root_task_cap)));
  push_cap(&msg_buf, root_boot_info->root_page_table_cap);

  const int max_page = get_max_page();
  for (int i = 0; i < max_page; ++i) {
    push_cap(&msg_buf, root_boot_info->page_table_caps[i]);
  }

  for (size_t i = 0; i < root_boot_info->num_mem_caps; ++i) {
    bool device = unwrap_sysret(sys_mem_cap_device(root_boot_info->caps[root_boot_info->mem_caps_offset + i]));
    if (device) {
      continue;
    }

    push_cap(&msg_buf, root_boot_info->caps[root_boot_info->mem_caps_offset + i]);
  }

  unwrap_sysret(sys_endpoint_cap_call(init_mm_ep_cap, &msg_buf));

  __if_unlikely (msg_buf.cap_part_length != 2) {
    abort();
  }

  __mm_ep_cap = msg_buf.data[0];
  __mm_id_cap = msg_buf.data[1];

  __if_unlikely (unwrap_sysret(sys_cap_type(__mm_ep_cap)) != CAP_ENDPOINT) {
    abort();
  }
  __if_unlikely (unwrap_sysret(sys_cap_type(__mm_id_cap)) != CAP_ID) {
    abort();
  }
}

void launch_apm(task_context_t* ctx) {
  create_and_setup_task(ctx, "apm");

  endpoint_cap_t init_apm_ep_cap = mm_fetch_and_create_endpoint_object();

  unwrap_sysret(sys_task_cap_set_reg(ctx->task_cap, REG_ARG_0, copy_ep_cap_and_transfer(ctx->task_cap, init_apm_ep_cap)));

  unwrap_sysret(sys_task_cap_resume(ctx->task_cap));

  message_buffer_t msg_buf;
  unwrap_sysret(sys_endpoint_cap_receive(init_apm_ep_cap, &msg_buf));

  __if_unlikely (msg_buf.cap_part_length != 1) {
    abort();
  }

  __apm_ep_cap = msg_buf.data[0];
}

void launch_fs(task_context_t* ctx) {
  create_and_setup_task(ctx, "fs");

  endpoint_cap_t ep_cap        = mm_fetch_and_create_endpoint_object();
  endpoint_cap_t copied_ep_cap = unwrap_sysret(sys_endpoint_cap_copy(ep_cap));
  endpoint_cap_t dst_ep_cap    = unwrap_sysret(sys_task_cap_transfer_cap(ctx->task_cap, copied_ep_cap));

  __if_unlikely (!apm_attach(unwrap_sysret(sys_task_cap_copy(ctx->task_cap)), unwrap_sysret(sys_endpoint_cap_copy(ep_cap)), "fs")) {
    abort();
  }

  unwrap_sysret(sys_task_cap_set_reg(ctx->task_cap, REG_ARG_6, dst_ep_cap));
  unwrap_sysret(sys_task_cap_resume(ctx->task_cap));

  __fs_ep_cap = apm_lookup("fs");
}

void launch_ramfs(task_context_t* ctx) {
  create_and_setup_task(ctx, "ramfs");

  endpoint_cap_t ep_cap        = mm_fetch_and_create_endpoint_object();
  endpoint_cap_t copied_ep_cap = unwrap_sysret(sys_endpoint_cap_copy(ep_cap));
  endpoint_cap_t dst_ep_cap    = unwrap_sysret(sys_task_cap_transfer_cap(ctx->task_cap, copied_ep_cap));

  __if_unlikely (!apm_attach(unwrap_sysret(sys_task_cap_copy(ctx->task_cap)), unwrap_sysret(sys_endpoint_cap_copy(ep_cap)), "ramfs")) {
    abort();
  }

  unwrap_sysret(sys_task_cap_set_reg(ctx->task_cap, REG_ARG_6, dst_ep_cap));

  uintptr_t start = (uintptr_t)_ramfs_start;
  uintptr_t end   = (uintptr_t)_ramfs_end;

  uintptr_t va_base = 0;
  while (start < end) {
    virt_page_cap_t virt_page_cap = find_virt_page_cap(start);
    __if_unlikely (virt_page_cap == 0) {
      abort();
    }

    uintptr_t va = mm_vpremap(__mm_id_cap, ctx->mm_id_cap, MM_VMAP_FLAG_READ | MM_VMAP_FLAG_WRITE, virt_page_cap, 0);
    __if_unlikely (va == 0) {
      abort();
    }
    __if_unlikely (va_base == 0) {
      va_base = va;
    }
    start += KILO_PAGE_SIZE;
  }

  unwrap_sysret(sys_task_cap_set_reg(ctx->task_cap, REG_ARG_0, va_base));

  unwrap_sysret(sys_task_cap_resume(ctx->task_cap));
  unwrap_sysret(sys_task_cap_switch(ctx->task_cap));
}

void launch_dm(task_context_t* ctx) {
  ctx->task_cap = apm_create("/init/dm", "dm", APM_CREATE_FLAG_DEFAULT);
  __if_unlikely (ctx->task_cap == 0) {
    abort();
  }

  endpoint_cap_t ep_cap = apm_lookup("dm");
  __if_unlikely (ep_cap == 0) {
    abort();
  }

  message_buffer_t msg_buf;

  msg_buf.cap_part_length  = 0;
  msg_buf.data_part_length = 1;

  push_cap(&msg_buf, unwrap_sysret(sys_id_cap_copy(__mm_id_cap)));

  root_boot_info_t* root_boot_info = (root_boot_info_t*)__init_context.__arg_regs[0];

  for (size_t i = 0; i < root_boot_info->arch_info.num_dtb_vp_caps; ++i) {
    push_cap(&msg_buf, root_boot_info->caps[root_boot_info->arch_info.dtb_vp_caps_offset + i]);
  }

  for (size_t i = 0; i < root_boot_info->num_mem_caps; ++i) {
    // non-device memory caps are already sent to mm. thus, type of the cap is CAP_NULL.
    __if_unlikely (unwrap_sysret(sys_cap_type(root_boot_info->caps[root_boot_info->mem_caps_offset + i])) != CAP_MEM) {
      continue;
    }

    bool device = unwrap_sysret(sys_mem_cap_device(root_boot_info->caps[root_boot_info->mem_caps_offset + i]));
    __if_unlikely (!device) {
      continue;
    }

    push_cap(&msg_buf, root_boot_info->caps[root_boot_info->mem_caps_offset + i]);
  }

  msg_buf.data[msg_buf.cap_part_length] = root_boot_info->arch_info.num_dtb_vp_caps;

  unwrap_sysret(sys_endpoint_cap_call(ep_cap, &msg_buf));

  sys_cap_destroy(ep_cap);
}

void launch_shell(task_context_t* ctx) {
  ctx->task_cap = apm_create("/init/shell", "shell", APM_CREATE_FLAG_DEFAULT);
  __if_unlikely (ctx->task_cap == 0) {
    abort();
  }
}
