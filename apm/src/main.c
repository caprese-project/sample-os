#include <apm/task.h>
#include <crt/global.h>
#include <libcaprese/cap.h>
#include <libcaprese/syscall.h>
#include <service/mm.h>
#include <stdlib.h>

extern const char _plic_elf_start[];
extern const char _plic_elf_end[];

endpoint_cap_t mm_ep_cap;

int main(endpoint_cap_t _mm_ep_cap) {
  mm_ep_cap = _mm_ep_cap;

  task_cap_t plic_task_cap = create_task(_plic_elf_start, _plic_elf_end - _plic_elf_start);
  if (plic_task_cap == 0) {
    abort();
  }

  mm_attach(_mm_ep_cap, plic_task_cap);

  sys_task_cap_resume(plic_task_cap);
  sys_task_cap_switch(plic_task_cap);

  while (true) {
    sys_system_yield();
  }

  return 0;
}
