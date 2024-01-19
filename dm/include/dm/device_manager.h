#ifndef DM_DEVICE_MANAGER_H_
#define DM_DEVICE_MANAGER_H_

#include <dm/dtb.h>
#include <libcaprese/cap.h>
#include <string_view>

using device_launcher_t = bool (*)(const device_tree_node&, std::string_view);

bool                    load_dtb(const char* begin, const char* end);
const device_tree_node& lookup_node(std::string_view full_path);
bool                    register_device(std::string_view full_path, device_launcher_t launcher, std::string_view executable_path);
bool                    launch_device(std::string_view full_path);
void                    register_mem_cap(mem_cap_t dev_mem_cap);
mem_cap_t               find_mem_cap(uintptr_t addr);

#endif // DM_DEVICE_MANAGER_H_
