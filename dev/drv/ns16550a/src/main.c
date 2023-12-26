#include "server.h"
#include "uart.h"

#include <crt/global.h>
#include <libcaprese/syscall.h>
#include <service/mm.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
  endpoint_cap_t dm_ep_cap = __init_context.__arg_regs[0];
  if (unwrap_sysret(sys_cap_type(dm_ep_cap)) != CAP_ENDPOINT) {
    abort();
  }

  message_buffer_t msg_buf;
  unwrap_sysret(sys_endpoint_cap_receive(dm_ep_cap, &msg_buf));

  virt_page_cap_t vp_cap    = unwrap_sysret(sys_mem_cap_create_virt_page_object(msg_buf.data[0], true, true, false, KILO_PAGE));
  uintptr_t       base_addr = mm_vpmap(__mm_id_cap, MM_VMAP_FLAG_READ | MM_VMAP_FLAG_WRITE, vp_cap, 0);
  if (base_addr == 0) {
    abort();
  }

  init_uart(base_addr);

  run();

  return 0;
}
