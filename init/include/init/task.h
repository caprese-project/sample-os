#ifndef INIT_TASK_H_
#define INIT_TASK_H_

#include <libcaprese/root_boot_info.h>

task_cap_t create_task(root_boot_info_t* root_boot_info, const char* elf, size_t elf_size);

#endif // INIT_TASK_H_
