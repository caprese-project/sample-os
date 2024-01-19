#ifndef DM_DEV_NS16550A_H_
#define DM_DEV_NS16550A_H_

#include <dm/dtb.h>

bool ns16550a_launcher(const device_tree_node& node, std::string_view executable_path);

#endif // DM_DEV_NS16550A_H_
