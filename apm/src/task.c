#include <apm/elf.h>
#include <apm/task.h>
#include <crt/global.h>
#include <libcaprese/syscall.h>
#include <service/mm.h>

task_cap_t create_task(const char* elf, size_t elf_size) {
  mem_cap_t mem_cap = mm_fetch(__mm_ep_cap, KILO_PAGE_SIZE * 5, KILO_PAGE_SIZE, MM_FETCH_FLAG_READ | MM_FETCH_FLAG_WRITE);
  if (mem_cap == 0) {
    return 0;
  }

  sysret_t cap_space_cap_result = sys_mem_cap_create_cap_space_object(mem_cap);
  if (cap_space_cap_result.error != 0) {
    sys_cap_revoke(mem_cap);
    return 0;
  }

  sysret_t root_page_table_cap_result = sys_mem_cap_create_page_table_object(mem_cap);
  if (root_page_table_cap_result.error != 0) {
    sys_cap_revoke(mem_cap);
    return 0;
  }

  sysret_t cap_space_page_table0_result = sys_mem_cap_create_page_table_object(mem_cap);
  if (cap_space_page_table0_result.error != 0) {
    sys_cap_revoke(mem_cap);
    return 0;
  }

  sysret_t cap_space_page_table1_result = sys_mem_cap_create_page_table_object(mem_cap);
  if (cap_space_page_table1_result.error != 0) {
    sys_cap_revoke(mem_cap);
    return 0;
  }

  sysret_t task_cap_result = sys_mem_cap_create_task_object(mem_cap,
                                                            cap_space_cap_result.result,
                                                            root_page_table_cap_result.result,
                                                            cap_space_page_table0_result.result,
                                                            cap_space_page_table1_result.result,
                                                            0);
  if (task_cap_result.error != 0) {
    sys_cap_revoke(mem_cap);
    return 0;
  }

  if (!elf_load(task_cap_result.result, root_page_table_cap_result.result, elf, elf_size)) {
    sys_cap_revoke(mem_cap);
    return 0;
  }

  sysret_t transfer_cap_result = sys_task_cap_transfer_cap(task_cap_result.result, root_page_table_cap_result.result);
  if (transfer_cap_result.error != 0) {
    sys_cap_revoke(mem_cap);
    return 0;
  }

  transfer_cap_result = sys_task_cap_transfer_cap(task_cap_result.result, cap_space_cap_result.result);
  if (transfer_cap_result.error != 0) {
    sys_cap_revoke(mem_cap);
    return 0;
  }

  transfer_cap_result = sys_task_cap_transfer_cap(task_cap_result.result, cap_space_page_table0_result.result);
  if (transfer_cap_result.error != 0) {
    sys_cap_revoke(mem_cap);
    return 0;
  }

  transfer_cap_result = sys_task_cap_transfer_cap(task_cap_result.result, cap_space_page_table1_result.result);
  if (transfer_cap_result.error != 0) {
    sys_cap_revoke(mem_cap);
    return 0;
  }

  return task_cap_result.result;
}
