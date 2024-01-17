#include <crt/global.h>
#include <cstdint>
#include <cstdlib>
#include <ramfs/fs.h>
#include <ramfs/server.h>
#include <service/fs.h>

int main() {
  uintptr_t ramfs_va_base = __init_context.__arg_regs[0];

  if (!ramfs_init(ramfs_va_base)) [[unlikely]] {
    return 1;
  }

  run();
}
