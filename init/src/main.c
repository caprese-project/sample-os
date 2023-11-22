#include <crt/global.h>
#include <init/elf.h>
#include <init/mem.h>
#include <libcaprese/root_boot_info.h>
#include <libcaprese/syscall.h>
#include <stdlib.h>

extern const char _apm_elf_start[];
extern const char _apm_elf_end[];
extern char       _init_end[];

static task_cap_t create_task(root_boot_info_t* root_boot_info, const char* elf, size_t elf_size) {
  size_t page_size = unwrap_sysret(sys_system_page_size());

  uintptr_t mem_cap = fetch_mem_cap(root_boot_info, false, true, true, false, page_size * 5, page_size);

  cap_space_cap_t  cap_space_cap         = unwrap_sysret(sys_mem_cap_create_cap_space_object(mem_cap));
  page_table_cap_t root_page_table_cap   = unwrap_sysret(sys_mem_cap_create_page_table_object(mem_cap));
  page_table_cap_t cap_space_page_table0 = unwrap_sysret(sys_mem_cap_create_page_table_object(mem_cap));
  page_table_cap_t cap_space_page_table1 = unwrap_sysret(sys_mem_cap_create_page_table_object(mem_cap));

  task_cap_t task_cap = unwrap_sysret(sys_mem_cap_create_task_object(mem_cap, cap_space_cap, root_page_table_cap, cap_space_page_table0, cap_space_page_table1, 0));

  size_t need_pages = elf_needed_pages(elf, elf_size);
  if (need_pages == 0) {
    return 0;
  }

  if (!elf_load(root_boot_info, task_cap, root_page_table_cap, elf)) {
    return 0;
  }

  unwrap_sysret(sys_task_cap_transfer_cap(task_cap, root_page_table_cap));
  unwrap_sysret(sys_task_cap_transfer_cap(task_cap, cap_space_cap));
  unwrap_sysret(sys_task_cap_transfer_cap(task_cap, cap_space_page_table0));
  unwrap_sysret(sys_task_cap_transfer_cap(task_cap, cap_space_page_table1));

  return task_cap;
}

int main(root_boot_info_t* root_boot_info) {
  __this_task_cap = root_boot_info->root_task_cap;

  task_cap_t apm_task_cap = create_task(root_boot_info, _apm_elf_start, _apm_elf_end - _apm_elf_start);
  if (apm_task_cap == 0) {
    abort();
  }

  for (size_t i = 0; i < root_boot_info->num_mem_caps; ++i) {
    unwrap_sysret(sys_task_cap_transfer_cap(apm_task_cap, root_boot_info->mem_caps[i]));
  }

  sys_task_cap_set_reg(apm_task_cap, REG_ARG_0, root_boot_info->arch_info.dtb_start);
  sys_task_cap_set_reg(apm_task_cap, REG_ARG_1, root_boot_info->arch_info.dtb_end);

  sys_task_cap_resume(apm_task_cap);

  while (true) {
    sys_task_cap_switch(apm_task_cap);
  }

  return 0;
}
