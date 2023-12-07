#ifndef MM_MEMORY_MANAGER_H_
#define MM_MEMORY_MANAGER_H_

#include <cstddef>
#include <libcaprese/cap.h>

void      register_mem_cap(mem_cap_t mem_cap);
mem_cap_t fetch_mem_cap(size_t size, size_t alignment, int flags);
mem_cap_t retrieve_mem_cap(uintptr_t addr, size_t size, int flags);
void      revoke_mem_cap(mem_cap_t mem_cap);

#endif // MM_MEMORY_MANAGER_H_
