#include <dm/dev/ns16550a.h>
#include <dm/device_manager.h>
#include <libcaprese/syscall.h>
#include <service/apm.h>
#include <service/mm.h>

namespace {
  constexpr uint32_t DEFAULT_UART_FREQ         = 0;
  constexpr uint32_t DEFAULT_UART_BAUD         = 115200;
  constexpr uint32_t DEFAULT_UART_REG_SHIFT    = 0;
  constexpr uint32_t DEFAULT_UART_REG_IO_WIDTH = 1;
  constexpr uint32_t DEFAULT_UART_REG_OFFSET   = 0;
} // namespace

bool ns16550a_launcher(const device_tree_node& node, std::string_view executable_path) {
  const device_tree_property&                      reg_prop = node.properties.at("reg");
  const std::vector<std::pair<uintptr_t, size_t>>& regs     = reg_prop.to_reg(node.address_cells, node.size_cells);

  mem_cap_t mem_cap = find_mem_cap(regs[0].first);
  if (mem_cap == 0) {
    return 0;
  }

  uint32_t freq   = DEFAULT_UART_FREQ;
  uint32_t baud   = DEFAULT_UART_BAUD;
  uint32_t shift  = DEFAULT_UART_REG_SHIFT;
  uint32_t width  = DEFAULT_UART_REG_IO_WIDTH;
  uint32_t offset = DEFAULT_UART_REG_OFFSET;

  if (node.properties.contains("clock-frequency")) {
    const device_tree_property& clock_frequency_prop = node.properties.at("clock-frequency");
    freq                                             = clock_frequency_prop.to_u32();
  }

  if (node.properties.contains("current-speed")) {
    const device_tree_property& current_speed_prop = node.properties.at("current-speed");
    baud                                           = current_speed_prop.to_u32();
  }

  if (node.properties.contains("reg-shift")) {
    const device_tree_property& reg_shift_prop = node.properties.at("reg-shift");
    shift                                      = reg_shift_prop.to_u32();
  }

  if (node.properties.contains("reg-io-width")) {
    const device_tree_property& reg_io_width_prop = node.properties.at("reg-io-width");
    width                                         = reg_io_width_prop.to_u32();
  }

  if (node.properties.contains("reg-offset")) {
    const device_tree_property& reg_offset_prop = node.properties.at("reg-offset");
    offset                                      = reg_offset_prop.to_u32();
  }

  endpoint_cap_t ep_cap = mm_fetch_and_create_endpoint_object();
  if (ep_cap == 0) [[unlikely]] {
    return false;
  }

  task_cap_t task_cap = apm_create(executable_path.data(), "uart", APM_CREATE_FLAG_DETACHED | APM_CREATE_FLAG_SUSPENDED, nullptr);
  if (task_cap == 0) [[unlikely]] {
    return false;
  }

  endpoint_cap_t copied_ep_cap = unwrap_sysret(sys_endpoint_cap_copy(ep_cap));
  endpoint_cap_t dst_ep_cap    = unwrap_sysret(sys_task_cap_transfer_cap(task_cap, copied_ep_cap));
  unwrap_sysret(sys_task_cap_set_reg(task_cap, REG_ARG_0, dst_ep_cap));
  sys_task_cap_resume(task_cap);

  message_t* msg = new_ipc_message(sizeof(uintptr_t) * 6);
  set_ipc_cap(msg, 0, mem_cap, true);
  set_ipc_data(msg, 1, freq);
  set_ipc_data(msg, 2, baud);
  set_ipc_data(msg, 3, shift);
  set_ipc_data(msg, 4, width);
  set_ipc_data(msg, 5, offset);
  unwrap_sysret(sys_endpoint_cap_send_long(ep_cap, msg));
  delete_ipc_message(msg);

  return true;
}
