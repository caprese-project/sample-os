#include <cassert>
#include <crt/global.h>
#include <libcaprese/syscall.h>
#include <mm/ipc.h>
#include <mm/memory_manager.h>
#include <mm/server.h>
#include <mm/task_table.h>

namespace {
  void attach(message_t* msg) {
    assert(get_ipc_data(msg, 0) == MM_MSG_TYPE_ATTACH);

    task_cap_t       task_cap            = move_ipc_cap(msg, 1);
    page_table_cap_t root_page_table_cap = move_ipc_cap(msg, 2);
    size_t           stack_available     = get_ipc_data(msg, 3);
    size_t           total_available     = get_ipc_data(msg, 4);
    size_t           stack_commit        = get_ipc_data(msg, 5);

    if (unwrap_sysret(sys_cap_type(task_cap)) != CAP_TASK || unwrap_sysret(sys_cap_type(root_page_table_cap)) != CAP_PAGE_TABLE) [[unlikely]] {
      destroy_ipc_message(msg);
      set_ipc_data(msg, 0, MM_CODE_E_ILL_ARGS);
      return;
    }

    id_cap_t id_cap = unwrap_sysret(sys_id_cap_create());

    int result = attach_task(id_cap, task_cap, root_page_table_cap, stack_available, total_available, stack_commit, false);

    destroy_ipc_message(msg);

    set_ipc_data(msg, 0, result);

    if (result != MM_CODE_S_OK) [[unlikely]] {
      sys_cap_destroy(id_cap);
      return;
    }

    id_cap_t copied_id_cap = unwrap_sysret(sys_id_cap_copy(id_cap));
    set_ipc_cap(msg, 1, copied_id_cap, false);
  }

  void detach(message_t* msg) {
    assert(get_ipc_data(msg, 0) == MM_MSG_TYPE_DETACH);

    id_cap_t id_cap = get_ipc_cap(msg, 1);

    if (unwrap_sysret(sys_cap_type(id_cap)) != CAP_ID) [[unlikely]] {
      destroy_ipc_message(msg);
      set_ipc_data(msg, 0, MM_CODE_E_ILL_ARGS);
      return;
    }

    int result = detach_task(id_cap);

    destroy_ipc_message(msg);
    set_ipc_data(msg, 0, result);
  }

  void vmap(message_t* msg) {
    assert(get_ipc_data(msg, 0) == MM_MSG_TYPE_VMAP);

    id_cap_t  id_cap  = get_ipc_cap(msg, 1);
    int       level   = get_ipc_data(msg, 2);
    int       flags   = get_ipc_data(msg, 3);
    uintptr_t va_base = get_ipc_data(msg, 4);

    if (unwrap_sysret(sys_cap_type(id_cap)) != CAP_ID) [[unlikely]] {
      destroy_ipc_message(msg);
      set_ipc_data(msg, 0, MM_CODE_E_ILL_ARGS);
      return;
    }

    uintptr_t act_va_base;
    int       result = vmap_task(id_cap, level, flags, va_base, &act_va_base);

    destroy_ipc_message(msg);
    set_ipc_data(msg, 0, result);

    if (result != MM_CODE_S_OK) [[unlikely]] {
      return;
    }

    set_ipc_data(msg, 1, act_va_base);
  }

  void vremap(message_t* msg) {
    assert(get_ipc_data(msg, 0) == MM_MSG_TYPE_VREMAP);

    id_cap_t  src_id_cap  = get_ipc_cap(msg, 1);
    id_cap_t  dst_id_cap  = get_ipc_cap(msg, 2);
    int       flags       = get_ipc_data(msg, 3);
    uintptr_t src_va_base = get_ipc_data(msg, 4);
    uintptr_t dst_va_base = get_ipc_data(msg, 5);

    if (unwrap_sysret(sys_cap_type(src_id_cap)) != CAP_ID || unwrap_sysret(sys_cap_type(dst_id_cap)) != CAP_ID) [[unlikely]] {
      destroy_ipc_message(msg);
      set_ipc_data(msg, 0, MM_CODE_E_ILL_ARGS);
      return;
    }

    uintptr_t act_va_base;
    int       result = vremap_task(src_id_cap, dst_id_cap, flags, src_va_base, dst_va_base, &act_va_base);

    destroy_ipc_message(msg);
    set_ipc_data(msg, 0, result);

    if (result != MM_CODE_S_OK) [[unlikely]] {
      return;
    }

    set_ipc_data(msg, 1, act_va_base);
  }

  void vpmap(message_t* msg) {
    assert(get_ipc_data(msg, 0) == MM_MSG_TYPE_VPMAP);

    id_cap_t        id_cap        = get_ipc_cap(msg, 1);
    virt_page_cap_t virt_page_cap = move_ipc_cap(msg, 2);
    int             flags         = get_ipc_data(msg, 3);
    uintptr_t       va_base       = get_ipc_data(msg, 4);

    if (unwrap_sysret(sys_cap_type(id_cap)) != CAP_ID || unwrap_sysret(sys_cap_type(virt_page_cap)) != CAP_VIRT_PAGE) [[unlikely]] {
      sys_cap_destroy(virt_page_cap);
      destroy_ipc_message(msg);
      set_ipc_data(msg, 0, MM_CODE_E_ILL_ARGS);
      return;
    }

    uintptr_t act_va_base;
    int       result = vpmap_task(id_cap, flags, virt_page_cap, va_base, &act_va_base);

    destroy_ipc_message(msg);
    set_ipc_data(msg, 0, result);

    if (result != MM_CODE_S_OK) [[unlikely]] {
      sys_cap_destroy(virt_page_cap);
      return;
    }

    set_ipc_data(msg, 1, act_va_base);
  }

