#include <cerrno>
#include <crt/global.h>
#include <cstring>
#include <fs/fs_table.h>
#include <fs/ipc.h>
#include <fs/server.h>
#include <libcaprese/syscall.h>
#include <memory>
#include <service/mm.h>
#include <string>

namespace {
  constexpr size_t max_size = 0x1000 - sizeof(uintptr_t) * 4;

  void mount(message_t* msg) {
    assert(get_ipc_data(msg, 0) == FS_MSG_TYPE_MOUNT);

    endpoint_cap_t ep_cap = move_ipc_cap(msg, 1);
    if (unwrap_sysret(sys_cap_type(ep_cap)) != CAP_ENDPOINT) [[unlikely]] {
      sys_cap_destroy(ep_cap);
      destroy_ipc_message(msg);
      set_ipc_data(msg, 0, FS_CODE_E_FAILURE);
      return;
    }

    const char* c_path = reinterpret_cast<const char*>(get_ipc_data_ptr(msg, 2));
    std::string path(c_path, strnlen(c_path, msg->header.payload_length - sizeof(uintptr_t) * 2));

    if (path.empty()) [[unlikely]] {
      sys_cap_destroy(ep_cap);
      destroy_ipc_message(msg);
      set_ipc_data(msg, 0, FS_CODE_E_ILL_ARGS);
      return;
    }

    if (path.back() != '/') {
      path.push_back('/');
    }

    id_cap_t id_cap = unwrap_sysret(sys_id_cap_create());

    int result = mount_fs(id_cap, path, ep_cap);
    if (result != FS_CODE_S_OK) [[unlikely]] {
      sys_cap_destroy(ep_cap);
      sys_cap_destroy(id_cap);
      destroy_ipc_message(msg);
      set_ipc_data(msg, 0, result);
      return;
    }

    destroy_ipc_message(msg);
    set_ipc_data(msg, 0, FS_CODE_S_OK);
    set_ipc_cap(msg, 1, unwrap_sysret(sys_id_cap_copy(id_cap)), false);
  }

  void unmount(message_t* msg) {
    assert(get_ipc_data(msg, 0) == FS_MSG_TYPE_UNMOUNT);

    id_cap_t id_cap = get_ipc_cap(msg, 1);

    int result = unmount_fs(id_cap);

    destroy_ipc_message(msg);
    set_ipc_data(msg, 0, result);
  }

  void mounted(message_t* msg) {
    assert(get_ipc_data(msg, 0) == FS_MSG_TYPE_MOUNTED);

    const char* c_path = reinterpret_cast<const char*>(get_ipc_data_ptr(msg, 1));
    std::string path(c_path, strnlen(c_path, msg->header.payload_length - sizeof(uintptr_t) * 1));

    if (path.empty()) [[unlikely]] {
      destroy_ipc_message(msg);
      set_ipc_data(msg, 0, FS_CODE_E_ILL_ARGS);
      return;
    }

    if (path.back() != '/') {
      path.push_back('/');
    }

    bool result = mounted_fs(path);

    destroy_ipc_message(msg);
    set_ipc_data(msg, 0, FS_CODE_S_OK);
    set_ipc_data(msg, 1, result);
  }

  void open(message_t* msg) {
    assert(get_ipc_data(msg, 0) == FS_MSG_TYPE_OPEN);

    const char* c_path = reinterpret_cast<const char*>(get_ipc_data_ptr(msg, 1));
    std::string path(c_path, strnlen(c_path, msg->header.payload_length - sizeof(uintptr_t) * 1));

    if (path.empty()) [[unlikely]] {
      destroy_ipc_message(msg);
      set_ipc_data(msg, 0, FS_CODE_E_ILL_ARGS);
      return;
    }

    id_cap_t fd = open_fs(path);

    if (fd == 0) [[unlikely]] {
      destroy_ipc_message(msg);
      set_ipc_data(msg, 0, errno);
      return;
    }

    destroy_ipc_message(msg);
    set_ipc_data(msg, 0, FS_CODE_S_OK);
    set_ipc_cap(msg, 1, unwrap_sysret(sys_id_cap_copy(fd)), false);
  }

