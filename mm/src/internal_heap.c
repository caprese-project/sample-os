#include <crt/global.h>
#include <libcaprese/syscall.h>
#include <mm/internal_heap.h>
#include <stdlib.h>

uintptr_t        internal_heap_root;
size_t           internal_heap_page_count;
page_table_cap_t root_page_table_cap;
page_table_cap_t inter_page_table_cap;
mem_cap_t*       mem_caps;
size_t           num_mem_caps;

static mem_cap_t fetch_mem_cap(uintptr_t dtb_start, uintptr_t dtb_end) {
  cap_t  mem_cap      = 0;
  size_t mem_cap_size = 0;

  for (cap_t cap = 1;; ++cap) {
    cap_type_t type = unwrap_sysret(sys_cap_type(cap));
    if (type == CAP_NULL) {
      break;
    } else if (type != CAP_MEM) {
      continue;
    }

    bool mem_device = unwrap_sysret(sys_mem_cap_device(cap));
    if (mem_device) {
      continue;
    }

    bool mem_readable = unwrap_sysret(sys_mem_cap_readable(cap));
    if (!mem_readable) {
      continue;
    }

    bool mem_writable = unwrap_sysret(sys_mem_cap_writable(cap));
    if (!mem_writable) {
      continue;
    }

    uintptr_t phys_addr = unwrap_sysret(sys_mem_cap_phys_addr(cap));
    uintptr_t size_bit  = unwrap_sysret(sys_mem_cap_size_bit(cap));
    uintptr_t mem_size  = 1 << size_bit;
    uintptr_t end       = phys_addr + mem_size;

    if (phys_addr <= dtb_start && dtb_start < end) {
      continue;
    }

    if (phys_addr <= dtb_end && dtb_end < end) {
      continue;
    }

    uintptr_t used_size = unwrap_sysret(sys_mem_cap_used_size(cap));
    uintptr_t base_addr = (phys_addr + used_size + KILO_PAGE_SIZE - 1) / KILO_PAGE_SIZE * KILO_PAGE_SIZE;
    uintptr_t rem_size  = mem_size - (base_addr - phys_addr);

    if (rem_size >= KILO_PAGE_SIZE) {
      if (mem_cap_size == 0 || mem_cap_size > mem_size) {
        mem_cap      = cap;
        mem_cap_size = mem_size;
      }
    }
  }

  return mem_cap;
}

void init_first_internal_heap(uintptr_t dtb_start, uintptr_t dtb_end, uintptr_t heap_root) {
  page_table_cap_t page_table_cap = inter_page_table_cap;

  for (int level = MEGA_PAGE; level > 0; --level) {
    mem_cap_t mem_cap = fetch_mem_cap(dtb_start, dtb_end);
    if (mem_cap == 0) {
      abort();
    }

    page_table_cap_t next_page_table_cap = unwrap_sysret(sys_mem_cap_create_page_table_object(mem_cap));
    unwrap_sysret(sys_page_table_cap_map_table(page_table_cap, RISCV_MMU_GET_PAGE_TABLE_INDEX(heap_root, level), next_page_table_cap));
    page_table_cap = next_page_table_cap;
  }

  mem_cap_t mem_cap = fetch_mem_cap(dtb_start, dtb_end);
  if (mem_cap == 0) {
    abort();
  }

  virt_page_cap_t page_cap = unwrap_sysret(sys_mem_cap_create_virt_page_object(mem_cap, KILO_PAGE));
  unwrap_sysret(sys_page_table_cap_map_page(page_table_cap, RISCV_MMU_GET_PAGE_TABLE_INDEX(heap_root, KILO_PAGE), true, true, false, page_cap));

  internal_heap_root       = heap_root;
  internal_heap_page_count = 1;
}

void extend_internal_heap(uintptr_t dtb_start, uintptr_t dtb_end) {
  (void)dtb_start;
  (void)dtb_end;
}
