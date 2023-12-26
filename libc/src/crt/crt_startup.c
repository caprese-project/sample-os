#include <crt/global.h>
#include <crt/heap.h>
#include <internal/branch.h>
#include <libcaprese/cap.h>
#include <service/apm.h>
#include <stdio.h>
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
  __this_ep_cap   = __init_context.__arg_regs[6];

  __if_unlikely (atexit(__crt_cleanup) != 0) {
    return 1;
  }

  if (__apm_ep_cap != 0 && __mm_id_cap != 0) {
    void* heap_start = __heap_sbrk();
    __if_unlikely (heap_start == NULL) {
      return 1;
    }
    __brk_start = (uintptr_t)heap_start;
    __heap_init();

    __fs_ep_cap = apm_lookup("fs");
    __if_unlikely (unwrap_sysret(sys_cap_same(__this_ep_cap, __fs_ep_cap))) {
      sys_cap_destroy(__fs_ep_cap);
      __fs_ep_cap = 0;
    }
  }

  if (__fs_ep_cap != 0) {
    freopen("/cons/tty/0", "r", stdin);
    freopen("/cons/tty/0", "w", stdout);
    freopen("/cons/tty/0", "w", stderr);
  }

  for (void (**constructor)() = __init_array_start; constructor != __init_array_end; ++constructor) {
    (*constructor)();
  }

  return 0;
}
