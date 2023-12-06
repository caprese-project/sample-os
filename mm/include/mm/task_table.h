#ifndef MM_TASK_TABLE_H_
#define MM_TASK_TABLE_H_

#include <libcaprese/syscall.h>

int attach_task(id_cap_t id, task_cap_t task, page_table_cap_t root_page_table, uintptr_t heap_root, uintptr_t heap_tail);
int detach_task(id_cap_t id);
int sbrk_task(id_cap_t id, intptr_t increment, uintptr_t* new_brk_pos);

#endif // MM_TASK_TABLE_H_
