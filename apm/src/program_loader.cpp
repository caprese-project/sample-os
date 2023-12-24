#include <apm/program_loader.h>
#include <apm/task_manager.h>
#include <crt/global.h>
#include <cstring>
#include <libcaprese/syscall.h>
#include <service/mm.h>
#include <utility>

program_loader::program_loader(std::reference_wrapper<task> target_ref, std::reference_wrapper<std::istream> stream_ref): target_ref(target_ref), stream_ref(stream_ref) { }

program_loader::~program_loader() { }

bool program_loader::map_page(uintptr_t va, int level, int flags, const void* data) {
  assert(va % get_page_size(level) == 0);

  if (data != nullptr) {
    uintptr_t addr = mm_vmap(__mm_id_cap, level, MM_VMAP_FLAG_READ | MM_VMAP_FLAG_WRITE, 0);
    if (addr == 0) [[unlikely]] {
      return false;
    }
    memcpy(reinterpret_cast<void*>(addr), data, get_page_size(level));
    addr = mm_vremap(__mm_id_cap, target_ref.get().get_mm_id_cap().get(), flags, addr, va);
    if (addr == 0) [[unlikely]] {
      return false;
    }
  } else {
    uintptr_t addr = mm_vmap(target_ref.get().get_mm_id_cap().get(), level, flags, va);
    if (addr == 0) [[unlikely]] {
      return false;
    }
  }

  return true;
}
