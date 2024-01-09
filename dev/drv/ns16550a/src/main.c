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

  char       msg_buf[sizeof(struct message_header) + sizeof(uintptr_t) * 1];
  message_t* msg               = (message_t*)msg_buf;
  msg->header.payload_length   = 0;
  msg->header.payload_capacity = sizeof(msg_buf) - sizeof(struct message_header);

  unwrap_sysret(sys_endpoint_cap_receive(dm_ep_cap, msg));

  virt_page_cap_t vp_cap    = unwrap_sysret(sys_mem_cap_create_virt_page_object(move_ipc_cap(msg, 0), true, true, false, KILO_PAGE));
  uintptr_t       base_addr = mm_vpmap(__mm_id_cap, MM_VMAP_FLAG_READ | MM_VMAP_FLAG_WRITE, vp_cap, 0);
  if (base_addr == 0) {
    abort();
  }

  init_uart(base_addr);

  run();

  return 0;
}
