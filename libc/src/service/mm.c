#include <assert.h>
#include <crt/global.h>
#include <internal/branch.h>
#include <libcaprese/ipc.h>
#include <libcaprese/syscall.h>
#include <mm/ipc.h>
#include <service/mm.h>

id_cap_t mm_attach(task_cap_t task_cap, page_table_cap_t root_page_table_cap, size_t stack_available, size_t total_available, size_t stack_commit, const void* stack_data, size_t stack_data_size) {
  assert(unwrap_sysret(sys_cap_type(task_cap)) == CAP_TASK);
  assert(unwrap_sysret(sys_cap_type(root_page_table_cap)) == CAP_PAGE_TABLE);

  __if_unlikely (stack_data == NULL) {
    stack_data_size = 0;
  }

  char       msg_buf[sizeof(struct message_header) + sizeof(uintptr_t) * 7 + stack_data_size];
  message_t* msg               = (message_t*)msg_buf;
  msg->header.payload_length   = 0;
  msg->header.payload_capacity = sizeof(msg_buf) - sizeof(struct message_header);

  set_ipc_data(msg, 0, MM_MSG_TYPE_ATTACH);
  set_ipc_cap(msg, 1, unwrap_sysret(sys_task_cap_copy(task_cap)), false);
  set_ipc_cap(msg, 2, root_page_table_cap, false);
  set_ipc_data(msg, 3, stack_available);
  set_ipc_data(msg, 4, total_available);
  set_ipc_data(msg, 5, stack_commit);
  set_ipc_data(msg, 6, stack_data_size);
  set_ipc_data_array(msg, 7, stack_data, stack_data_size);

  sysret_t sysret = sys_endpoint_cap_call(__mm_ep_cap, msg);

  __if_unlikely (sysret_failed(sysret)) {
    return 0;
  }

  int result = get_ipc_data(msg, 0);
  __if_unlikely (result != MM_CODE_S_OK) {
    return 0;
  }

  return move_ipc_cap(msg, 1);
}

bool mm_detach(id_cap_t id_cap) {
  assert(unwrap_sysret(sys_cap_type(id_cap)) == CAP_ID);

  char       msg_buf[sizeof(struct message_header) + sizeof(uintptr_t) * 2];
  message_t* msg               = (message_t*)msg_buf;
  msg->header.payload_length   = 0;
  msg->header.payload_capacity = sizeof(msg_buf) - sizeof(struct message_header);

  set_ipc_data(msg, 0, MM_MSG_TYPE_DETACH);
  set_ipc_cap(msg, 1, id_cap, false);

  sysret_t sysret = sys_endpoint_cap_call(__mm_ep_cap, msg);

  __if_unlikely (sysret_failed(sysret)) {
    return false;
  }

  int result = get_ipc_data(msg, 0);

  __if_unlikely (result != MM_CODE_S_OK) {
    return false;
  }

  return move_ipc_cap(msg, 1);
}

uintptr_t mm_vmap(id_cap_t id_cap, int level, int flags, uintptr_t va_base) {
  assert(unwrap_sysret(sys_cap_type(id_cap)) == CAP_ID);

  char       msg_buf[sizeof(struct message_header) + sizeof(uintptr_t) * 5];
  message_t* msg               = (message_t*)msg_buf;
  msg->header.payload_length   = 0;
  msg->header.payload_capacity = sizeof(msg_buf) - sizeof(struct message_header);

  set_ipc_data(msg, 0, MM_MSG_TYPE_VMAP);
  set_ipc_cap(msg, 1, id_cap, true);
  set_ipc_data(msg, 2, level);
  set_ipc_data(msg, 3, flags);
  set_ipc_data(msg, 4, va_base);

  sysret_t sysret = sys_endpoint_cap_call(__mm_ep_cap, msg);

  assert(unwrap_sysret(sys_cap_type(id_cap)) == CAP_ID);

  __if_unlikely (sysret_failed(sysret)) {
    return 0;
  }

  int result = get_ipc_data(msg, 0);

  __if_unlikely (result != MM_CODE_S_OK) {
    return 0;
  }

  return get_ipc_data(msg, 1);
}

