#include <crt/global.h>
#include <crt/heap.h>
#include <internal/branch.h>
#include <libcaprese/cap.h>
#include <stdlib.h>

void __crt_cleanup() {
  for (void (**destructor)() = __fini_array_start; destructor != __fini_array_end; ++destructor) {
    (*destructor)();
  }
}

int __crt_startup() {
  __this_task_cap = __init_context.__arg_regs[2];
  __apm_ep_cap    = __init_context.__arg_regs[3];
  __mm_ep_cap     = __init_context.__arg_regs[4];
  __mm_id_cap     = __init_context.__arg_regs[5];

  __if_unlikely (atexit(__crt_cleanup) != 0) {
    return 1;
  }

  if (__apm_ep_cap != 0) {
    // TODO
  }

  for (void (**constructor)() = __init_array_start; constructor != __init_array_end; ++constructor) {
    (*constructor)();
  }

  return 0;
}
