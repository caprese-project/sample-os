#include <cons/file.h>
#include <cons/fs.h>
#include <cons/server.h>
#include <crt/global.h>
#include <fs/ipc.h>
#include <libcaprese/ipc.h>
#include <libcaprese/syscall.h>
#include <memory>
#include <service/mm.h>

id_cap_t cons_id_cap;

namespace {
  void info(message_t* msg) {
    assert(get_ipc_data(msg, 0) == FS_MSG_TYPE_INFO);

    const char* c_path = reinterpret_cast<const char*>(get_ipc_data_ptr(msg, 2));
    if (c_path == nullptr) [[unlikely]] {
      destroy_ipc_message(msg);
      set_ipc_data(msg, 0, FS_CODE_E_ILL_ARGS);
      return;
    }

    fs_file_info info;
    int          result = cons_get_info(c_path, info);

    destroy_ipc_message(msg);
    set_ipc_data(msg, 0, result);

    if (result != 0) [[unlikely]] {
      return;
    }

    set_ipc_data_array(msg, 1, &info, sizeof(info));
  }

  void open(message_t* msg) {
    assert(get_ipc_data(msg, 0) == FS_MSG_TYPE_OPEN);

    const char* c_path = reinterpret_cast<const char*>(get_ipc_data_ptr(msg, 2));
    if (c_path == nullptr) [[unlikely]] {
      destroy_ipc_message(msg);
      set_ipc_data(msg, 0, FS_CODE_E_ILL_ARGS);
      return;
    }

    id_cap_t fd;
    int      result = cons_open(c_path, fd);

    destroy_ipc_message(msg);
    set_ipc_data(msg, 0, result);

    if (result != 0) [[unlikely]] {
      return;
    }

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

    size_t len = get_ipc_data(msg, 3);
    if (len > FS_READ_MAX_SIZE) [[unlikely]] {
      destroy_ipc_message(msg);
      set_ipc_data(msg, 0, FS_CODE_E_ILL_ARGS);
      return;
    }

    std::unique_ptr<char[]> buffer = std::make_unique<char[]>(len);
    std::streamsize         act_size;
    int                     result = cons_read(fd, buffer.get(), len, act_size);

    destroy_ipc_message(msg);
    set_ipc_data(msg, 0, result);

    if (result != FS_CODE_S_OK && result != FS_CODE_E_EOF) [[unlikely]] {
      return;
    }

    set_ipc_data(msg, 1, act_size);
    set_ipc_data_array(msg, 2, buffer.get(), act_size);
  }

  void write(message_t* msg) {
    assert(get_ipc_data(msg, 0) == FS_MSG_TYPE_WRITE);

    id_cap_t fd = get_ipc_cap(msg, 2);
    if (unwrap_sysret(sys_cap_type(fd)) != CAP_ID) [[unlikely]] {
      destroy_ipc_message(msg);
      set_ipc_data(msg, 0, FS_CODE_E_ILL_ARGS);
      return;
    }

    size_t len = get_ipc_data(msg, 3);
    if (len > FS_WRITE_MAX_SIZE) [[unlikely]] {
      destroy_ipc_message(msg);
      set_ipc_data(msg, 0, FS_CODE_E_ILL_ARGS);
      return;
    }

    const char* data = reinterpret_cast<const char*>(get_ipc_data_ptr(msg, 4));
    if (data == nullptr) [[unlikely]] {
      destroy_ipc_message(msg);
      set_ipc_data(msg, 0, FS_CODE_E_ILL_ARGS);
      return;
    }

    std::streamsize act_size;
    int             result = cons_write(fd, std::string_view(data, len), act_size);

    destroy_ipc_message(msg);
    set_ipc_data(msg, 0, result);

    if (result != FS_CODE_S_OK && result != FS_CODE_E_EOF) [[unlikely]] {
      return;
    }

    set_ipc_data(msg, 1, act_size);
  }

  constexpr void (*const table[])(message_t*) = {
    [0]                   = nullptr,
    [FS_MSG_TYPE_MOUNT]   = nullptr,
    [FS_MSG_TYPE_UNMOUNT] = nullptr,
    [FS_MSG_TYPE_MOUNTED] = nullptr,
    [FS_MSG_TYPE_INFO]    = info,
    [FS_MSG_TYPE_CREATE]  = nullptr,
    [FS_MSG_TYPE_REMOVE]  = nullptr,
    [FS_MSG_TYPE_OPEN]    = open,
    [FS_MSG_TYPE_CLOSE]   = close,
    [FS_MSG_TYPE_READ]    = read,
    [FS_MSG_TYPE_WRITE]   = write,
    [FS_MSG_TYPE_SEEK]    = nullptr,
    [FS_MSG_TYPE_TELL]    = nullptr,
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

    if (msg_type >= std::size(table)) [[unlikely]] {
      destroy_ipc_message(msg);
      set_ipc_data(msg, 0, FS_CODE_E_UNSUPPORTED);
      return;
    }

    if (table[msg_type] == nullptr) [[unlikely]] {
      destroy_ipc_message(msg);
      set_ipc_data(msg, 0, FS_CODE_E_UNSUPPORTED);
      return;
    }

    table[msg_type](msg);
  }
} // namespace

[[noreturn]] void run() {
  message_t* msg = new_ipc_message(FS_MSG_CAPACITY);
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
