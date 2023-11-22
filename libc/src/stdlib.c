#include <crt/global.h>
#include <internal/attribute.h>
#include <libcaprese/syscall.h>
#include <stdbool.h>
#include <stdlib.h>

__noreturn void _Exit(int status) {
  while (true) {
    sys_task_cap_kill(__this_task_cap, status);
  }
}
