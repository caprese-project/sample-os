#include <cons/file.h>
#include <cons/server.h>
#include <crt/global.h>
#include <cstdlib>
#include <service/apm.h>
#include <service/fs.h>

int main() {
  endpoint_cap_t uart_ep_cap = apm_lookup("uart");
  if (uart_ep_cap == 0) [[unlikely]] {
    return 1;
  }

  cons_mkfile("tty/0", file_type::uart, uart_ep_cap);

  cons_id_cap = fs_mount(__this_ep_cap, "/cons");
  if (cons_id_cap == 0) [[unlikely]] {
    return 1;
  }

  atexit([] { fs_unmount(cons_id_cap); });

  run();
}
