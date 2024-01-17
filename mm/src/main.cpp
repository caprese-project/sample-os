#include <crt/global.h>
#include <crt/heap.h>
#include <cstdlib>
#include <iterator>
#include <libcaprese/syscall.h>
#include <mm/ipc.h>
#include <mm/memory_manager.h>
#include <mm/server.h>
#include <mm/task_table.h>

id_cap_t __this_id_cap;

extern "C" {
  void* __heap_sbrk() {
    if (vmap_task(__this_id_cap, MEGA_PAGE, MM_VMAP_FLAG_READ | MM_VMAP_FLAG_WRITE, 0, &__brk_pos) != MM_CODE_S_OK) {
      return nullptr;
    }

    return reinterpret_cast<void*>(__brk_pos);
  }
}

namespace {
  bool load_page_table_caps(page_table_cap_t (&dst)[TERA_PAGE], int max_page_level, uintptr_t user_space_end, message_t* msg) {
    for (size_t i = 0, len = get_message_size(msg); i < len; ++i) {
      if (!is_ipc_cap(msg, i)) {
        continue;
      }

      cap_t      cap  = get_ipc_cap(msg, i);
      cap_type_t type = static_cast<cap_type_t>(unwrap_sysret(sys_cap_type(cap)));
      if (type != CAP_PAGE_TABLE) {
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

  mem_cap_t early_fetch_mem_cap(size_t size, size_t alignment, message_t* msg) {
    cap_t  mem_cap      = 0;
    size_t mem_cap_size = 0;

    for (size_t i = 0, msg_size = get_message_size(msg); i < msg_size; ++i) {
      if (!is_ipc_cap(msg, i)) {
        continue;
      }

      cap_t      cap  = get_ipc_cap(msg, i);
      cap_type_t type = static_cast<cap_type_t>(unwrap_sysret(sys_cap_type(cap)));
      if (type != CAP_MEM) {
        continue;
      }

      bool mem_device = unwrap_sysret(sys_mem_cap_device(cap));
      if (mem_device) {
        continue;
      }

      uintptr_t phys_addr = unwrap_sysret(sys_mem_cap_phys_addr(cap));
      uintptr_t mem_size  = unwrap_sysret(sys_mem_cap_size(cap));
      uintptr_t end       = phys_addr + mem_size;
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

  bool map_first_internal_heap(const page_table_cap_t (&page_table_caps)[TERA_PAGE], message_t* msg) {
    mem_cap_t page_mem_cap = early_fetch_mem_cap(MEGA_PAGE_SIZE, MEGA_PAGE_SIZE, msg);
    if (page_mem_cap == 0) {
      return false;
    }

    virt_page_cap_t virt_page_cap = unwrap_sysret(sys_mem_cap_create_virt_page_object(page_mem_cap, true, true, false, MEGA_PAGE));
    if (sysret_failed(sys_page_table_cap_map_page(page_table_caps[MEGA_PAGE], get_page_table_index(__brk_pos, MEGA_PAGE), true, true, false, virt_page_cap))) {
      return false;
    }

    __brk_pos += MEGA_PAGE_SIZE;

    __heap_init();

    return true;
  }

  id_cap_t attach_init_task(task_cap_t init_task_cap, page_table_cap_t root_page_table_cap, page_table_cap_t (&page_table_caps)[TERA_PAGE - 1], int max_page_level) {
    id_cap_t init_id_cap = unwrap_sysret(sys_id_cap_create());
    int      result      = attach_task(init_id_cap, init_task_cap, root_page_table_cap, MM_STACK_DEFAULT, MM_TOTAL_DEFAULT, 0, true, nullptr, 0);
    if (result != MM_CODE_S_OK) [[unlikely]] {
      return 0;
    }

    task_info& info = get_task_info(init_id_cap);

    for (int level = 0; level < max_page_level; ++level) {
      page_table_cap_t& cap                                                              = page_table_caps[level];
      info.page_table_caps[level][unwrap_sysret(sys_page_table_cap_virt_addr_base(cap))] = cap;
    }

    return init_id_cap;
  }

  void attach_self(page_table_cap_t root_page_table_cap, int max_page_level, message_t* msg) {
    assert(max_page_level <= TERA_PAGE);

    __this_id_cap = unwrap_sysret(sys_id_cap_create());

    int result = attach_task(__this_id_cap, __this_task_cap, root_page_table_cap, MM_STACK_DEFAULT, MM_TOTAL_DEFAULT, KILO_PAGE_SIZE, true, nullptr, 0);

    if (result != MM_CODE_S_OK) {
      abort();
    }

    task_info& info = get_task_info(__this_id_cap);

    for (size_t i = 0, len = get_message_size(msg); i < len; ++i) {
      if (!is_ipc_cap(msg, i)) {
        continue;
      }

      cap_t      cap  = get_ipc_cap(msg, i);
      cap_type_t type = static_cast<cap_type_t>(unwrap_sysret(sys_cap_type(cap)));
      if (type == CAP_VIRT_PAGE) {
        uintptr_t va            = unwrap_sysret(sys_virt_page_cap_virt_addr(cap));
        info.virt_page_caps[va] = move_ipc_cap(msg, i);
      } else if (type == CAP_PAGE_TABLE) {
        int       level                 = unwrap_sysret(sys_page_table_cap_level(cap));
        uintptr_t va                    = unwrap_sysret(sys_page_table_cap_virt_addr_base(cap));
        info.page_table_caps[level][va] = move_ipc_cap(msg, i);
      }
    }

    result = grow_stack(__this_id_cap, 4 * KILO_PAGE_SIZE);

    if (result != MM_CODE_S_OK) {
      abort();
    }
  }

  endpoint_cap_t handshake(endpoint_cap_t init_task_ep_cap) {
    uintptr_t user_space_end = unwrap_sysret(sys_system_user_space_end());
    int       max_page_level = get_max_page();

    char       msg_buf[0x400];
    message_t* msg               = reinterpret_cast<message_t*>(msg_buf);
    msg->header.payload_length   = 0;
    msg->header.payload_capacity = sizeof(msg_buf) - sizeof(message_header);

    unwrap_sysret(sys_endpoint_cap_receive(init_task_ep_cap, msg));

    task_cap_t       init_task_cap                            = move_ipc_cap(msg, 0);
    page_table_cap_t init_task_root_page_table_cap            = move_ipc_cap(msg, 1);
    page_table_cap_t init_task_page_table_caps[TERA_PAGE - 1] = {};
    for (int i = 0; i < max_page_level; ++i) {
      init_task_page_table_caps[i] = move_ipc_cap(msg, 2 + i);
    }

    page_table_cap_t page_table_caps[TERA_PAGE] = {};
    if (!load_page_table_caps(page_table_caps, max_page_level, user_space_end, msg)) [[unlikely]] {
      return 1;
    }

    if (!map_first_internal_heap(page_table_caps, msg)) {
      return 0;
    }

    for (size_t i = 0, len = get_message_size(msg); i < len; ++i) {
      if (!is_ipc_cap(msg, i)) {
        continue;
      }

      cap_t      cap  = get_ipc_cap(msg, i);
      cap_type_t type = static_cast<cap_type_t>(unwrap_sysret(sys_cap_type(cap)));
      if (type != CAP_MEM) {
        continue;
      }

      register_mem_cap(move_ipc_cap(msg, i));
    }

    attach_self(page_table_caps[max_page_level], max_page_level, msg);

    id_cap_t init_id_cap = attach_init_task(init_task_cap, init_task_root_page_table_cap, init_task_page_table_caps, max_page_level);
    if (init_id_cap == 0) {
      return 0;
    }
    id_cap_t init_id_cap_copy = unwrap_sysret(sys_id_cap_copy(init_id_cap));

    const size_t ep_size    = unwrap_sysret(sys_system_cap_size(CAP_ENDPOINT));
    const size_t ep_align   = unwrap_sysret(sys_system_cap_align(CAP_ENDPOINT));
    mem_cap_t    ep_mem_cap = fetch_mem_cap(ep_size, ep_align);
    if (ep_mem_cap == 0) {
      return 0;
    }
    endpoint_cap_t ep_cap      = unwrap_sysret(sys_mem_cap_create_endpoint_object(ep_mem_cap));
    endpoint_cap_t ep_cap_copy = unwrap_sysret(sys_endpoint_cap_copy(ep_cap));

    msg->header.data_type_map[0] = 0;
    msg->header.data_type_map[1] = 0;

    destroy_ipc_message(msg);

    set_ipc_cap(msg, 0, ep_cap_copy, false);
    set_ipc_cap(msg, 1, init_id_cap_copy, false);

    unwrap_sysret(sys_endpoint_cap_reply(init_task_ep_cap, msg));

    return ep_cap;
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

  endpoint_cap_t ep_cap = handshake(init_task_ep_cap);

  if (ep_cap == 0) [[unlikely]] {
    return 1;
  }

  run(ep_cap);
}
