#ifndef MM_MEMORY_MANAGER_H_
#define MM_MEMORY_MANAGER_H_

#include <cstddef>
#include <libcaprese/cap.h>

void      register_mem_cap(mem_cap_t mem_cap);
mem_cap_t fetch_mem_cap(size_t size, size_t alignment);
void      revoke_mem_cap(mem_cap_t mem_cap);

#endif // MM_MEMORY_MANAGER_H_
