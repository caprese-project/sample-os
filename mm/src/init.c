#include <crt/global.h>
#include <libcaprese/syscall.h>
#include <mm/internal_heap.h>
#include <stdlib.h>

static mem_cap_t fetch_mem_cap(uintptr_t dtb_start, uintptr_t dtb_end, size_t size, size_t alignment) {
  size_t index        = 0;
  size_t mem_cap_size = 0;

  for (size_t i = 0; i < num_mem_caps; ++i) {
    bool mem_device = unwrap_sysret(sys_mem_cap_device(mem_caps[i]));
    if (mem_device) {
      continue;
    }

    bool mem_readable = unwrap_sysret(sys_mem_cap_readable(mem_caps[i]));
    bool mem_writable = unwrap_sysret(sys_mem_cap_writable(mem_caps[i]));

    if (!mem_readable || !mem_writable) {
      continue;
    }

    uintptr_t phys_addr = unwrap_sysret(sys_mem_cap_phys_addr(mem_caps[i]));
    uintptr_t size_bit  = unwrap_sysret(sys_mem_cap_size_bit(mem_caps[i]));
    uintptr_t mem_size  = 1 << size_bit;
    uintptr_t end       = phys_addr + mem_size;

    if (phys_addr <= dtb_start && dtb_start < end) {
      continue;
    }

    if (phys_addr <= dtb_end && dtb_end < end) {
      continue;
    }

    uintptr_t used_size = unwrap_sysret(sys_mem_cap_used_size(mem_caps[i]));

    uintptr_t base_addr = phys_addr + used_size;
    if (alignment > 0) {
      base_addr = (base_addr + alignment - 1) / alignment * alignment;
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

  return mem_caps[index];
}

endpoint_cap_t init(endpoint_cap_t init_task_ep_cap) {
  message_buffer_t msg_buf;
  sys_endpoint_cap_receive(init_task_ep_cap, &msg_buf);

  uintptr_t dtb_start = msg_buf.data[msg_buf.cap_part_length + 0];
  uintptr_t dtb_end   = msg_buf.data[msg_buf.cap_part_length + 1];

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
  num_mem_caps = msg_buf.cap_part_length;

  for (size_t i = 0; i < num_mem_caps; ++i) {
    mem_caps[i] = msg_buf.data[i];
  }

  mem_cap_t ep_mem_cap = fetch_mem_cap(dtb_start, dtb_end, unwrap_sysret(sys_system_cap_size(CAP_ENDPOINT)), unwrap_sysret(sys_system_cap_align(CAP_ENDPOINT)));
  if (ep_mem_cap == 0) {
    abort();
  }
  endpoint_cap_t ep_cap      = unwrap_sysret(sys_mem_cap_create_endpoint_object(ep_mem_cap));
  endpoint_cap_t ep_cap_copy = unwrap_sysret(sys_endpoint_cap_copy(ep_cap));

  msg_buf.cap_part_length  = 1;
  msg_buf.data_part_length = 0;
  msg_buf.data[0]          = ep_cap_copy;
  sys_endpoint_cap_send_long(init_task_ep_cap, &msg_buf);

  return ep_cap;
}
