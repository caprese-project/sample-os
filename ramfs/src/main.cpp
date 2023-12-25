#include <crt/global.h>
#include <cstdint>
#include <cstdlib>
#include <ramfs/server.h>
#include <service/fs.h>

int main() {
  uintptr_t ramfs_va_base = __init_context.__arg_regs[0];

  ramfs_id_cap = fs_mount(__this_ep_cap, "/init");
  if (ramfs_id_cap == 0) [[unlikely]] {
    return 1;
  }

  atexit([]() { fs_unmount(ramfs_id_cap); });

  run(ramfs_va_base);
}
