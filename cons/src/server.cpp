#include <cons/file.h>
#include <cons/server.h>
#include <crt/global.h>
#include <fs/ipc.h>
#include <libcaprese/cxx/id_map.h>
#include <libcaprese/ipc.h>
#include <libcaprese/syscall.h>
#include <memory>
#include <service/mm.h>
#include <string>

id_cap_t cons_id_cap;

namespace {
  void open(message_t* msg) {
    assert(get_ipc_data(msg, 0) == FS_MSG_TYPE_OPEN);

    const char* c_path = reinterpret_cast<const char*>(get_ipc_data_ptr(msg, 2));

    id_cap_t fd;
    int      result = cons_open(c_path, &fd);

    if (result != FS_CODE_S_OK) [[unlikely]] {
      destroy_ipc_message(msg);
      set_ipc_data(msg, 0, result);
      return;
    }

    destroy_ipc_message(msg);
    set_ipc_data(msg, 0, FS_CODE_S_OK);
    set_ipc_cap(msg, 1, unwrap_sysret(sys_id_cap_copy(fd)), false);
  }

  void close(message_t* msg) {
    assert(get_ipc_data(msg, 0) == FS_MSG_TYPE_CLOSE);

    id_cap_t fd = get_ipc_cap(msg, 2);
    if (unwrap_sysret(sys_cap_type(fd)) != CAP_ID) [[unlikely]] {
      destroy_ipc_message(msg);
      set_ipc_data(msg, 0, FS_CODE_E_ILL_ARGS);
      return;
    }

    int result = cons_close(fd);

    destroy_ipc_message(msg);
    set_ipc_data(msg, 0, result);
  }

  void read(message_t* msg) {
    assert(get_ipc_data(msg, 0) == FS_MSG_TYPE_READ);

    id_cap_t fd = get_ipc_cap(msg, 2);
    if (unwrap_sysret(sys_cap_type(fd)) != CAP_ID) [[unlikely]] {
      destroy_ipc_message(msg);
      set_ipc_data(msg, 0, FS_CODE_E_ILL_ARGS);
      return;
    }

    std::streamsize         size = std::min<std::streamsize>(get_ipc_data(msg, 3), msg->header.payload_capacity - sizeof(uintptr_t) * 2);
    std::unique_ptr<char[]> buf  = std::make_unique<char[]>(size);
    size_t                  act_size;
    int                     result = cons_read(fd, buf.get(), size, &act_size);

    destroy_ipc_message(msg);
    set_ipc_data(msg, 0, result);

    if (result != FS_CODE_S_OK && result != FS_CODE_E_EOF) [[unlikely]] {
      return;
    }

    set_ipc_data(msg, 1, act_size);
    set_ipc_data_array(msg, 2, buf.get(), act_size);
  }

  void write(message_t* msg) {
    assert(get_ipc_data(msg, 0) == FS_MSG_TYPE_WRITE);

    id_cap_t fd = get_ipc_cap(msg, 2);

    if (unwrap_sysret(sys_cap_type(fd)) != CAP_ID) [[unlikely]] {
      destroy_ipc_message(msg);
      set_ipc_data(msg, 0, FS_CODE_E_ILL_ARGS);
      return;
    }

    std::streamsize size = std::min<std::streamsize>(get_ipc_data(msg, 3), msg->header.payload_length - sizeof(uintptr_t) * 2);
    size_t          act_size;
    int             result = cons_write(fd, get_ipc_data_ptr(msg, 4), size, &act_size);

    destroy_ipc_message(msg);
    set_ipc_data(msg, 0, result);

    if (result != FS_CODE_S_OK && result != FS_CODE_E_EOF) [[unlikely]] {
      return;
    }

    set_ipc_data(msg, 1, act_size);
  }

  constexpr void (*const table[])(message_t*) = {
    [0]                   = NULL,
    [FS_MSG_TYPE_MOUNT]   = NULL,
    [FS_MSG_TYPE_UNMOUNT] = NULL,
    [FS_MSG_TYPE_MOUNTED] = NULL,
    [FS_MSG_TYPE_OPEN]    = open,
    [FS_MSG_TYPE_CLOSE]   = close,
    [FS_MSG_TYPE_READ]    = read,
    [FS_MSG_TYPE_WRITE]   = write,
    [FS_MSG_TYPE_SEEK]    = NULL,
    [FS_MSG_TYPE_TELL]    = NULL,
    [FS_MSG_TYPE_MKDIR]   = NULL,
    [FS_MSG_TYPE_RMDIR]   = NULL,
  };

  void proc_msg(message_t* msg) {
    id_cap_t id = get_ipc_cap(msg, 1);
    if (unwrap_sysret(sys_cap_type(id)) != CAP_ID) [[unlikely]] {
      destroy_ipc_message(msg);
      set_ipc_data(msg, 0, FS_CODE_E_ILL_ARGS);
      return;
    }

    if (unwrap_sysret(sys_id_cap_compare(id, cons_id_cap)) != 0) [[unlikely]] {
      destroy_ipc_message(msg);
      set_ipc_data(msg, 0, FS_CODE_E_ILL_ARGS);
      return;
    }

    uintptr_t msg_type = get_ipc_data(msg, 0);

    if (msg_type < FS_MSG_TYPE_OPEN || msg_type > FS_MSG_TYPE_RMDIR) [[unlikely]] {
      destroy_ipc_message(msg);
      set_ipc_data(msg, 0, FS_CODE_E_UNSUPPORTED);
      return;
    }

    if (table[msg_type] == NULL) [[unlikely]] {
      destroy_ipc_message(msg);
      set_ipc_data(msg, 0, FS_CODE_E_UNSUPPORTED);
      return;
    }

    table[msg_type](msg);
  }
} // namespace

[[noreturn]] void run() {
  message_t* msg = new_ipc_message(0x1000);
  sysret_t   sysret;

  sysret.error = SYS_E_UNKNOWN;
  while (true) {
    if (unwrap_sysret(sys_task_cap_get_free_slot_count(__this_task_cap)) < 0x10) [[unlikely]] {
      cap_space_cap_t cap_space_cap = mm_fetch_and_create_cap_space_object();
      unwrap_sysret(sys_task_cap_insert_cap_space(__this_task_cap, cap_space_cap));
    }

    if (sysret_succeeded(sysret)) {
      sysret = sys_endpoint_cap_reply_and_receive(__this_ep_cap, msg);
    } else {
      sysret = sys_endpoint_cap_receive(__this_ep_cap, msg);
    }

    if (sysret_succeeded(sysret)) {
      proc_msg(msg);
    }
  }
}
