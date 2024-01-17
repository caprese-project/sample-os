#include <cons/fs.h>
#include <cons/server.h>

int main() {
  if (!cons_init()) [[unlikely]] {
    return 1;
  }

  run();
}
