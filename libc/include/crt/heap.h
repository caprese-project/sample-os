#ifndef LIBC_CRT_HEAP_H_
#define LIBC_CRT_HEAP_H_

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

  void  __heap_init();
  void* __heap_alloc(size_t size);
  void  __heap_free(void* ptr);
  void* __heap_sbrk();

#ifdef __cplusplus
} // extern "C"
#endif // __cplusplus

#endif // LIBC_CRT_HEAP_H_
