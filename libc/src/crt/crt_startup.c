#include <crt/global.h>
#include <internal/branch.h>
#include <stdlib.h>

void __crt_cleanup() {
  for (void (**destructor)() = __fini_array_start; destructor != __fini_array_end; ++destructor) {
    (*destructor)();
  }
}

int __crt_startup(uintptr_t this_task_cap, uintptr_t apm_task_cap, uintptr_t heap_start) {
  __this_task_cap = this_task_cap;
  __apm_task_cap  = apm_task_cap;
  __heap_start    = heap_start;

  __if_unlikely (atexit(__crt_cleanup) != 0) {
    return 1;
  }

  for (void (**constructor)() = __init_array_start; constructor != __init_array_end; ++constructor) {
    (*constructor)();
  }

  if (__apm_task_cap != 0) {
    // TODO
  }

  return 0;
}
