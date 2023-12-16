#include <crt/global.h>
#include <init/task.h>
#include <libcaprese/root_boot_info.h>
#include <libcaprese/syscall.h>
#include <service/mm.h>
#include <stdlib.h>
#include <string.h>

extern const char _apm_elf_start[];
extern const char _apm_elf_end[];
extern const char _mm_elf_start[];
extern const char _mm_elf_end[];
extern char       _init_end[];

static id_cap_t apm_mm_id_cap;

static mem_cap_t extract_dtb_mem_cap(root_boot_info_t* root_boot_info) {
  for (size_t i = 0; i < root_boot_info->num_mem_caps; ++i) {
    uintptr_t phys_addr = unwrap_sysret(sys_mem_cap_phys_addr(root_boot_info->mem_caps[i]));
    uintptr_t mem_size  = unwrap_sysret(sys_mem_cap_size(root_boot_info->mem_caps[i]));
    uintptr_t end       = phys_addr + mem_size;

    if (phys_addr <= root_boot_info->arch_info.dtb_start && root_boot_info->arch_info.dtb_start < end) {
      return root_boot_info->mem_caps[i];
    }

    if (phys_addr <= root_boot_info->arch_info.dtb_end && root_boot_info->arch_info.dtb_end < end) {
      return root_boot_info->mem_caps[i];
    }
  }

  return 0;
}