uintptr_t mm_vremap(id_cap_t src_id_cap, id_cap_t dst_id_cap, int flags, uintptr_t src_va_base, uintptr_t dst_va_base) {
  assert(unwrap_sysret(sys_cap_type(src_id_cap)) == CAP_ID);
  assert(unwrap_sysret(sys_cap_type(dst_id_cap)) == CAP_ID);

  char       msg_buf[sizeof(struct message_header) + sizeof(uintptr_t) * 6];
  message_t* msg               = (message_t*)msg_buf;
  msg->header.payload_length   = 0;
  msg->header.payload_capacity = sizeof(msg_buf) - sizeof(struct message_header);

  set_ipc_data(msg, 0, MM_MSG_TYPE_VREMAP);
  set_ipc_cap(msg, 1, src_id_cap, true);
  set_ipc_cap(msg, 2, dst_id_cap, true);
  set_ipc_data(msg, 3, flags);
  set_ipc_data(msg, 4, src_va_base);
  set_ipc_data(msg, 5, dst_va_base);

  sysret_t sysret = sys_endpoint_cap_call(__mm_ep_cap, msg);

  assert(unwrap_sysret(sys_cap_type(src_id_cap)) == CAP_ID);
  assert(unwrap_sysret(sys_cap_type(dst_id_cap)) == CAP_ID);

  __if_unlikely (sysret_failed(sysret)) {
    return 0;
  }

  int result = get_ipc_data(msg, 0);

  __if_unlikely (result != MM_CODE_S_OK) {
    return 0;
  }

  return get_ipc_data(msg, 1);
}

uintptr_t mm_vpmap(id_cap_t id_cap, int flags, virt_page_cap_t virt_page_cap, uintptr_t va_base) {
  assert(unwrap_sysret(sys_cap_type(id_cap)) == CAP_ID);
  assert(unwrap_sysret(sys_cap_type(virt_page_cap)) == CAP_VIRT_PAGE);
  assert(unwrap_sysret(sys_virt_page_cap_mapped(virt_page_cap)) == false);

  char       msg_buf[sizeof(struct message_header) + sizeof(uintptr_t) * 5];
  message_t* msg               = (message_t*)msg_buf;
  msg->header.payload_length   = 0;
  msg->header.payload_capacity = sizeof(msg_buf) - sizeof(struct message_header);

  set_ipc_data(msg, 0, MM_MSG_TYPE_VPMAP);
  set_ipc_cap(msg, 1, id_cap, true);
  set_ipc_cap(msg, 2, virt_page_cap, false);
  set_ipc_data(msg, 3, flags);
  set_ipc_data(msg, 4, va_base);

  sysret_t sysret = sys_endpoint_cap_call(__mm_ep_cap, msg);

  assert(unwrap_sysret(sys_cap_type(id_cap)) == CAP_ID);

  __if_unlikely (sysret_failed(sysret)) {
    return 0;
  }

  int result = get_ipc_data(msg, 0);

  __if_unlikely (result != MM_CODE_S_OK) {
    return 0;
  }

  return get_ipc_data(msg, 1);
}

uintptr_t mm_vpremap(id_cap_t src_id_cap, id_cap_t dst_id_cap, int flags, virt_page_cap_t virt_page_cap, uintptr_t va_base) {
  assert(unwrap_sysret(sys_cap_type(src_id_cap)) == CAP_ID);
  assert(unwrap_sysret(sys_cap_type(dst_id_cap)) == CAP_ID);
  assert(unwrap_sysret(sys_cap_type(virt_page_cap)) == CAP_VIRT_PAGE);
  assert(unwrap_sysret(sys_virt_page_cap_mapped(virt_page_cap)) == true);

  char       msg_buf[sizeof(struct message_header) + sizeof(uintptr_t) * 6];
  message_t* msg               = (message_t*)msg_buf;
  msg->header.payload_length   = 0;
  msg->header.payload_capacity = sizeof(msg_buf) - sizeof(struct message_header);

  set_ipc_data(msg, 0, MM_MSG_TYPE_VPREMAP);
  set_ipc_cap(msg, 1, src_id_cap, true);
  set_ipc_cap(msg, 2, dst_id_cap, true);
  set_ipc_cap(msg, 3, virt_page_cap, false);
  set_ipc_data(msg, 4, flags);
  set_ipc_data(msg, 5, va_base);

  sysret_t sysret = sys_endpoint_cap_call(__mm_ep_cap, msg);

  assert(unwrap_sysret(sys_cap_type(src_id_cap)) == CAP_ID);
  assert(unwrap_sysret(sys_cap_type(dst_id_cap)) == CAP_ID);

  __if_unlikely (sysret_failed(sysret)) {
    return 0;
  }

  int result = get_ipc_data(msg, 0);

  __if_unlikely (result != MM_CODE_S_OK) {
    return 0;
  }

  return get_ipc_data(msg, 1);
}

