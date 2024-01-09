#include <crt/global.h>
#include <crt/heap.h>
#include <internal/attribute.h>
#include <internal/branch.h>
#include <libcaprese/syscall.h>
#include <service/apm.h>
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

  ptr = __heap_alloc(size + alignment);

  return ptr;
}

void free(void* ptr) {
  __if_unlikely (ptr == NULL) {
    return;
  }

  __heap_free(ptr);
}

char* getenv(const char* name) {
  static char env_buf[APM_ENV_MAX_LEN];
  size_t      size = sizeof(env_buf);

  if (!apm_getenv(__this_task_cap, name, env_buf, &size)) {
    return NULL;
  }

  return env_buf;
}

int setenv(const char* name, const char* value, int overwrite) {
  if (overwrite) {
    apm_setenv(__this_task_cap, name, value);
  } else {
    if (getenv(name) == NULL) {
      apm_setenv(__this_task_cap, name, value);
    }
  }

  return 0;
}
