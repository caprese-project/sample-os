#include <assert.h>
#include <libcaprese/syscall.h>
#include <mm/ipc.h>
#include <service/mm.h>

bool mm_attach(endpoint_cap_t mm_ep_cap, task_cap_t task_cap) {
  assert(unwrap_sysret(sys_task_cap_switch(mm_ep_cap)) == CAP_ENDPOINT);
  assert(unwrap_sysret(sys_cap_type(task_cap)) == CAP_TASK);

  message_buffer_t msg_buf = {};

  msg_buf.cap_part_length = 1;
  msg_buf.data[0]         = task_cap;

  msg_buf.data_part_length = 1;
  msg_buf.data[1]          = MM_MSG_TYPE_ATTACH;

  sysret_t sysret = sys_endpoint_cap_send_long(mm_ep_cap, &msg_buf);

  return sysret.error == 0;
}

bool mm_detach(endpoint_cap_t mm_ep_cap, task_cap_t task_cap) {
  assert(unwrap_sysret(sys_task_cap_switch(mm_ep_cap)) == CAP_ENDPOINT);
  assert(unwrap_sysret(sys_cap_type(task_cap)) == CAP_TASK);

  message_buffer_t msg_buf = {};

  msg_buf.cap_part_length = 1;
  msg_buf.data[0]         = task_cap;

  msg_buf.data_part_length = 1;
  msg_buf.data[1]          = MM_MSG_TYPE_DETACH;

  sysret_t sysret = sys_endpoint_cap_send_long(mm_ep_cap, &msg_buf);

  return sysret.error == 0;
}

mem_cap_t mm_fetch(endpoint_cap_t mm_ep_cap, size_t size, size_t alignment, int flags) {
  assert(unwrap_sysret(sys_cap_type(mm_ep_cap)) == CAP_ENDPOINT);

  message_buffer_t msg_buf = {};

  msg_buf.cap_part_length = 0;

  msg_buf.data_part_length = 4;
  msg_buf.data[0]          = MM_MSG_TYPE_FETCH;
  msg_buf.data[1]          = size;
  msg_buf.data[2]          = alignment;
  msg_buf.data[3]          = flags;

  sysret_t sysret = sys_endpoint_cap_call(mm_ep_cap, &msg_buf);

  if (sysret.error != 0) {
    return 0;
  }

  return msg_buf.data[0];
}

void mm_revoke(endpoint_cap_t mm_ep_cap, mem_cap_t mem_cap) {
  assert(unwrap_sysret(sys_task_cap_switch(mm_ep_cap)) == CAP_ENDPOINT);
  assert(unwrap_sysret(sys_cap_type(mem_cap)) == CAP_MEM);

  message_buffer_t msg_buf = {};

  msg_buf.cap_part_length = 1;
  msg_buf.data[0]         = mem_cap;

  msg_buf.data_part_length = 1;
  msg_buf.data[1]          = MM_MSG_TYPE_REVOKE;

  sysret_t sysret = sys_endpoint_cap_send_long(mm_ep_cap, &msg_buf);

  assert(sysret.error == 0);
}
