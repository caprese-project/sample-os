#include <crt/global.h>
#include <internal/attribute.h>
#include <internal/branch.h>
#include <libcaprese/syscall.h>
#include <stdbool.h>
#include <stdlib.h>

__noreturn void _Exit(int status) {
  while (true) {
    sys_task_cap_kill(__this_task_cap, status);
  }
}

void* __alloc(void* ptr, size_t size, size_t alignment) {
  __if_unlikely (size == 0) {
    return NULL;
  }

  __if_unlikely (alignment < sizeof(max_align_t)) {
    alignment = sizeof(max_align_t);
  }

  return ptr;
}

void free(void* ptr) {
  (void)ptr;
}
