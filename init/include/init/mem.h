#ifndef INIT_MEM_H_
#define INIT_MEM_H_

#include <libcaprese/cap.h>
#include <libcaprese/root_boot_info.h>
#include <stdbool.h>

mem_cap_t fetch_mem_cap(root_boot_info_t* root_boot_info, bool device, bool readable, bool writable, bool executable, size_t size, size_t alignment);

#endif // INIT_MEM_H_
