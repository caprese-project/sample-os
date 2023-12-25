#ifndef INIT_TASK_H_
#define INIT_TASK_H_

#include <libcaprese/cap.h>
#include <libcaprese/root_boot_info.h>
#include <libcaprese/syscall.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
  task_cap_t       task_cap;
  cap_space_cap_t  cap_space_cap;
  page_table_cap_t page_table_caps[TERA_PAGE][8];
  page_table_cap_t cap_space_page_table_caps[TERA_PAGE];
  uintptr_t        heap_root;
  id_cap_t         mm_id_cap;
} task_context_t;

typedef mem_cap_t (*mem_cap_fetcher_t)(size_t size, size_t alignment);
typedef void (*vmapper_t)(task_context_t* ctx, int flags, uintptr_t va, const void* data);

bool create_task(task_context_t* ctx, mem_cap_fetcher_t fetch_mem_cap);
bool load_elf(task_context_t* ctx, const void* elf_data, size_t elf_size, vmapper_t vmap);
bool alloc_stack(task_context_t* ctx, vmapper_t vmap);
bool delegate_all_caps(task_context_t* ctx);

#endif // INIT_TASK_H_
