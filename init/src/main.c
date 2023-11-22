#include <crt/global.h>
#include <init/task.h>
#include <libcaprese/root_boot_info.h>
#include <libcaprese/syscall.h>
#include <stdlib.h>

extern const char _apm_elf_start[];
extern const char _apm_elf_end[];
extern const char _mm_elf_start[];
extern const char _mm_elf_end[];
extern char       _init_end[];

int main(root_boot_info_t* root_boot_info) {
  __this_task_cap = root_boot_info->root_task_cap;

  task_cap_t mm_task_cap = create_task(root_boot_info, _mm_elf_start, _mm_elf_end - _mm_elf_start);
  if (mm_task_cap == 0) {
    abort();
  }

  task_cap_t apm_task_cap = create_task(root_boot_info, _apm_elf_start, _apm_elf_end - _apm_elf_start);
  if (apm_task_cap == 0) {
    abort();
  }

  for (size_t i = 0; i < root_boot_info->num_mem_caps; ++i) {
    unwrap_sysret(sys_task_cap_transfer_cap(mm_task_cap, root_boot_info->mem_caps[i]));
  }

  sys_task_cap_set_reg(mm_task_cap, REG_ARG_0, root_boot_info->arch_info.dtb_start);
  sys_task_cap_set_reg(mm_task_cap, REG_ARG_1, root_boot_info->arch_info.dtb_end);

  sys_task_cap_resume(mm_task_cap);
  sys_task_cap_switch(mm_task_cap);

  sys_task_cap_set_reg(apm_task_cap, REG_ARG_0, root_boot_info->arch_info.dtb_start);
  sys_task_cap_set_reg(apm_task_cap, REG_ARG_1, root_boot_info->arch_info.dtb_end);

  sys_task_cap_resume(apm_task_cap);
  sys_task_cap_switch(apm_task_cap);

  while (true) {
    sys_system_yield();
  }

  return 0;
}
