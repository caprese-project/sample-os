#ifndef APM_TASK_H_
#define APM_TASK_H_

#include <libcaprese/cap.h>
#include <stddef.h>

task_cap_t create_task(const char* elf, size_t elf_size);

#endif // APM_TASK_H_
