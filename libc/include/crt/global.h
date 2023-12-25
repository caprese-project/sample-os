#ifndef LIBC_CRT_GLOBAL_H_
#define LIBC_CRT_GLOBAL_H_

#include <libcaprese/cap.h>
#include <libcaprese/syscall.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

  extern void (*__init_array_start[])();
  extern void (*__init_array_end[])();
  extern void (*__fini_array_start[])();
  extern void (*__fini_array_end[])();

  struct __init_context {
    uintptr_t __arg_regs[REG_NUM_ARGS];
  };

  extern struct __init_context __init_context;

  extern task_cap_t     __this_task_cap;
  extern endpoint_cap_t __apm_ep_cap;
  extern endpoint_cap_t __mm_ep_cap;
  extern id_cap_t       __mm_id_cap;
  extern endpoint_cap_t __this_ep_cap;
  extern endpoint_cap_t __fs_ep_cap;
  extern uintptr_t      __brk_start;
  extern uintptr_t      __brk_pos;

#ifdef __cplusplus
} // extern "C"
#endif // __cplusplus

#endif // LIBC_CRT_GLOBAL_H_