static mem_cap_t early_fetch_mem_cap(size_t size, size_t alignment) {
  root_boot_info_t* root_boot_info = (root_boot_info_t*)__init_context.__arg_regs[0];

  size_t index        = 0;
  size_t mem_cap_size = 0;

  for (size_t i = 0; i < root_boot_info->num_mem_caps; ++i) {
    bool device = unwrap_sysret(sys_mem_cap_device(root_boot_info->mem_caps[i]));
    if (device) {
      continue;
    }

    uintptr_t phys_addr = unwrap_sysret(sys_mem_cap_phys_addr(root_boot_info->mem_caps[i]));
    uintptr_t mem_size  = unwrap_sysret(sys_mem_cap_size(root_boot_info->mem_caps[i]));
    uintptr_t end       = phys_addr + mem_size;

    if (phys_addr <= root_boot_info->arch_info.dtb_start && root_boot_info->arch_info.dtb_start < end) {
      continue;
    }

    if (phys_addr <= root_boot_info->arch_info.dtb_end && root_boot_info->arch_info.dtb_end < end) {
      continue;
    }

    uintptr_t used_size = unwrap_sysret(sys_mem_cap_used_size(root_boot_info->mem_caps[i]));

    uintptr_t base_addr = phys_addr + used_size;
    if (alignment > 0) {
      base_addr = (base_addr + alignment - 1) / alignment * alignment;
    }
    if (base_addr >= end) {
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

  if (mem_cap_size == 0) {
    return 0;
  }

  return root_boot_info->mem_caps[index];
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

    if (i == 8) {
      abort();
    }

    if (ctx->page_table_caps[level][i] == 0) {
      mem_cap_t mem_cap = early_fetch_mem_cap(page_table_size, page_table_align);
      if (mem_cap == 0) {
        abort();
      }

      page_table_cap_t next_page_table_cap = unwrap_sysret(sys_mem_cap_create_page_table_object(mem_cap));
      unwrap_sysret(sys_page_table_cap_map_table(page_table_cap, get_page_table_index(va_base, level + 1), next_page_table_cap));

      ctx->page_table_caps[level][i] = next_page_table_cap;
    }

    page_table_cap = ctx->page_table_caps[level][i];
  }

  mem_cap_t mem_cap = early_fetch_mem_cap(KILO_PAGE_SIZE, KILO_PAGE_SIZE);
  if (mem_cap == 0) {
    abort();
  }

  bool readable   = flags & MM_VMAP_FLAG_READ;
  bool writable   = flags & MM_VMAP_FLAG_WRITE;
  bool executable = flags & MM_VMAP_FLAG_EXEC;

  virt_page_cap_t virt_page_cap = unwrap_sysret(sys_mem_cap_create_virt_page_object(mem_cap, readable, writable, executable, KILO_PAGE));

  if (data == NULL) {
    unwrap_sysret(sys_page_table_cap_map_page(page_table_cap, get_page_table_index(va, KILO_PAGE), readable, writable, executable, virt_page_cap));
  } else {
    unwrap_sysret(sys_page_table_cap_map_page(root_boot_info->page_table_caps[KILO_PAGE], get_page_table_index((uintptr_t)_init_end, KILO_PAGE), true, true, false, virt_page_cap));
    memcpy(_init_end, data, KILO_PAGE_SIZE);
    unwrap_sysret(sys_page_table_cap_remap_page(page_table_cap, get_page_table_index(va, KILO_PAGE), readable, writable, executable, virt_page_cap, root_boot_info->page_table_caps[KILO_PAGE]));
  }
}

static void apm_vmap(task_context_t*, int flags, uintptr_t va, const void* data) {
  if (data == NULL) {
    if (mm_vmap(apm_mm_id_cap, KILO_PAGE, flags, va) == 0) {
      abort();
    }
  } else {
    uintptr_t src_va = mm_vmap(__mm_id_cap, KILO_PAGE, flags, (uintptr_t)_init_end);
    if (src_va == 0) {
      abort();
    }
    memcpy(_init_end, data, KILO_PAGE_SIZE);
    if (mm_vremap(__mm_id_cap, apm_mm_id_cap, flags, src_va, va) == 0) {
      abort();
    }
  }
}

static task_context_t mm_ctx;
static task_context_t apm_ctx;

int main() {
  root_boot_info_t* root_boot_info = (root_boot_info_t*)__init_context.__arg_regs[0];

  __this_task_cap = root_boot_info->root_task_cap;

  const int max_page = get_max_page();

  mem_cap_t dtb_mem_cap = extract_dtb_mem_cap(root_boot_info);

  if (!create_task(&mm_ctx, early_fetch_mem_cap)) {
    abort();
  }
  if (!load_elf(&mm_ctx, _mm_elf_start, _mm_elf_end - _mm_elf_start, early_vmap)) {
    abort();
  }
  if (!alloc_stack(&mm_ctx, early_vmap)) {
    abort();
  }
  if (!delegate_all_caps(&mm_ctx)) {
    abort();
  }

  if (!create_task(&apm_ctx, early_fetch_mem_cap)) {
    abort();
  }

  mem_cap_t ep_mem_cap = early_fetch_mem_cap(unwrap_sysret(sys_system_cap_size(CAP_ENDPOINT)), unwrap_sysret(sys_system_cap_align(CAP_ENDPOINT)));
  if (ep_mem_cap == 0) {
    abort();
  }

  endpoint_cap_t ep_cap      = unwrap_sysret(sys_mem_cap_create_endpoint_object(ep_mem_cap));
  endpoint_cap_t ep_cap_copy = unwrap_sysret(sys_endpoint_cap_copy(ep_cap));
  endpoint_cap_t ep_cap_dst  = unwrap_sysret(sys_task_cap_transfer_cap(mm_ctx.task_cap, ep_cap_copy));

  unwrap_sysret(sys_task_cap_set_reg(mm_ctx.task_cap, REG_ARG_0, ep_cap_dst));
  unwrap_sysret(sys_task_cap_set_reg(mm_ctx.task_cap, REG_ARG_1, mm_ctx.heap_root));

  unwrap_sysret(sys_task_cap_resume(mm_ctx.task_cap));
  unwrap_sysret(sys_task_cap_switch(mm_ctx.task_cap));

  message_buffer_t msg_buf;
  msg_buf.cap_part_length = 2 + max_page;
  msg_buf.data[0]         = msg_buf_transfer(unwrap_sysret(sys_task_cap_copy(root_boot_info->root_task_cap)));
  msg_buf.data[1]         = msg_buf_transfer(root_boot_info->root_page_table_cap);
  for (int i = 0; i < max_page; ++i) {
    msg_buf.data[2 + i] = msg_buf_transfer(root_boot_info->page_table_caps[i]);
  }
  msg_buf.cap_part_length += root_boot_info->num_mem_caps - 1;
  size_t dtb_cap_pos;
  for (dtb_cap_pos = 0; dtb_cap_pos < root_boot_info->num_mem_caps; ++dtb_cap_pos) {
    if (root_boot_info->mem_caps[dtb_cap_pos] == dtb_mem_cap) {
      break;
    }
    msg_buf.data[2 + max_page + dtb_cap_pos] = msg_buf_transfer(root_boot_info->mem_caps[dtb_cap_pos]);
  }
  for (size_t i = dtb_cap_pos + 1; i < root_boot_info->num_mem_caps; ++i) {
    msg_buf.data[2 + max_page + i - 1] = root_boot_info->mem_caps[i];
  }
  msg_buf.data_part_length = 0;
  unwrap_sysret(sys_endpoint_cap_call(ep_cap, &msg_buf));

  if (msg_buf.cap_part_length != 2) {
    abort();
  }

  __mm_ep_cap = msg_buf.data[0];
  __mm_id_cap = msg_buf.data[1];
  if (unwrap_sysret(sys_cap_type(__mm_ep_cap)) != CAP_ENDPOINT) {
    abort();
  }
  if (unwrap_sysret(sys_cap_type(__mm_id_cap)) != CAP_ID) {
    abort();
  }

  apm_mm_id_cap = mm_attach(apm_ctx.task_cap, apm_ctx.page_table_caps[max_page][0], 0, 0, 4 * KILO_PAGE_SIZE);
  if (apm_mm_id_cap == 0) {
    abort();
  }

  if (!load_elf(&apm_ctx, _apm_elf_start, _apm_elf_end - _apm_elf_start, apm_vmap)) {
    abort();
  }

  endpoint_cap_t mm_ep_cap_dst     = unwrap_sysret(sys_task_cap_transfer_cap(apm_ctx.task_cap, __mm_ep_cap));
  id_cap_t       apm_mm_id_cap_dst = unwrap_sysret(sys_task_cap_transfer_cap(apm_ctx.task_cap, apm_mm_id_cap));

  unwrap_sysret(sys_task_cap_set_reg(apm_ctx.task_cap, REG_ARG_0, mm_ep_cap_dst));
  unwrap_sysret(sys_task_cap_set_reg(apm_ctx.task_cap, REG_ARG_1, apm_mm_id_cap_dst));

  unwrap_sysret(sys_task_cap_resume(apm_ctx.task_cap));
  unwrap_sysret(sys_task_cap_switch(apm_ctx.task_cap));

  while (true) {
    unwrap_sysret(sys_system_yield());
  }

  return 0;
}
