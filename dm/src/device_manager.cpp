#include <dm/device_manager.h>
#include <libcaprese/syscall.h>
#include <service/apm.h>
#include <service/mm.h>
#include <set>

namespace {
  using namespace std::literals::string_literals;

  device_tree dt;

  const std::map known_devices({
      std::pair<const std::string, std::pair<std::string, std::string>> {"riscv,plic0",     std::pair { "/init/plic"s, "plic"s }},
      std::pair<const std::string, std::pair<std::string, std::string>> {   "ns16550a", std::pair { "/init/ns16550a"s, "uart"s }},
      std::pair<const std::string, std::pair<std::string, std::string>> {"virtio,mmio", std::pair { "/init/virtio"s, "virtio"s }},
  });

  std::map<uintptr_t, mem_cap_t> dev_mem_caps;
} // namespace

bool load_dtb(const char* begin, const char* end) {
  dt.load(begin, end);
  return true;
}

const device_tree_node& lookup_node(std::string_view full_path) {
  std::string_view name = full_path.contains('@') ? full_path.substr(0, full_path.find('@')) : full_path;
  return dt.get_node(name);
}

bool launch_device(std::string_view full_path) {
  std::string_view name = full_path.contains('@') ? full_path.substr(0, full_path.find('@')) : full_path;

  if (!dt.has_node(name)) [[unlikely]] {
    return false;
  }

  const device_tree_node&                          node            = dt.get_node(name);
  const device_tree_property&                      compatible_prop = node.properties.at("compatible");
  const std::vector<std::string>&                  compatibles     = std::get<std::vector<std::string>>(compatible_prop.value);
  const device_tree_property&                      reg_prop        = node.properties.at("reg");
  const std::vector<std::pair<uintptr_t, size_t>>& regs            = std::get<std::vector<std::pair<uintptr_t, size_t>>>(reg_prop.value);

  std::vector<mem_cap_t> msg_payload;

  for (const auto& [addr, size] : regs) {
    size_t offset = 0;
    while (offset < size) {
      uintptr_t base = addr + offset;
      if (!dev_mem_caps.contains(base)) [[unlikely]] {
        return false;
      }
      mem_cap_t cap      = dev_mem_caps.at(base);
      size_t    cap_size = unwrap_sysret(sys_mem_cap_size(cap));
      msg_payload.push_back(cap);
      offset += cap_size;
    }
  }

  task_cap_t task_cap = 0;

  for (const std::string& compatible : compatibles) {
    if (known_devices.contains(compatible)) {
      auto& [path, name] = known_devices.at(compatible);
      task_cap           = apm_create(path.c_str(), name.c_str(), APM_CREATE_FLAG_DETACHED | APM_CREATE_FLAG_SUSPENDED, nullptr);
      break;
    }
  }

  if (!task_cap) [[unlikely]] {
    return false;
  }

  endpoint_cap_t ep_cap = mm_fetch_and_create_endpoint_object();
  if (ep_cap == 0) [[unlikely]] {
    return false;
  }

  endpoint_cap_t copied_ep_cap = unwrap_sysret(sys_endpoint_cap_copy(ep_cap));
  endpoint_cap_t dst_ep_cap    = unwrap_sysret(sys_task_cap_transfer_cap(task_cap, copied_ep_cap));
  unwrap_sysret(sys_task_cap_set_reg(task_cap, REG_ARG_0, dst_ep_cap));
  sys_task_cap_resume(task_cap);

  message_t* msg = new_ipc_message(sizeof(uintptr_t) * msg_payload.size());
  for (size_t i = 0; i < msg_payload.size(); i++) {
    set_ipc_cap(msg, i, msg_payload[i], false);
  }
  unwrap_sysret(sys_endpoint_cap_send_long(ep_cap, msg));
  delete_ipc_message(msg);

  return true;
}

void register_mem_cap(mem_cap_t dev_mem_cap) {
  assert(unwrap_sysret(sys_mem_cap_device(dev_mem_cap)));
  uintptr_t addr     = unwrap_sysret(sys_mem_cap_phys_addr(dev_mem_cap));
  dev_mem_caps[addr] = dev_mem_cap;
}
