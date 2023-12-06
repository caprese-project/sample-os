#include <init/elf.h>
#include <init/mem.h>
#include <init/task.h>
#include <libcaprese/syscall.h>
#include <stdbool.h>

task_cap_t create_task(root_boot_info_t* root_boot_info, const char* elf, size_t elf_size, page_table_cap_t* root_page_table, uintptr_t* heap_root) {
  mem_cap_t mem_cap = fetch_mem_cap(root_boot_info, false, true, true, false, KILO_PAGE_SIZE * 5, KILO_PAGE_SIZE);

  cap_space_cap_t  cap_space_cap         = unwrap_sysret(sys_mem_cap_create_cap_space_object(mem_cap));
  page_table_cap_t root_page_table_cap   = unwrap_sysret(sys_mem_cap_create_page_table_object(mem_cap));
  page_table_cap_t cap_space_page_table0 = unwrap_sysret(sys_mem_cap_create_page_table_object(mem_cap));
  page_table_cap_t cap_space_page_table1 = unwrap_sysret(sys_mem_cap_create_page_table_object(mem_cap));

  task_cap_t task_cap = unwrap_sysret(sys_mem_cap_create_task_object(mem_cap, cap_space_cap, root_page_table_cap, cap_space_page_table0, cap_space_page_table1, 0));

  if (!elf_load(root_boot_info, task_cap, root_page_table_cap, elf, elf_size, heap_root)) {
    return 0;
  }

  if (root_page_table != NULL) {
    *root_page_table = root_page_table_cap;
  } else {
    unwrap_sysret(sys_task_cap_transfer_cap(task_cap, root_page_table_cap));
  }

  unwrap_sysret(sys_task_cap_transfer_cap(task_cap, cap_space_cap));
  unwrap_sysret(sys_task_cap_transfer_cap(task_cap, cap_space_page_table0));
  unwrap_sysret(sys_task_cap_transfer_cap(task_cap, cap_space_page_table1));

  return task_cap;
}
