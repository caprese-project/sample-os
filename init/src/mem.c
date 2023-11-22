#include <init/mem.h>
#include <libcaprese/syscall.h>

mem_cap_t fetch_mem_cap(root_boot_info_t* root_boot_info, bool device, bool readable, bool writable, bool executable, size_t size, size_t alignment) {
  size_t index        = 0;
  size_t mem_cap_size = 0;

  for (size_t i = 0; i < root_boot_info->num_mem_caps; ++i) {
    bool mem_device = unwrap_sysret(sys_mem_cap_device(root_boot_info->mem_caps[i]));
    if (device != mem_device) {
      continue;
    }

    if (readable) {
      bool mem_readable = unwrap_sysret(sys_mem_cap_readable(root_boot_info->mem_caps[i]));
      if (!mem_readable) {
        continue;
      }
    }

    if (writable) {
      bool mem_writable = unwrap_sysret(sys_mem_cap_writable(root_boot_info->mem_caps[i]));
      if (!mem_writable) {
        continue;
      }
    }

    if (executable) {
      bool mem_executable = unwrap_sysret(sys_mem_cap_executable(root_boot_info->mem_caps[i]));
      if (!mem_executable) {
        continue;
      }
    }

    uintptr_t phys_addr = unwrap_sysret(sys_mem_cap_phys_addr(root_boot_info->mem_caps[i]));
    uintptr_t size_bit  = unwrap_sysret(sys_mem_cap_size_bit(root_boot_info->mem_caps[i]));
    uintptr_t mem_size  = 1 << size_bit;
    uintptr_t end = phys_addr + mem_size;

    if (phys_addr <= root_boot_info->arch_info.dtb_start && root_boot_info->arch_info.dtb_start < end) {
      continue;
    }

    if (phys_addr <= root_boot_info->arch_info.dtb_end && root_boot_info->arch_info.dtb_end < end) {
      continue;
    }

    uintptr_t used_size = unwrap_sysret(sys_mem_cap_used_size(root_boot_info->mem_caps[i]));

    uintptr_t base_addr = phys_addr + used_size;
    if (alignment > 0) {
      base_addr = (base_addr + alignment - 1) / alignment * alignment;
    }
    size_t rem_size = mem_size - (base_addr - phys_addr);

    if (rem_size >= size) {
      if (mem_cap_size == 0 || mem_cap_size > mem_size) {
        mem_cap_size = mem_size;
        index        = i;
      }
    }
  }

  if (mem_cap_size == 0) {
    return 0;
  }

  return root_boot_info->mem_caps[index];
}
