#include <cassert>
#include <crt/global.h>
#include <libcaprese/syscall.h>
#include <mm/ipc.h>
#include <mm/memory_manager.h>
#include <mm/server.h>
#include <mm/task_table.h>

namespace {
  void attach(message_buffer_t* msg_buf) {
    assert(msg_buf->data[msg_buf->cap_part_length] == MM_MSG_TYPE_ATTACH);

    if (msg_buf->cap_part_length != 2 || msg_buf->data_part_length < 2) [[unlikely]] {
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = MM_CODE_E_ILL_ARGS;
      return;
    }

    task_cap_t       task_cap            = msg_buf->data[0];
    page_table_cap_t root_page_table_cap = msg_buf->data[1];
    size_t           stack_available     = msg_buf->data[msg_buf->cap_part_length + 1];
    size_t           total_available     = msg_buf->data[msg_buf->cap_part_length + 2];
    size_t           stack_commit        = msg_buf->data[msg_buf->cap_part_length + 3];

    if (unwrap_sysret(sys_cap_type(task_cap)) != CAP_TASK || unwrap_sysret(sys_cap_type(root_page_table_cap)) != CAP_PAGE_TABLE) [[unlikely]] {
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = MM_CODE_E_ILL_ARGS;
      return;
    }

    id_cap_t id_cap = unwrap_sysret(sys_id_cap_create());

    int result = attach_task(id_cap, task_cap, root_page_table_cap, stack_available, total_available, stack_commit, false);

    if (result == MM_CODE_S_OK) [[likely]] {
      id_cap_t copied_id_cap = unwrap_sysret(sys_id_cap_copy(id_cap));

      msg_buf->cap_part_length  = 1;
      msg_buf->data_part_length = 1;
      msg_buf->data[0]          = copied_id_cap;
      msg_buf->data[1]          = MM_CODE_S_OK;
    } else {
      msg_buf->cap_part_length  = 0;
      msg_buf->data_part_length = 1;
      msg_buf->data[0]          = result;
    }
  }

  void detach(message_buffer_t* msg_buf) {
    assert(msg_buf->data[msg_buf->cap_part_length] == MM_MSG_TYPE_DETACH);

    if (msg_buf->cap_part_length != 1 || msg_buf->data_part_length < 1) [[unlikely]] {
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = MM_CODE_E_ILL_ARGS;
      return;
    }

    id_cap_t id_cap = msg_buf->data[msg_buf->cap_part_length];

    if (unwrap_sysret(sys_cap_type(id_cap)) != CAP_ID) [[unlikely]] {
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = MM_CODE_E_ILL_ARGS;
      return;
    }

    msg_buf->cap_part_length  = 0;
    msg_buf->data_part_length = 1;
    msg_buf->data[0]          = detach_task(id_cap);
  }

  void vmap(message_buffer_t* msg_buf) {
    assert(msg_buf->data[msg_buf->cap_part_length] == MM_MSG_TYPE_VMAP);

    if (msg_buf->cap_part_length != 1 || msg_buf->data_part_length < 4) [[unlikely]] {
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = MM_CODE_E_ILL_ARGS;
      return;
    }

    id_cap_t  id_cap  = msg_buf->data[0];
    int       level   = msg_buf->data[msg_buf->cap_part_length + 1];
    int       flags   = msg_buf->data[msg_buf->cap_part_length + 2];
    uintptr_t va_base = msg_buf->data[msg_buf->cap_part_length + 3];

    if (unwrap_sysret(sys_cap_type(id_cap)) != CAP_ID) [[unlikely]] {
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = MM_CODE_E_ILL_ARGS;
      return;
    }

    uintptr_t act_va_base;
    int       result = vmap_task(id_cap, level, flags, va_base, &act_va_base);
    if (result != MM_CODE_S_OK) [[unlikely]] {
      msg_buf->cap_part_length  = 0;
      msg_buf->data_part_length = 1;
      msg_buf->data[0]          = result;
      return;
    }

    msg_buf->cap_part_length  = 0;
    msg_buf->data_part_length = 2;
    msg_buf->data[0]          = MM_CODE_S_OK;
    msg_buf->data[1]          = act_va_base;
  }

  void vremap(message_buffer_t* msg_buf) {
    assert(msg_buf->data[msg_buf->cap_part_length] == MM_MSG_TYPE_VREMAP);

    if (msg_buf->cap_part_length != 2 || msg_buf->data_part_length < 4) [[unlikely]] {
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = MM_CODE_E_ILL_ARGS;
      return;
    }

    id_cap_t  src_id_cap  = msg_buf->data[0];
    id_cap_t  dst_id_cap  = msg_buf->data[1];
    int       flags       = msg_buf->data[msg_buf->cap_part_length + 1];
    uintptr_t src_va_base = msg_buf->data[msg_buf->cap_part_length + 2];
    uintptr_t dst_va_base = msg_buf->data[msg_buf->cap_part_length + 3];

    if (unwrap_sysret(sys_cap_type(src_id_cap)) != CAP_ID || unwrap_sysret(sys_cap_type(dst_id_cap)) != CAP_ID) [[unlikely]] {
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = MM_CODE_E_ILL_ARGS;
      return;
    }

    uintptr_t act_va_base;
    int       result = vremap_task(src_id_cap, dst_id_cap, flags, src_va_base, dst_va_base, &act_va_base);
    if (result != MM_CODE_S_OK) [[unlikely]] {
      msg_buf->cap_part_length  = 0;
      msg_buf->data_part_length = 1;
      msg_buf->data[0]          = result;
      return;
    }

    msg_buf->cap_part_length  = 0;
    msg_buf->data_part_length = 2;
    msg_buf->data[0]          = MM_CODE_S_OK;
    msg_buf->data[1]          = act_va_base;
  }

