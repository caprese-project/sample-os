#include <libcaprese/syscall.h>
#include <map>
#include <mm/ipc.h>
#include <mm/memory_manager.h>

namespace {
  struct mem_info_t {
    mem_cap_t cap;
    uintptr_t phys_addr;
    size_t    size;
  };

  std::map<uintptr_t, mem_info_t> dev_mem_caps;
  std::map<uintptr_t, mem_info_t> ram_mem_caps;
} // namespace

void register_mem_cap(mem_cap_t mem_cap) {
  assert(unwrap_sysret(sys_cap_type(mem_cap)) == CAP_MEM);

  bool      device    = unwrap_sysret(sys_mem_cap_device(mem_cap));
  uintptr_t phys_addr = unwrap_sysret(sys_mem_cap_phys_addr(mem_cap));
  size_t    size_bit  = unwrap_sysret(sys_mem_cap_size_bit(mem_cap));
  size_t    size      = (size_t)1 << size_bit;

  if (device) {
    assert(!dev_mem_caps.contains(phys_addr));
    dev_mem_caps[phys_addr] = { mem_cap, phys_addr, size };
  } else {
    assert(!ram_mem_caps.contains(phys_addr));
    ram_mem_caps[phys_addr] = { mem_cap, phys_addr, size };
  }
}

mem_cap_t fetch_mem_cap(size_t size, size_t alignment, int flags) {
  cap_t  mem_cap      = 0;
  size_t mem_cap_size = 0;

  for (auto& [phys_addr, mem_info] : ram_mem_caps) {
    assert(mem_info.phys_addr == unwrap_sysret(sys_mem_cap_phys_addr(mem_info.cap)));
    assert(mem_info.size == ((size_t)1 << unwrap_sysret(sys_mem_cap_size_bit(mem_info.cap))));

    if (mem_info.size < size) {
      continue;
    }

    bool readable   = unwrap_sysret(sys_mem_cap_readable(mem_info.cap));
    bool writable   = unwrap_sysret(sys_mem_cap_writable(mem_info.cap));
    bool executable = unwrap_sysret(sys_mem_cap_executable(mem_info.cap));

    if ((flags & MM_FETCH_FLAG_READ) && !readable) {
      continue;
    }

    if ((flags & MM_FETCH_FLAG_WRITE) && !writable) {
      continue;
    }

    if ((flags & MM_FETCH_FLAG_EXEC) && !executable) {
      continue;
    }

    uintptr_t end_addr  = phys_addr + mem_info.size;
    uintptr_t used_size = unwrap_sysret(sys_mem_cap_used_size(mem_info.cap));
    uintptr_t base_addr = (phys_addr + used_size + alignment - 1) / alignment * alignment;

    if (base_addr >= end_addr) {
      continue;
    }

    uintptr_t rem_size = mem_info.size - (base_addr - phys_addr);

    if (rem_size >= size) {
      if (mem_cap_size == 0 || mem_cap_size > mem_info.size) {
        mem_cap      = mem_info.cap;
        mem_cap_size = mem_info.size;
      }
    }
  }

  sysret_t sysret = sys_mem_cap_create_memory_object(mem_cap, flags & MM_FETCH_FLAG_READ, flags & MM_FETCH_FLAG_WRITE, flags & MM_FETCH_FLAG_EXEC, size, alignment);
  if (sysret_failed(sysret)) {
    return 0;
  }

  return sysret.result;
}

void revoke_mem_cap(mem_cap_t mem_cap) {
  (void)mem_cap;
}
