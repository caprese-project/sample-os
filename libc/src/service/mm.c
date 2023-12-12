#include <assert.h>
#include <crt/global.h>
#include <internal/branch.h>
#include <libcaprese/syscall.h>
#include <mm/ipc.h>
#include <service/mm.h>

id_cap_t mm_attach(task_cap_t task_cap, page_table_cap_t root_page_table_cap, size_t stack_available, size_t total_available, size_t stack_commit) {
  assert(unwrap_sysret(sys_cap_type(task_cap)) == CAP_TASK);
  assert(unwrap_sysret(sys_cap_type(root_page_table_cap)) == CAP_PAGE_TABLE);

  task_cap_t copied_task_cap = unwrap_sysret(sys_task_cap_copy(task_cap));

  message_buffer_t msg_buf = {};

  msg_buf.cap_part_length = 2;
  msg_buf.data[0]         = copied_task_cap;
  msg_buf.data[1]         = root_page_table_cap;

  msg_buf.data_part_length = 4;
  msg_buf.data[2]          = MM_MSG_TYPE_ATTACH;
  msg_buf.data[3]          = stack_available;
  msg_buf.data[4]          = total_available;
  msg_buf.data[5]          = stack_commit;

  sysret_t sysret = sys_endpoint_cap_call(__mm_ep_cap, &msg_buf);

  __if_unlikely (sysret_failed(sysret)) {
    return 0;
  }

  __if_unlikely (msg_buf.data[msg_buf.cap_part_length] != MM_CODE_S_OK) {
    return 0;
  }

  assert(msg_buf.cap_part_length == 1);
  assert(msg_buf.data_part_length == 1);

  return msg_buf.data[0];
}

bool mm_detach(id_cap_t id_cap) {
  assert(unwrap_sysret(sys_cap_type(id_cap)) == CAP_ID);

  message_buffer_t msg_buf = {};

  msg_buf.cap_part_length = 1;
  msg_buf.data[0]         = id_cap;

  msg_buf.data_part_length = 1;
  msg_buf.data[1]          = MM_MSG_TYPE_DETACH;

  sysret_t sysret = sys_endpoint_cap_call(__mm_ep_cap, &msg_buf);

  __if_unlikely (sysret_failed(sysret)) {
    return false;
  }

  __if_unlikely (msg_buf.data[msg_buf.cap_part_length] != MM_CODE_S_OK) {
    return false;
  }

  assert(msg_buf.cap_part_length == 0);
  assert(msg_buf.data_part_length == 1);

  return true;
}

uintptr_t mm_vmap(id_cap_t id_cap, int level, int flags, uintptr_t va_base, virt_page_cap_t* dst) {
  assert(unwrap_sysret(sys_cap_type(id_cap)) == CAP_ID);

  message_buffer_t msg_buf = {};

  msg_buf.cap_part_length = 1;
  msg_buf.data[0]         = id_cap;

  msg_buf.data_part_length = 4;
  msg_buf.data[1]          = MM_MSG_TYPE_VMAP;
  msg_buf.data[2]          = level;
  msg_buf.data[3]          = flags;
  msg_buf.data[4]          = va_base;

  sysret_t sysret = sys_endpoint_cap_call(__mm_ep_cap, &msg_buf);

  __if_unlikely (sysret_failed(sysret)) {
    return 0;
  }

  __if_unlikely (msg_buf.data[msg_buf.cap_part_length] != MM_CODE_S_OK) {
    return 0;
  }

  assert(msg_buf.cap_part_length == 1);
  assert(msg_buf.data_part_length == 2);

  __if_unlikely (dst != NULL) {
    *dst = msg_buf.data[0];
  }

  return msg_buf.data[msg_buf.cap_part_length + 1];
}

uintptr_t mm_vremap(id_cap_t src_id_cap, id_cap_t dst_id_cap, int flags, uintptr_t dst_va_base, virt_page_cap_t page_cap) {
  assert(unwrap_sysret(sys_cap_type(src_id_cap)) == CAP_ID);
  assert(unwrap_sysret(sys_cap_type(dst_id_cap)) == CAP_ID);
  assert(unwrap_sysret(sys_cap_type(page_cap)) == CAP_VIRT_PAGE);

  message_buffer_t msg_buf = {};

  msg_buf.cap_part_length = 3;
  msg_buf.data[0]         = src_id_cap;
  msg_buf.data[1]         = dst_id_cap;
  msg_buf.data[2]         = page_cap;

  msg_buf.data_part_length = 3;
  msg_buf.data[3]          = MM_MSG_TYPE_VREMAP;
  msg_buf.data[4]          = flags;
  msg_buf.data[5]          = dst_va_base;

  sysret_t sysret = sys_endpoint_cap_call(__mm_ep_cap, &msg_buf);

  __if_unlikely (sysret_failed(sysret)) {
    return 0;
  }

  __if_unlikely (msg_buf.data[msg_buf.cap_part_length] != MM_CODE_S_OK) {
    return 0;
  }

  assert(msg_buf.cap_part_length == 0);
  assert(msg_buf.data_part_length == 2);

  return msg_buf.data[msg_buf.cap_part_length + 1];
}