  void fetch(message_buffer_t* msg_buf) {
    assert(msg_buf->data[msg_buf->cap_part_length] == MM_MSG_TYPE_FETCH);

    if (msg_buf->cap_part_length != 0 || msg_buf->data_part_length < 4) [[unlikely]] {
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = MM_CODE_E_ILL_ARGS;
      return;
    }

    size_t   size      = msg_buf->data[1];
    size_t   alignment = msg_buf->data[2];
    uint32_t flags     = msg_buf->data[3];

    mem_cap_t mem_cap = fetch_mem_cap(size, alignment, flags);
    if (mem_cap == 0) [[unlikely]] {
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = MM_CODE_E_FAILURE;
      return;
    }

    msg_buf->cap_part_length  = 1;
    msg_buf->data[0]          = mem_cap;
    msg_buf->data_part_length = 1;
    msg_buf->data[1]          = MM_CODE_S_OK;
  }

  void revoke(message_buffer_t* msg_buf) {
    assert(msg_buf->data[msg_buf->cap_part_length] == MM_MSG_TYPE_REVOKE);

    if (msg_buf->cap_part_length != 1) [[unlikely]] {
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = MM_CODE_E_ILL_ARGS;
      return;
    }

    mem_cap_t mem_cap = msg_buf->data[0];
    if (unwrap_sysret(sys_cap_type(mem_cap)) != CAP_MEM) [[unlikely]] {
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = MM_CODE_E_ILL_ARGS;
      return;
    }

    revoke_mem_cap(mem_cap);

    msg_buf->cap_part_length  = 0;
    msg_buf->data_part_length = 1;
    msg_buf->data[0]          = MM_CODE_S_OK;
  }

  void info(message_buffer_t* msg_buf) {
    assert(msg_buf->data[msg_buf->cap_part_length] == MM_MSG_TYPE_INFO);

    if (msg_buf->cap_part_length != 0) [[unlikely]] {
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = MM_CODE_E_ILL_ARGS;
      return;
    }

    msg_buf->cap_part_length  = 0;
    msg_buf->data_part_length = 1;
    msg_buf->data[0]          = MM_CODE_S_OK;
  }

  void proc_msg(message_buffer_t* msg_buf) {
    assert(msg_buf != NULL);

    if (msg_buf->data_part_length == 0) {
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = MM_CODE_E_ILL_ARGS;
      return;
    }

    uintptr_t msg_type = msg_buf->data[msg_buf->cap_part_length];

    switch (msg_type) {
      case MM_MSG_TYPE_ATTACH:
        attach(msg_buf);
        break;
      case MM_MSG_TYPE_DETACH:
        detach(msg_buf);
        break;
      case MM_MSG_TYPE_VMAP:
        vmap(msg_buf);
        break;
      case MM_MSG_TYPE_VREMAP:
        vremap(msg_buf);
        break;
      case MM_MSG_TYPE_FETCH:
        fetch(msg_buf);
        break;
      case MM_MSG_TYPE_REVOKE:
        revoke(msg_buf);
        break;
      case MM_MSG_TYPE_INFO:
        info(msg_buf);
        break;
      default:
        msg_buf->data_part_length               = 1;
        msg_buf->data[msg_buf->cap_part_length] = MM_CODE_E_ILL_ARGS;
        break;
    }
  }
} // namespace

[[noreturn]] void run(endpoint_cap_t ep_cap) {
  const size_t cap_space_size  = unwrap_sysret(sys_system_cap_size(CAP_CAP_SPACE));
  const size_t cap_space_align = unwrap_sysret(sys_system_cap_align(CAP_CAP_SPACE));

  message_buffer_t msg_buf;
  sysret_t         sysret;

  sysret.error = SYS_E_UNKNOWN;
  while (true) {
    if (unwrap_sysret(sys_task_cap_get_free_slot_count(__this_task_cap)) < 0x10) [[unlikely]] {
      mem_cap_t mem_cap = fetch_mem_cap(cap_space_size, cap_space_align, MM_FETCH_FLAG_READ | MM_FETCH_FLAG_WRITE);
      if (mem_cap == 0) [[unlikely]] {
        abort();
      }
      cap_space_cap_t cap_space_cap = unwrap_sysret(sys_mem_cap_create_cap_space_object(mem_cap));
      unwrap_sysret(sys_task_cap_insert_cap_space(__this_task_cap, cap_space_cap));
    }

    if (sysret_succeeded(sysret)) {
      sysret = sys_endpoint_cap_reply_and_receive(ep_cap, &msg_buf);
    } else {
      sysret = sys_endpoint_cap_receive(ep_cap, &msg_buf);
    }

    if (sysret_succeeded(sysret)) {
      proc_msg(&msg_buf);
    }
  }
}
