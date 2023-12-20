#include <cassert>
#include <crt/global.h>
#include <dm/dtb.h>
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

  size_t num_dtb_vp_caps = msg_buf.data[msg_buf.cap_part_length];
  // size_t num_dev_mem_caps = msg_buf.cap_part_length - num_dtb_vp_caps;

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

  sys_cap_destroy(msg_buf.data[0]);

  device_tree dt(reinterpret_cast<const char*>(dtb_addr), reinterpret_cast<const char*>(dtb_addr + KILO_PAGE_SIZE * num_dtb_vp_caps));

  const device_tree_node&         chosen_node            = dt.get_node("/chosen");
  const device_tree_property&     stdout_path_prop       = chosen_node.properties.at("stdout-path");
  const std::string&              stdout_path            = std::get<std::string>(stdout_path_prop.value);
  const device_tree_node&         stdout_node            = dt.get_node(stdout_path.substr(0, stdout_path.find('@')));
  const device_tree_property&     stdout_compatible_prop = stdout_node.properties.at("compatible");
  const std::vector<std::string>& stdout_compatibles     = std::get<std::vector<std::string>>(stdout_compatible_prop.value);

  for (const std::string& stdout_compatible : stdout_compatibles) {
    if (stdout_compatible == "ns16550a") {
      [[maybe_unused]] int i = 0;
    }
  }

  while (true) { }

  return 0;
}
