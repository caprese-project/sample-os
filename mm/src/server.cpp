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
      msg_buf->data[0]          = msg_buf_transfer(copied_id_cap);
      msg_buf->data[1]          = MM_CODE_S_OK;
    } else {
      sys_cap_destroy(id_cap);

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

    sys_cap_destroy(id_cap);
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

    sys_cap_destroy(id_cap);

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

    sys_cap_destroy(src_id_cap);

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

  void vpmap(message_buffer_t* msg_buf) {
    assert(msg_buf->data[msg_buf->cap_part_length] == MM_MSG_TYPE_VPMAP);

    if (msg_buf->cap_part_length != 2 || msg_buf->data_part_length < 3) [[unlikely]] {
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = MM_CODE_E_ILL_ARGS;
      return;
    }

    id_cap_t        id_cap        = msg_buf->data[0];
    virt_page_cap_t virt_page_cap = msg_buf->data[1];
    int             flags         = msg_buf->data[msg_buf->cap_part_length + 1];
    uintptr_t       va_base       = msg_buf->data[msg_buf->cap_part_length + 2];

    if (unwrap_sysret(sys_cap_type(id_cap)) != CAP_ID || unwrap_sysret(sys_cap_type(virt_page_cap)) != CAP_VIRT_PAGE) [[unlikely]] {
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = MM_CODE_E_ILL_ARGS;
      return;
    }

    uintptr_t act_va_base;
    int       result = vpmap_task(id_cap, flags, virt_page_cap, va_base, &act_va_base);

    sys_cap_destroy(id_cap);

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

  void vpremap(message_buffer_t* msg_buf) {
    assert(msg_buf->data[msg_buf->cap_part_length] == MM_MSG_TYPE_VPREMAP);

    if (msg_buf->cap_part_length != 3 || msg_buf->data_part_length < 3) [[unlikely]] {
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = MM_CODE_E_ILL_ARGS;
      return;
    }

    id_cap_t        src_id_cap    = msg_buf->data[0];
    id_cap_t        dst_id_cap    = msg_buf->data[1];
    virt_page_cap_t virt_page_cap = msg_buf->data[2];
    int             flags         = msg_buf->data[msg_buf->cap_part_length + 1];
    uintptr_t       va_base       = msg_buf->data[msg_buf->cap_part_length + 2];

    if (unwrap_sysret(sys_cap_type(src_id_cap)) != CAP_ID || unwrap_sysret(sys_cap_type(dst_id_cap)) != CAP_ID || unwrap_sysret(sys_cap_type(virt_page_cap)) != CAP_VIRT_PAGE) [[unlikely]] {
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = MM_CODE_E_ILL_ARGS;
      return;
    }

    uintptr_t act_va_base;
    int       result = vpremap_task(src_id_cap, dst_id_cap, flags, virt_page_cap, va_base, &act_va_base);

    sys_cap_destroy(src_id_cap);
    sys_cap_destroy(dst_id_cap);

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

    if (msg_buf->cap_part_length != 0 || msg_buf->data_part_length < 3) [[unlikely]] {
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = MM_CODE_E_ILL_ARGS;
      return;
    }

    size_t size      = msg_buf->data[1];
    size_t alignment = msg_buf->data[2];

    mem_cap_t mem_cap = fetch_mem_cap(size, alignment);
    if (mem_cap == 0) [[unlikely]] {
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = MM_CODE_E_FAILURE;
      return;
    }

    msg_buf->cap_part_length  = 1;
    msg_buf->data[0]          = msg_buf_transfer(mem_cap);
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

  // clang-format off

  void (*const table[])(message_buffer_t*) = {
    [0]                   = nullptr,
    [MM_MSG_TYPE_ATTACH]  = attach,
    [MM_MSG_TYPE_DETACH]  = detach,
    [MM_MSG_TYPE_VMAP]    = vmap,
    [MM_MSG_TYPE_VREMAP]  = vremap,
    [MM_MSG_TYPE_VPMAP]   = vpmap,
    [MM_MSG_TYPE_VPREMAP] = vpremap,
    [MM_MSG_TYPE_FETCH]   = fetch,
    [MM_MSG_TYPE_REVOKE]  = revoke,
    [MM_MSG_TYPE_INFO]    = info,
  };

  // clang-format on

  void proc_msg(message_buffer_t* msg_buf) {
    assert(msg_buf != NULL);

    if (msg_buf->data_part_length == 0) {
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = MM_CODE_E_ILL_ARGS;
      return;
    }

    uintptr_t msg_type = msg_buf->data[msg_buf->cap_part_length];

    if (msg_type < MM_MSG_TYPE_ATTACH || msg_type > MM_MSG_TYPE_INFO) [[unlikely]] {
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = MM_CODE_E_ILL_ARGS;
    } else {
      table[msg_type](msg_buf);
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
      mem_cap_t mem_cap = fetch_mem_cap(cap_space_size, cap_space_align);
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
