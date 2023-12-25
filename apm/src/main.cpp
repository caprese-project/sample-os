#include <apm/server.h>
#include <crt/global.h>
#include <crt/heap.h>
#include <service/mm.h>
#include <stdlib.h>

void init() {
  endpoint_cap_t init_task_ep_cap = __init_context.__arg_regs[0];

  if (unwrap_sysret(sys_cap_type(init_task_ep_cap)) != CAP_ENDPOINT) [[unlikely]] {
    abort();
  }

  if (__heap_sbrk() == nullptr) [[unlikely]] {
    abort();
  }

  __brk_start = __brk_pos - MEGA_PAGE_SIZE;
  __heap_init();

  apm_ep_cap = mm_fetch_and_create_endpoint_object();

  message_buffer_t msg_buf;
  msg_buf.cap_part_length  = 1;
  msg_buf.data_part_length = 0;
  msg_buf.data[0]          = unwrap_sysret(sys_endpoint_cap_copy(apm_ep_cap));

  unwrap_sysret(sys_endpoint_cap_send_long(init_task_ep_cap, &msg_buf));
}

int main() {
  init();
  run();
}
