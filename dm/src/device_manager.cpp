#include <dm/dev/ns16550a.h>
#include <dm/device_manager.h>
#include <libcaprese/syscall.h>
#include <service/apm.h>
#include <service/mm.h>

namespace {
  using namespace std::literals::string_literals;

  device_tree                                                                   dt;
  std::map<std::string, std::pair<device_launcher_t, std::string>, std::less<>> device_launchers;
  std::map<uintptr_t, mem_cap_t>                                                dev_mem_caps;
} // namespace

bool load_dtb(const char* begin, const char* end) {
  dt.load(begin, end);

  if (dt.has_node("/soc/uart")) {
    register_device("/soc/uart", ns16550a_launcher, "/init/ns16550a");
  } else if (dt.has_node("/soc/serial")) {
    register_device("/soc/serial", ns16550a_launcher, "/init/ns16550a");
  }

  return true;
}

const device_tree_node& lookup_node(std::string_view full_path) {
  std::string_view name = full_path.contains('@') ? full_path.substr(0, full_path.find('@')) : full_path;
  return dt.get_node(name);
}

bool register_device(std::string_view full_path, device_launcher_t launcher, std::string_view executable_path) {
  std::string_view name = full_path.contains('@') ? full_path.substr(0, full_path.find('@')) : full_path;

  if (!dt.has_node(name)) [[unlikely]] {
    return false;
  }

  auto result = device_launchers.emplace(full_path, std::pair<device_launcher_t, std::string>(launcher, executable_path));

  return result.second;
}

bool launch_device(std::string_view full_path) {
  std::string_view name = full_path.contains('@') ? full_path.substr(0, full_path.find('@')) : full_path;

  if (!device_launchers.contains(name)) [[unlikely]] {
    return false;
  }

  if (!dt.has_node(name)) [[unlikely]] {
    return false;
  }

  const device_tree_node& node            = dt.get_node(name);
  const auto& [launcher, executable_path] = device_launchers.find(name)->second;
  return launcher(node, executable_path);
}

void register_mem_cap(mem_cap_t dev_mem_cap) {
  assert(unwrap_sysret(sys_mem_cap_device(dev_mem_cap)));
  uintptr_t addr     = unwrap_sysret(sys_mem_cap_phys_addr(dev_mem_cap));
  dev_mem_caps[addr] = dev_mem_cap;
}

mem_cap_t find_mem_cap(uintptr_t addr) {
  if (dev_mem_caps.contains(addr)) {
    return dev_mem_caps[addr];
  } else {
    return 0;
  }
}
