#include <fs/server.h>
#include <fs/vfs.h>

int main() {
  if (!vfs_init()) [[unlikely]] {
    return 1;
  }

  run();
}
