#include <cassert>
#include <crt/global.h>
#include <dm/device_manager.h>
#include <libcaprese/syscall.h>
#include <service/mm.h>

int main() {
  message_t* msg = new_ipc_message(0x1000);
  unwrap_sysret(sys_endpoint_cap_receive(__this_ep_cap, msg));

  size_t num_dtb_vp_caps  = get_ipc_data(msg, 0);
  size_t num_dev_mem_caps = get_ipc_data(msg, 1);

  id_cap_t  init_mm_id_cap = move_ipc_cap(msg, 2);
  uintptr_t dtb_addr       = mm_vpremap(init_mm_id_cap, __mm_id_cap, MM_VMAP_FLAG_READ, move_ipc_cap(msg, 3), 0);
  if (dtb_addr == 0) {
    return 1;
  }

  for (size_t i = 1; i < num_dtb_vp_caps; ++i) {
    uintptr_t addr = mm_vpremap(init_mm_id_cap, __mm_id_cap, MM_VMAP_FLAG_READ, move_ipc_cap(msg, 3 + i), dtb_addr + KILO_PAGE_SIZE * i);
    if (addr == 0) {
      return 1;
    }
  }

  for (size_t i = 0; i < num_dev_mem_caps; ++i) {
    register_mem_cap(move_ipc_cap(msg, 3 + num_dtb_vp_caps + i));
  }

  if (!load_dtb(reinterpret_cast<const char*>(dtb_addr), reinterpret_cast<const char*>(dtb_addr + KILO_PAGE_SIZE * num_dtb_vp_caps))) [[unlikely]] {
    return 1;
  }

  const device_tree_node&     chosen_node      = lookup_node("/chosen");
  const device_tree_property& stdout_path_prop = chosen_node.properties.at("stdout-path");
  const std::string&          stdout_path      = std::get<std::string>(stdout_path_prop.value);

  if (!launch_device(stdout_path)) [[unlikely]] {
    return 1;
  }

  destroy_ipc_message(msg);
  unwrap_sysret(sys_endpoint_cap_reply(__this_ep_cap, msg));
  delete_ipc_message(msg);

  while (true) {
    sys_system_yield();
  }

  return 0;
}
