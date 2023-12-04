#include <crt/global.h>
#include <crt/heap.h>
#include <iterator>
#include <libcaprese/syscall.h>
#include <mm/server.h>

extern "C" {
  void* __heap_sbrk() {
    return NULL;
  }
}

namespace {
  bool load_page_table_caps(page_table_cap_t (&dst)[4], uintptr_t user_space_end) {
    uintptr_t mmu_mode = unwrap_sysret(sys_arch_mmu_mode());
    int       max_page_level;

    switch (mmu_mode) {
      case RISCV_MMU_SV39:
        max_page_level = RISCV_MMU_SV39_MAX_PAGE;
        break;
      case RISCV_MMU_SV48:
        max_page_level = RISCV_MMU_SV48_MAX_PAGE;
        break;
      default:
        return false;
    }

    for (cap_t cap = 1;; ++cap) {
      cap_type_t type = static_cast<cap_type_t>(unwrap_sysret(sys_cap_type(cap)));
      if (type == CAP_NULL) {
        break;
      } else if (type != CAP_PAGE_TABLE) {
        continue;
      }

      uintptr_t level   = unwrap_sysret(sys_page_table_cap_level(cap));
      uintptr_t va_base = unwrap_sysret(sys_page_table_cap_virt_addr_base(cap));

      assert(level < std::size(dst));

      // Stack Pages
      if (va_base >= user_space_end - get_page_size(max_page_level)) {
        continue;
      }

      dst[level] = cap;
    }

    for (int level = 0; level < max_page_level; ++level) {
      if (dst[level] == 0) {
        return false;
      }
    }

    return true;
  }

  mem_cap_t fetch_mem_cap(uintptr_t dtb_start, uintptr_t dtb_end, size_t size, size_t alignment) {
    cap_t  mem_cap      = 0;
    size_t mem_cap_size = 0;

    for (cap_t cap = 1;; ++cap) {
      cap_type_t type = static_cast<cap_type_t>(unwrap_sysret(sys_cap_type(cap)));
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
      uintptr_t base_addr = (phys_addr + used_size + alignment - 1) / alignment * alignment;

      if (base_addr >= end) {
        continue;
      }

      uintptr_t rem_size = mem_size - (base_addr - phys_addr);

      if (rem_size >= size) {
        if (mem_cap_size == 0 || mem_cap_size > mem_size) {
          mem_cap      = cap;
          mem_cap_size = mem_size;
        }
      }
    }

    return mem_cap;
  }

  bool map_first_internal_heap(uintptr_t dtb_start, uintptr_t dtb_end, const page_table_cap_t (&page_table_caps)[4]) {
    mem_cap_t page_mem_cap = fetch_mem_cap(dtb_start, dtb_end, MEGA_PAGE_SIZE, MEGA_PAGE_SIZE);
    if (page_mem_cap == 0) {
      return false;
    }

    virt_page_cap_t virt_page_cap = unwrap_sysret(sys_mem_cap_create_virt_page_object(page_mem_cap, MEGA_PAGE));
    if (sysret_failed(sys_page_table_cap_map_page(page_table_caps[MEGA_PAGE], get_page_table_index(__brk_pos, MEGA_PAGE), true, true, false, virt_page_cap))) {
      return false;
    }

    __brk_pos += MEGA_PAGE_SIZE;

    __heap_init();

    return true;
  }
} // namespace

int main() {
  endpoint_cap_t init_task_ep_cap = __init_context.__arg_regs[0];
  uintptr_t      heap_root        = __init_context.__arg_regs[1];

  assert(unwrap_sysret(sys_cap_type(init_task_ep_cap)) == CAP_ENDPOINT);
  assert(heap_root % MEGA_PAGE_SIZE == 0);
  assert(heap_root % GIGA_PAGE_SIZE != 0);

  __brk_start = heap_root;
  __brk_pos   = heap_root;

  message_buffer_t msg_buf;
  unwrap_sysret(sys_endpoint_cap_receive(init_task_ep_cap, &msg_buf));

  uintptr_t dtb_start = msg_buf.data[msg_buf.cap_part_length + 0];
  uintptr_t dtb_end   = msg_buf.data[msg_buf.cap_part_length + 1];

  uintptr_t user_space_end = unwrap_sysret(sys_system_user_space_end());

  page_table_cap_t page_table_caps[4] = {};
  if (!load_page_table_caps(page_table_caps, user_space_end)) {
    return 1;
  }

  if (!map_first_internal_heap(dtb_start, dtb_end, page_table_caps)) {
    return 1;
  }

  mem_cap_t ep_mem_cap = fetch_mem_cap(dtb_start, dtb_end, unwrap_sysret(sys_system_cap_size(CAP_ENDPOINT)), unwrap_sysret(sys_system_cap_align(CAP_ENDPOINT)));
  if (ep_mem_cap == 0) {
    return 1;
  }
  endpoint_cap_t ep_cap      = unwrap_sysret(sys_mem_cap_create_endpoint_object(ep_mem_cap));
  endpoint_cap_t ep_cap_copy = unwrap_sysret(sys_endpoint_cap_copy(ep_cap));

  msg_buf.cap_part_length  = 1;
  msg_buf.data_part_length = 0;
  msg_buf.data[0]          = ep_cap_copy;
  unwrap_sysret(sys_endpoint_cap_send_long(init_task_ep_cap, &msg_buf));

  run(ep_cap);
}
