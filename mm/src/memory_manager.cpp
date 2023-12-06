#include <mm/ipc.h>
#include <mm/memory_manager.h>

namespace { }

mem_cap_t fetch_mem_cap(size_t size, size_t alignment, int flags) {
  (void)size;
  (void)alignment;
  (void)flags;
  return 0;
}

mem_cap_t retrieve_mem_cap(uintptr_t addr, size_t size) {
  (void)addr;
  (void)size;
  return 0;
}

void revoke_mem_cap(mem_cap_t mem_cap) {
  (void)mem_cap;
}
