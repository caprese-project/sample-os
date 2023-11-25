#include <crt/global.h>
#include <internal/branch.h>
#include <libcaprese/cap.h>
#include <stdlib.h>

void __crt_cleanup() {
  for (void (**destructor)() = __fini_array_start; destructor != __fini_array_end; ++destructor) {
    (*destructor)();
  }
}

int __crt_startup(task_cap_t this_task_cap, endpoint_cap_t apm_ep_cap) {
  __this_task_cap = this_task_cap;
  __apm_ep_cap    = apm_ep_cap;

  __if_unlikely (atexit(__crt_cleanup) != 0) {
    return 1;
  }

  for (void (**constructor)() = __init_array_start; constructor != __init_array_end; ++constructor) {
    (*constructor)();
  }

  if (__apm_ep_cap != 0) {
    // TODO
  }

  return 0;
}
