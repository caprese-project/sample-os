#include <cassert>
#include <crt/global.h>
#include <dm/device_manager.h>
#include <libcaprese/syscall.h>
#include <service/mm.h>

int main() {
  endpoint_cap_t init_task_ep_cap = __init_context.__arg_regs[0];

  assert(unwrap_sysret(sys_cap_type(init_task_ep_cap)) == CAP_ENDPOINT);

  message_buffer_t msg_buf;
  unwrap_sysret(sys_endpoint_cap_receive(init_task_ep_cap, &msg_buf));

  if (msg_buf.data_part_length != 1) [[unlikely]] {
    return 1;
  }

  size_t num_dtb_vp_caps  = msg_buf.data[msg_buf.cap_part_length];
  size_t num_dev_mem_caps = msg_buf.cap_part_length - num_dtb_vp_caps - 1;

  uintptr_t dtb_addr = mm_vpremap(msg_buf.data[0], __mm_id_cap, MM_VMAP_FLAG_READ, msg_buf.data[1], 0);
  if (dtb_addr == 0) {
    return 1;
  }

  for (size_t i = 1; i < num_dtb_vp_caps; ++i) {
    uintptr_t addr = mm_vpremap(msg_buf.data[0], __mm_id_cap, MM_VMAP_FLAG_READ, msg_buf.data[1 + i], dtb_addr + KILO_PAGE_SIZE * i);
    if (addr == 0) {
      return 1;
    }
  }

  for (size_t i = 0; i < num_dev_mem_caps; ++i) {
    register_mem_cap(msg_buf.data[1 + num_dtb_vp_caps + i]);
  }

  sys_cap_destroy(msg_buf.data[0]);

  if (!load_dtb(reinterpret_cast<const char*>(dtb_addr), reinterpret_cast<const char*>(dtb_addr + KILO_PAGE_SIZE * num_dtb_vp_caps))) [[unlikely]] {
    return 1;
  }

  const device_tree_node&     chosen_node      = lookup_node("/chosen");
  const device_tree_property& stdout_path_prop = chosen_node.properties.at("stdout-path");
  const std::string&          stdout_path      = std::get<std::string>(stdout_path_prop.value);

  if (!launch_device(stdout_path)) [[unlikely]] {
    return 1;
  }

  while (true) {
    sys_system_yield();
  }

  return 0;
}