  void vpremap(message_t* msg) {
    assert(get_ipc_data(msg, 0) == MM_MSG_TYPE_VPREMAP);

    id_cap_t        src_id_cap    = get_ipc_cap(msg, 1);
    id_cap_t        dst_id_cap    = get_ipc_cap(msg, 2);
    virt_page_cap_t virt_page_cap = move_ipc_cap(msg, 3);
    int             flags         = get_ipc_data(msg, 4);
    uintptr_t       va_base       = get_ipc_data(msg, 5);

    if (unwrap_sysret(sys_cap_type(src_id_cap)) != CAP_ID || unwrap_sysret(sys_cap_type(dst_id_cap)) != CAP_ID || unwrap_sysret(sys_cap_type(virt_page_cap)) != CAP_VIRT_PAGE) [[unlikely]] {
      sys_cap_destroy(virt_page_cap);
      destroy_ipc_message(msg);
      set_ipc_data(msg, 0, MM_CODE_E_ILL_ARGS);
      return;
    }

    uintptr_t act_va_base;
    int       result = vpremap_task(src_id_cap, dst_id_cap, flags, virt_page_cap, va_base, &act_va_base);

    destroy_ipc_message(msg);
    set_ipc_data(msg, 0, result);

    if (result != MM_CODE_S_OK) [[unlikely]] {
      sys_cap_destroy(virt_page_cap);
    }

    set_ipc_data(msg, 1, act_va_base);
  }

  void fetch(message_t* msg) {
    assert(get_ipc_data(msg, 0) == MM_MSG_TYPE_FETCH);

    size_t size      = get_ipc_data(msg, 1);
    size_t alignment = get_ipc_data(msg, 2);

    mem_cap_t mem_cap = fetch_mem_cap(size, alignment);
    if (mem_cap == 0) [[unlikely]] {
      destroy_ipc_message(msg);
      set_ipc_data(msg, 0, MM_CODE_E_FAILURE);
      return;
    }

    destroy_ipc_message(msg);
    set_ipc_data(msg, 0, MM_CODE_S_OK);
    set_ipc_cap(msg, 1, mem_cap, true);
  }

  void revoke(message_t* msg) {
    assert(get_ipc_data(msg, 0) == MM_MSG_TYPE_REVOKE);

    mem_cap_t mem_cap = move_ipc_cap(msg, 1);
    if (unwrap_sysret(sys_cap_type(mem_cap)) != CAP_MEM) [[unlikely]] {
      sys_cap_destroy(mem_cap);
      destroy_ipc_message(msg);
      set_ipc_data(msg, 0, MM_CODE_E_ILL_ARGS);
      return;
    }

    revoke_mem_cap(mem_cap);
    destroy_ipc_message(msg);
    set_ipc_data(msg, 0, MM_CODE_S_OK);
  }

  void info(message_t* msg) {
    assert(get_ipc_data(msg, 0) == MM_MSG_TYPE_INFO);

    destroy_ipc_message(msg);
    set_ipc_data(msg, 0, MM_CODE_S_OK);
  }

  // clang-format off

  constexpr void (*const table[])(message_t*) = {
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

  void proc_msg(message_t* msg) {
    assert(msg != nullptr);

    uintptr_t msg_type = get_ipc_data(msg, 0);

    if (msg_type < MM_MSG_TYPE_ATTACH || msg_type > MM_MSG_TYPE_INFO) [[unlikely]] {
      destroy_ipc_message(msg);
      set_ipc_data(msg, 0, MM_CODE_E_ILL_ARGS);
      return;
    }

    table[msg_type](msg);
  }
} // namespace

[[noreturn]] void run(endpoint_cap_t ep_cap) {
  const size_t cap_space_size  = unwrap_sysret(sys_system_cap_size(CAP_CAP_SPACE));
  const size_t cap_space_align = unwrap_sysret(sys_system_cap_align(CAP_CAP_SPACE));

  message_t* msg = new_ipc_message(0x100);
  sysret_t   sysret;

  sysret.error = SYS_E_UNKNOWN;
  while (true) {
    if (unwrap_sysret(sys_task_cap_get_free_slot_count(__this_task_cap)) < 0x20) [[unlikely]] {
      mem_cap_t mem_cap = fetch_mem_cap(cap_space_size, cap_space_align);
      if (mem_cap == 0) [[unlikely]] {
        abort();
      }
      cap_space_cap_t cap_space_cap = unwrap_sysret(sys_mem_cap_create_cap_space_object(mem_cap));
      unwrap_sysret(sys_task_cap_insert_cap_space(__this_task_cap, cap_space_cap));
    }

    if (sysret_succeeded(sysret)) {
      sysret = sys_endpoint_cap_reply_and_receive(ep_cap, msg);
    } else {
      sysret = sys_endpoint_cap_receive(ep_cap, msg);
    }

    if (sysret_succeeded(sysret)) {
      proc_msg(msg);
    }
  }
}
