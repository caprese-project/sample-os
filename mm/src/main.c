#include <crt/global.h>
#include <libcaprese/syscall.h>
#include <mm/internal.h>
#include <mm/internal_heap.h>
#include <stdlib.h>

int main(uintptr_t dtb_start, uintptr_t dtb_end) {
  uintptr_t user_space_end = unwrap_sysret(sys_system_user_space_end());

  for (cap_t cap = 1;; ++cap) {
    cap_type_t type = unwrap_sysret(sys_cap_type(cap));
    if (type == CAP_NULL) {
      break;
    } else if (type != CAP_PAGE_TABLE) {
      continue;
    }

    uintptr_t level = unwrap_sysret(sys_page_table_cap_level(cap));
    if (level == RISCV_MMU_SV39_MAX_PAGE) {
      root_page_table_cap = cap;
    } else if (level == MEGA_PAGE) {
      uintptr_t va_base = unwrap_sysret(sys_page_table_cap_virt_addr_base(cap));
      if (va_base >= user_space_end - GIGA_PAGE_SIZE) {
        continue;
      }
      inter_page_table_cap = cap;
    }
  }

  if (root_page_table_cap == 0 || inter_page_table_cap == 0) {
    abort();
  }

  init_first_internal_heap(dtb_start, dtb_end);

  mem_caps     = (mem_cap_t*)__heap_start;
  num_mem_caps = 0;

  for (cap_t cap = 1;; ++cap) {
    cap_type_t type = unwrap_sysret(sys_cap_type(cap));
    if (type == CAP_NULL) {
      break;
    } else if (type != CAP_MEM) {
      continue;
    }

    mem_caps[num_mem_caps++] = cap;
  }

  // Switch to the init task.
  sys_system_yield();

  while (true) { }

  return 0;
}
