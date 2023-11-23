#ifndef LIBC_CRT_GLOBAL_H_
#define LIBC_CRT_GLOBAL_H_

#include <libcaprese/cap.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

  extern void (*__init_array_start[])();
  extern void (*__init_array_end[])();
  extern void (*__fini_array_start[])();
  extern void (*__fini_array_end[])();

  extern task_cap_t     __this_task_cap;
  extern endpoint_cap_t __apm_ep_cap;
  extern uintptr_t      __heap_start;

#ifdef __cplusplus
} // extern "C"
#endif // __cplusplus

#endif // LIBC_CRT_GLOBAL_H_