mem_cap_t mm_fetch(size_t size, size_t alignment) {
  char       msg_buf[sizeof(struct message_header) + sizeof(uintptr_t) * 3];
  message_t* msg               = (message_t*)msg_buf;
  msg->header.payload_length   = 0;
  msg->header.payload_capacity = sizeof(msg_buf) - sizeof(struct message_header);

  set_ipc_data(msg, 0, MM_MSG_TYPE_FETCH);
  set_ipc_data(msg, 1, size);
  set_ipc_data(msg, 2, alignment);

  sysret_t sysret = sys_endpoint_cap_call(__mm_ep_cap, msg);

  __if_unlikely (sysret_failed(sysret)) {
    return 0;
  }

  int result = get_ipc_data(msg, 0);

  __if_unlikely (result != MM_CODE_S_OK) {
    return 0;
  }

  return move_ipc_cap(msg, 1);
}

bool mm_revoke(mem_cap_t mem_cap) {
  assert(unwrap_sysret(sys_cap_type(mem_cap)) == CAP_MEM);

  char       msg_buf[sizeof(struct message_header) + sizeof(uintptr_t) * 2];
  message_t* msg               = (message_t*)msg_buf;
  msg->header.payload_length   = 0;
  msg->header.payload_capacity = sizeof(msg_buf) - sizeof(struct message_header);

  set_ipc_data(msg, 0, MM_MSG_TYPE_REVOKE);
  set_ipc_cap(msg, 1, mem_cap, false);

  sysret_t sysret = sys_endpoint_cap_call(__mm_ep_cap, msg);

  __if_unlikely (sysret_failed(sysret)) {
    return false;
  }

  int result = get_ipc_data(msg, 0);

  return result == MM_CODE_S_OK;
}

task_cap_t mm_fetch_and_create_task_object(
    cap_space_cap_t cap_space_cap, page_table_cap_t root_page_table_cap, page_table_cap_t cap_space_page_table0, page_table_cap_t cap_space_page_table1, page_table_cap_t cap_space_page_table2) {
  size_t size      = unwrap_sysret(sys_system_cap_size(CAP_TASK));
  size_t alignment = unwrap_sysret(sys_system_cap_align(CAP_TASK));

  mem_cap_t mem_cap = mm_fetch(size, alignment);
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

  mem_cap_t mem_cap = mm_fetch(size, alignment);
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

  mem_cap_t mem_cap = mm_fetch(size, alignment);
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

virt_page_cap_t mm_fetch_and_create_virt_page_object(bool readable, bool writable, bool executable, uintptr_t level) {
  size_t size      = unwrap_sysret(sys_system_cap_size(CAP_VIRT_PAGE));
  size_t alignment = unwrap_sysret(sys_system_cap_align(CAP_VIRT_PAGE));

  mem_cap_t mem_cap = mm_fetch(size, alignment);
  __if_unlikely (mem_cap == 0) {
    return 0;
  }

  sysret_t sysret = sys_mem_cap_create_virt_page_object(mem_cap, readable, writable, executable, level);
  __if_unlikely (sysret_failed(sysret)) {
    mm_revoke(mem_cap);
    return 0;
  }

  return unwrap_sysret(sysret);
}

cap_space_cap_t mm_fetch_and_create_cap_space_object() {
  size_t size      = unwrap_sysret(sys_system_cap_size(CAP_CAP_SPACE));
  size_t alignment = unwrap_sysret(sys_system_cap_align(CAP_CAP_SPACE));

  mem_cap_t mem_cap = mm_fetch(size, alignment);
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