  void close(message_t* msg) {
    assert(get_ipc_data(msg, 0) == FS_MSG_TYPE_CLOSE);

    id_cap_t fd = get_ipc_cap(msg, 1);
    if (unwrap_sysret(sys_cap_type(fd)) != CAP_ID) [[unlikely]] {
      destroy_ipc_message(msg);
      set_ipc_data(msg, 0, FS_CODE_E_ILL_ARGS);
      return;
    }

    int result = close_fs(fd);

    destroy_ipc_message(msg);
    set_ipc_data(msg, 0, result);
  }

  void read(message_t* msg) {
    assert(get_ipc_data(msg, 0) == FS_MSG_TYPE_READ);

    id_cap_t fd = get_ipc_cap(msg, 1);
    if (unwrap_sysret(sys_cap_type(fd)) != CAP_ID) [[unlikely]] {
      destroy_ipc_message(msg);
      set_ipc_data(msg, 0, FS_CODE_E_ILL_ARGS);
      return;
    }

    size_t size = get_ipc_data(msg, 2);
    if (size > max_size) [[unlikely]] {
      destroy_ipc_message(msg);
      set_ipc_data(msg, 0, FS_CODE_E_ILL_ARGS);
      return;
    }

    std::unique_ptr<char[]> buf = std::make_unique<char[]>(size);
    size_t                  act_size;
    int                     result = read_fs(fd, buf.get(), size, &act_size);

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

    id_cap_t fd = get_ipc_cap(msg, 1);
    if (unwrap_sysret(sys_cap_type(fd)) != CAP_ID) [[unlikely]] {
      destroy_ipc_message(msg);
      set_ipc_data(msg, 0, FS_CODE_E_ILL_ARGS);
      return;
    }

    size_t size = get_ipc_data(msg, 2);
    if (size > max_size) [[unlikely]] {
      destroy_ipc_message(msg);
      set_ipc_data(msg, 0, FS_CODE_E_ILL_ARGS);
      return;
    }

    size_t act_size;
    int    result = write_fs(fd, get_ipc_data_ptr(msg, 3), size, &act_size);

    destroy_ipc_message(msg);
    set_ipc_data(msg, 0, result);

    if (result != FS_CODE_S_OK && result != FS_CODE_E_EOF) [[unlikely]] {
      return;
    }

    set_ipc_data(msg, 1, act_size);
  }

  void seek(message_t* msg) {
    assert(get_ipc_data(msg, 0) == FS_MSG_TYPE_SEEK);
  }

  void tell(message_t* msg) {
    assert(get_ipc_data(msg, 0) == FS_MSG_TYPE_TELL);
  }

  void mkdir(message_t* msg) {
    assert(get_ipc_data(msg, 0) == FS_MSG_TYPE_MKDIR);
  }

  void rmdir(message_t* msg) {
    assert(get_ipc_data(msg, 0) == FS_MSG_TYPE_RMDIR);
  }

  // clang-format off

  constexpr void (*const table[])(message_t*) = {
    [0]                   = nullptr,
    [FS_MSG_TYPE_MOUNT]   = mount,
    [FS_MSG_TYPE_UNMOUNT] = unmount,
    [FS_MSG_TYPE_MOUNTED] = mounted,
    [FS_MSG_TYPE_OPEN]    = open,
    [FS_MSG_TYPE_CLOSE]   = close,
    [FS_MSG_TYPE_READ]    = read,
    [FS_MSG_TYPE_WRITE]   = write,
    [FS_MSG_TYPE_SEEK]    = seek,
    [FS_MSG_TYPE_TELL]    = tell,
    [FS_MSG_TYPE_MKDIR]   = mkdir,
    [FS_MSG_TYPE_RMDIR]   = rmdir,
  };

  // clang-format on

  void proc_msg(message_t* msg) {
    assert(msg != nullptr);

    uintptr_t msg_type = get_ipc_data(msg, 0);

    if (msg_type < FS_MSG_TYPE_MOUNT || msg_type > FS_MSG_TYPE_RMDIR) [[unlikely]] {
      destroy_ipc_message(msg);
      set_ipc_data(msg, 0, FS_CODE_E_ILL_ARGS);
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