mem_cap_t mm_fetch(size_t size, size_t alignment, int flags) {
  message_buffer_t msg_buf = {};

  msg_buf.cap_part_length = 0;

  msg_buf.data_part_length = 4;
  msg_buf.data[0]          = MM_MSG_TYPE_FETCH;
  msg_buf.data[1]          = size;
  msg_buf.data[2]          = alignment;
  msg_buf.data[3]          = flags;

  sysret_t sysret = sys_endpoint_cap_call(__mm_ep_cap, &msg_buf);

  __if_unlikely (sysret_failed(sysret)) {
    return 0;
  }

  __if_unlikely (msg_buf.data[msg_buf.cap_part_length] != MM_CODE_S_OK) {
    return 0;
  }

  assert(msg_buf.cap_part_length == 1);
  assert(msg_buf.data_part_length == 1);

  return msg_buf.data[0];
}

bool mm_revoke(mem_cap_t mem_cap) {
  assert(unwrap_sysret(sys_cap_type(mem_cap)) == CAP_MEM);

  message_buffer_t msg_buf = {};

  msg_buf.cap_part_length = 1;
  msg_buf.data[0]         = mem_cap;

  msg_buf.data_part_length = 1;
  msg_buf.data[1]          = MM_MSG_TYPE_REVOKE;

  sysret_t sysret = sys_endpoint_cap_call(__mm_ep_cap, &msg_buf);

  __if_unlikely (sysret_failed(sysret)) {
    return false;
  }

  __if_unlikely (msg_buf.data[msg_buf.cap_part_length] != MM_CODE_S_OK) {
    return false;
  }

  assert(msg_buf.cap_part_length == 0);
  assert(msg_buf.data_part_length == 1);

  return true;
}

task_cap_t mm_fetch_and_create_task_object(
    cap_space_cap_t cap_space_cap, page_table_cap_t root_page_table_cap, page_table_cap_t cap_space_page_table0, page_table_cap_t cap_space_page_table1, page_table_cap_t cap_space_page_table2) {
  size_t size      = unwrap_sysret(sys_system_cap_size(CAP_TASK));
  size_t alignment = unwrap_sysret(sys_system_cap_align(CAP_TASK));

  mem_cap_t mem_cap = mm_fetch(size, alignment, MM_FETCH_FLAG_READ | MM_FETCH_FLAG_WRITE);
  __if_unlikely (mem_cap == 0) {
    return 0;
  }

  sysret_t sysret = sys_mem_cap_create_task_object(mem_cap, cap_space_cap, root_page_table_cap, cap_space_page_table0, cap_space_page_table1, cap_space_page_table2);
  __if_unlikely (sysret_failed(sysret)) {
    mm_revoke(mem_cap);
    return 0;
  }

  return unwrap_sysret(sysret);
}

endpoint_cap_t mm_fetch_and_create_endpoint_object() {
  size_t size      = unwrap_sysret(sys_system_cap_size(CAP_ENDPOINT));
  size_t alignment = unwrap_sysret(sys_system_cap_align(CAP_ENDPOINT));

  mem_cap_t mem_cap = mm_fetch(size, alignment, MM_FETCH_FLAG_READ | MM_FETCH_FLAG_WRITE);
  __if_unlikely (mem_cap == 0) {
    return 0;
  }

  sysret_t sysret = sys_mem_cap_create_endpoint_object(mem_cap);
  __if_unlikely (sysret_failed(sysret)) {
    mm_revoke(mem_cap);
    return 0;
  }

  return unwrap_sysret(sysret);
}

page_table_cap_t mm_fetch_and_create_page_table_object() {
  size_t size      = unwrap_sysret(sys_system_cap_size(CAP_PAGE_TABLE));
  size_t alignment = unwrap_sysret(sys_system_cap_align(CAP_PAGE_TABLE));

  mem_cap_t mem_cap = mm_fetch(size, alignment, MM_FETCH_FLAG_READ | MM_FETCH_FLAG_WRITE);
  __if_unlikely (mem_cap == 0) {
    return 0;
  }

  sysret_t sysret = sys_mem_cap_create_page_table_object(mem_cap);
  __if_unlikely (sysret_failed(sysret)) {
    mm_revoke(mem_cap);
    return 0;
  }

  return unwrap_sysret(sysret);
}

virt_page_cap_t mm_fetch_and_create_virt_page_object(uintptr_t level) {
  size_t size      = unwrap_sysret(sys_system_cap_size(CAP_VIRT_PAGE));
  size_t alignment = unwrap_sysret(sys_system_cap_align(CAP_VIRT_PAGE));

  mem_cap_t mem_cap = mm_fetch(size, alignment, MM_FETCH_FLAG_READ | MM_FETCH_FLAG_WRITE);
  __if_unlikely (mem_cap == 0) {
    return 0;
  }

  sysret_t sysret = sys_mem_cap_create_virt_page_object(mem_cap, level);
  __if_unlikely (sysret_failed(sysret)) {
    mm_revoke(mem_cap);
    return 0;
  }

  return unwrap_sysret(sysret);
}

cap_space_cap_t mm_fetch_and_create_cap_space_object() {
  size_t size      = unwrap_sysret(sys_system_cap_size(CAP_CAP_SPACE));
  size_t alignment = unwrap_sysret(sys_system_cap_align(CAP_CAP_SPACE));

  mem_cap_t mem_cap = mm_fetch(size, alignment, MM_FETCH_FLAG_READ | MM_FETCH_FLAG_WRITE);
  __if_unlikely (mem_cap == 0) {
    return 0;
  }

  sysret_t sysret = sys_mem_cap_create_cap_space_object(mem_cap);
  __if_unlikely (sysret_failed(sysret)) {
    mm_revoke(mem_cap);
    return 0;
  }

  return unwrap_sysret(sysret);
}