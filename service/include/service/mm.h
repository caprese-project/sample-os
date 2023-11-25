#ifndef SERVICE_MM_H_
#define SERVICE_MM_H_

#include <libcaprese/cap.h>
#include <mm/ipc.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

  bool      mm_attach(endpoint_cap_t mm_ep_cap, task_cap_t task_cap, uintptr_t heap_root);
  bool      mm_detach(endpoint_cap_t mm_ep_cap, task_cap_t task_cap);
  mem_cap_t mm_fetch(endpoint_cap_t mm_ep_cap, size_t size, size_t alignment, int flags);
  mem_cap_t mm_retrieve(endpoint_cap_t mm_ep_cap, uintptr_t addr, size_t size);
  void      mm_revoke(endpoint_cap_t mm_ep_cap, mem_cap_t mem_cap);

#ifdef __cplusplus
} // extern "C"
#endif // __cplusplus

#endif // SERVICE_MM_H_
