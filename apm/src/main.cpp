#include <apm/task_manager.h>
#include <crt/global.h>
#include <crt/heap.h>
#include <libcaprese/cap.h>
#include <libcaprese/syscall.h>
#include <sstream>
#include <stdlib.h>

extern "C" {
  extern const char _plic_elf_start[];
  extern const char _plic_elf_end[];
}

int main() {
  __mm_ep_cap = __init_context.__arg_regs[0];
  __mm_id_cap = __init_context.__arg_regs[1];

  if (__heap_sbrk() == nullptr) [[unlikely]] {
    abort();
  }

  __brk_start = __brk_pos - MEGA_PAGE_SIZE;
  __heap_init();

  std::istringstream stream(std::string(_plic_elf_start, _plic_elf_end - _plic_elf_start), std::istringstream::binary);
  if (!create_task("plic", std::ref<std::istream>(stream))) {
    abort();
  }

  const task& plic = lookup_task("plic");
  plic.resume();
  plic.switch_task();

  while (true) {
    sys_system_yield();
  }

  return 0;
}
