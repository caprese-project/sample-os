#include <assert.h>
#include <libcaprese/syscall.h>
#include <mm/ipc.h>
#include <service/mm.h>

id_cap_t mm_attach(endpoint_cap_t mm_ep_cap, task_cap_t task_cap, page_table_cap_t root_page_table_cap, uintptr_t heap_root) {
  assert(unwrap_sysret(sys_cap_type(mm_ep_cap)) == CAP_ENDPOINT);
  assert(unwrap_sysret(sys_cap_type(task_cap)) == CAP_TASK);
  assert(unwrap_sysret(sys_cap_type(root_page_table_cap)) == CAP_PAGE_TABLE);

  task_cap_t copied_task_cap = unwrap_sysret(sys_task_cap_copy(task_cap));

  message_buffer_t msg_buf = {};

  msg_buf.cap_part_length = 2;
  msg_buf.data[0]         = copied_task_cap;
  msg_buf.data[1]         = root_page_table_cap;

  msg_buf.data_part_length = 2;
  msg_buf.data[2]          = MM_MSG_TYPE_ATTACH;
  msg_buf.data[3]          = heap_root;

  sysret_t sysret = sys_endpoint_cap_call(mm_ep_cap, &msg_buf);

  if (sysret_failed(sysret)) {
    return 0;
  }

  if (msg_buf.data[msg_buf.cap_part_length] != MM_CODE_S_OK) {
    return 0;
  }

  assert(msg_buf.cap_part_length == 1);
  assert(msg_buf.data_part_length == 1);

  return msg_buf.data[0];
}

bool mm_detach(endpoint_cap_t mm_ep_cap, id_cap_t id_cap) {
  assert(unwrap_sysret(sys_cap_type(mm_ep_cap)) == CAP_ENDPOINT);
  assert(unwrap_sysret(sys_cap_type(id_cap)) == CAP_ID);

  message_buffer_t msg_buf = {};

  msg_buf.cap_part_length = 1;
  msg_buf.data[0]         = id_cap;

  msg_buf.data_part_length = 1;
  msg_buf.data[1]          = MM_MSG_TYPE_DETACH;

  sysret_t sysret = sys_endpoint_cap_call(mm_ep_cap, &msg_buf);

  if (sysret_failed(sysret)) {
    return false;
  }

  if (msg_buf.data[msg_buf.cap_part_length] != MM_CODE_S_OK) {
    return false;
  }

  assert(msg_buf.cap_part_length == 0);
  assert(msg_buf.data_part_length == 1);

  return true;
}

uintptr_t mm_sbrk(endpoint_cap_t mm_ep_cap, id_cap_t id_cap, intptr_t increment) {
  assert(unwrap_sysret(sys_cap_type(mm_ep_cap)) == CAP_ENDPOINT);
  assert(unwrap_sysret(sys_cap_type(id_cap)) == CAP_ID);

  task_cap_t copied_id_cap = unwrap_sysret(sys_id_cap_copy(id_cap));

  message_buffer_t msg_buf = {};

  msg_buf.cap_part_length = 1;
  msg_buf.data[0]         = copied_id_cap;

  msg_buf.data_part_length = 2;
  msg_buf.data[1]          = MM_MSG_TYPE_SBRK;
  msg_buf.data[2]          = increment;

  sysret_t sysret = sys_endpoint_cap_call(mm_ep_cap, &msg_buf);

  if (sysret_failed(sysret)) {
    return (uintptr_t)-1;
  }

  if (msg_buf.data[msg_buf.cap_part_length] != MM_CODE_S_OK) {
    return (uintptr_t)-1;
  }

  assert(msg_buf.cap_part_length == 0);
  assert(msg_buf.data_part_length == 2);

  return msg_buf.data[msg_buf.cap_part_length + 1];
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

  if (sysret_failed(sysret)) {
    return 0;
  }

  if (msg_buf.data[msg_buf.cap_part_length] != MM_CODE_S_OK) {
    return 0;
  }

  assert(msg_buf.cap_part_length == 1);
  assert(msg_buf.data_part_length == 1);

  return msg_buf.data[0];
}

mem_cap_t mm_retrieve(endpoint_cap_t mm_ep_cap, uintptr_t addr, size_t size, int flags) {
  assert(unwrap_sysret(sys_cap_type(mm_ep_cap)) == CAP_ENDPOINT);

  message_buffer_t msg_buf = {};

  msg_buf.cap_part_length = 0;

  msg_buf.data_part_length = 4;
  msg_buf.data[0]          = MM_MSG_TYPE_RETRIEVE;
  msg_buf.data[1]          = addr;
  msg_buf.data[2]          = size;
  msg_buf.data[3]          = flags;

  sysret_t sysret = sys_endpoint_cap_call(mm_ep_cap, &msg_buf);

  if (sysret_failed(sysret)) {
    return 0;
  }

  if (msg_buf.data[msg_buf.cap_part_length] != MM_CODE_S_OK) {
    return 0;
  }

  assert(msg_buf.cap_part_length == 1);
  assert(msg_buf.data_part_length == 1);

  return msg_buf.data[0];
}

bool mm_revoke(endpoint_cap_t mm_ep_cap, mem_cap_t mem_cap) {
  assert(unwrap_sysret(sys_cap_type(mm_ep_cap)) == CAP_ENDPOINT);
  assert(unwrap_sysret(sys_cap_type(mem_cap)) == CAP_MEM);

  message_buffer_t msg_buf = {};

  msg_buf.cap_part_length = 1;
  msg_buf.data[0]         = mem_cap;

  msg_buf.data_part_length = 1;
  msg_buf.data[1]          = MM_MSG_TYPE_REVOKE;

  sysret_t sysret = sys_endpoint_cap_call(mm_ep_cap, &msg_buf);

  if (sysret_failed(sysret)) {
    return false;
  }

  if (msg_buf.data[msg_buf.cap_part_length] != MM_CODE_S_OK) {
    return false;
  }

  assert(msg_buf.cap_part_length == 0);
  assert(msg_buf.data_part_length == 1);

  return true;
}
