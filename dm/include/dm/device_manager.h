#ifndef DM_DEVICE_MANAGER_H_
#define DM_DEVICE_MANAGER_H_

#include <dm/dtb.h>
#include <libcaprese/cap.h>

bool                    load_dtb(const char* begin, const char* end);
const device_tree_node& lookup_node(const std::string& full_path);
bool                    launch_device(const std::string& full_path);
void                    register_mem_cap(mem_cap_t dev_mem_cap);

#endif // DM_DEVICE_MANAGER_H_
