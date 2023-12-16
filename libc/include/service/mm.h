#ifndef LIBC_SERVICE_MM_H_
#define LIBC_SERVICE_MM_H_

#include <libcaprese/cap.h>
#include <mm/ipc.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

  id_cap_t  mm_attach(task_cap_t task_cap, page_table_cap_t root_page_table_cap, size_t stack_available, size_t total_available, size_t stack_commit);
  bool      mm_detach(id_cap_t id_cap);
  uintptr_t mm_vmap(id_cap_t id_cap, int level, int flags, uintptr_t va_base);
  uintptr_t mm_vremap(id_cap_t src_id_cap, id_cap_t dst_id_cap, int flags, uintptr_t src_va_base, uintptr_t dst_va_base);

  mem_cap_t mm_fetch(size_t size, size_t alignment);
  bool      mm_revoke(mem_cap_t mem_cap);

  task_cap_t mm_fetch_and_create_task_object(
      cap_space_cap_t cap_space_cap, page_table_cap_t root_page_table_cap, page_table_cap_t cap_space_page_table0, page_table_cap_t cap_space_page_table1, page_table_cap_t cap_space_page_table2);
  endpoint_cap_t   mm_fetch_and_create_endpoint_object();
  page_table_cap_t mm_fetch_and_create_page_table_object();
  virt_page_cap_t  mm_fetch_and_create_virt_page_object(bool readable, bool writable, bool executable, uintptr_t level);
  cap_space_cap_t  mm_fetch_and_create_cap_space_object();

#ifdef __cplusplus
} // extern "C"
#endif // __cplusplus

#endif // LIBC_SERVICE_MM_H_
