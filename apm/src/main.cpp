#include <apm/ramfs.h>
#include <apm/server.h>
#include <apm/task_manager.h>
#include <crt/global.h>
#include <crt/heap.h>
#include <libcaprese/cap.h>
#include <libcaprese/syscall.h>
#include <service/mm.h>
#include <sstream>
#include <stdlib.h>

extern "C" {
  extern const char _ramfs_start[];
  extern const char _ramfs_end[];
}

int main() {
  __mm_ep_cap = __init_context.__arg_regs[0];
  __mm_id_cap = __init_context.__arg_regs[1];

  if (__heap_sbrk() == nullptr) [[unlikely]] {
    abort();
  }

  __brk_start = __brk_pos - MEGA_PAGE_SIZE;
  __heap_init();

  apm_ep_cap = mm_fetch_and_create_endpoint_object();

  ramfs              fs(_ramfs_start, _ramfs_end);
  std::istringstream stream = fs.open_file("dm");
  if (!stream) {
    abort();
  }

  if (!create_task("dm", std::ref<std::istream>(stream))) {
    abort();
  }

  const task& dm = lookup_task("dm");
  dm.resume();
  dm.switch_task();

  run();
}
