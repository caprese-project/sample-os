#include <cassert>
#include <crt/global.h>
#include <cstdlib>
#include <dm/device_manager.h>
#include <libcaprese/syscall.h>
#include <service/mm.h>

int main() {
  constexpr int cap_space_reserved = 5;
  for (int i = 0; i < cap_space_reserved; ++i) {
    cap_space_cap_t cap_space_cap = mm_fetch_and_create_cap_space_object();

    if (cap_space_cap == 0) [[unlikely]] {
      return 1;
    }

    unwrap_sysret(sys_task_cap_insert_cap_space(__this_task_cap, cap_space_cap));
  }

  message_t* msg = new_ipc_message(128 * sizeof(uintptr_t));
  unwrap_sysret(sys_endpoint_cap_receive(__this_ep_cap, msg));

  size_t num_dtb_vp_caps  = get_ipc_data(msg, 0);
  size_t num_dev_mem_caps = get_ipc_data(msg, 1);

  id_cap_t  init_mm_id_cap = move_ipc_cap(msg, 2);
  uintptr_t dtb_addr       = mm_vpremap(init_mm_id_cap, __mm_id_cap, MM_VMAP_FLAG_READ, move_ipc_cap(msg, 3), 0);
  if (dtb_addr == 0) {
    return 1;
  }

  size_t index = 4;

  const auto load_ipc_cap = [&]() {
    cap_t cap = move_ipc_cap(msg, index++);

    if (index == 128) [[unlikely]] {
      destroy_ipc_message(msg);
      unwrap_sysret(sys_endpoint_cap_receive(__this_ep_cap, msg));
      index = 0;
    }

    return cap;
  };

  for (size_t i = 1; i < num_dtb_vp_caps; ++i) {
    uintptr_t addr = mm_vpremap(init_mm_id_cap, __mm_id_cap, MM_VMAP_FLAG_READ, load_ipc_cap(), dtb_addr + KILO_PAGE_SIZE * i);
    if (addr == 0) {
      return 1;
    }
  }

  for (size_t i = 0; i < num_dev_mem_caps; ++i) {
    register_mem_cap(load_ipc_cap());
  }

  if (!load_dtb(reinterpret_cast<const char*>(dtb_addr), reinterpret_cast<const char*>(dtb_addr + KILO_PAGE_SIZE * num_dtb_vp_caps))) [[unlikely]] {
    return 1;
  }

  const device_tree_node&     chosen_node      = lookup_node("/chosen");
  const device_tree_property& stdout_path_prop = chosen_node.properties.at("stdout-path");
  const std::string&          stdout_path      = stdout_path_prop.to_str();

  if (!launch_device(stdout_path)) [[unlikely]] {
    return 1;
  }

  destroy_ipc_message(msg);
  unwrap_sysret(sys_endpoint_cap_receive(__this_ep_cap, msg));
  unwrap_sysret(sys_endpoint_cap_reply(__this_ep_cap, msg));
  delete_ipc_message(msg);

  while (true) {
    sys_system_yield();
  }

  return 0;
}
