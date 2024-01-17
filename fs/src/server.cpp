#include <cerrno>
#include <crt/global.h>
#include <fs/ipc.h>
#include <fs/server.h>
#include <fs/vfs.h>
#include <libcaprese/syscall.h>
#include <memory>
#include <service/mm.h>
#include <string_view>

namespace {
  void mount(message_t* msg) {
    assert(get_ipc_data(msg, 0) == FS_MSG_TYPE_MOUNT);

    endpoint_cap_t ep_cap = move_ipc_cap(msg, 1);
    if (unwrap_sysret(sys_cap_type(ep_cap)) != CAP_ENDPOINT) [[unlikely]] {
      sys_cap_destroy(ep_cap);
      destroy_ipc_message(msg);
      set_ipc_data(msg, 0, FS_CODE_E_ILL_ARGS);
      return;
    }

    const char* c_path = reinterpret_cast<const char*>(get_ipc_data_ptr(msg, 2));

    if (c_path == nullptr) [[unlikely]] {
      destroy_ipc_message(msg);
      set_ipc_data(msg, 0, FS_CODE_E_ILL_ARGS);
      return;
    }

    id_cap_t fs_id;
    int      result = vfs_mount(ep_cap, c_path, fs_id);

    destroy_ipc_message(msg);
    set_ipc_data(msg, 0, result);

    if (result == FS_CODE_S_OK) {
      set_ipc_cap(msg, 1, unwrap_sysret(sys_id_cap_copy(fs_id)), false);
    }
  }

  void unmount(message_t* msg) {
    assert(get_ipc_data(msg, 0) == FS_MSG_TYPE_UNMOUNT);

    const char* c_path = reinterpret_cast<const char*>(get_ipc_data_ptr(msg, 1));

    if (c_path == nullptr) [[unlikely]] {
      destroy_ipc_message(msg);
      set_ipc_data(msg, 0, FS_CODE_E_ILL_ARGS);
      return;
    }

    int result = vfs_unmount(c_path);

    destroy_ipc_message(msg);
    set_ipc_data(msg, 0, result);
  }

  void mounted(message_t* msg) {
    assert(get_ipc_data(msg, 0) == FS_MSG_TYPE_MOUNTED);

    const char* c_path = reinterpret_cast<const char*>(get_ipc_data_ptr(msg, 1));

    if (c_path == nullptr) [[unlikely]] {
      destroy_ipc_message(msg);
      set_ipc_data(msg, 0, FS_CODE_E_ILL_ARGS);
      return;
    }

    bool mounted;
    int  result = vfs_mounted(c_path, &mounted);

    destroy_ipc_message(msg);
    set_ipc_data(msg, 0, result);

    if (result == FS_CODE_S_OK) {
      set_ipc_data(msg, 1, mounted);
    }
  }

  void info(message_t* msg) {
    assert(get_ipc_data(msg, 0) == FS_MSG_TYPE_INFO);

    const char* c_path = reinterpret_cast<const char*>(get_ipc_data_ptr(msg, 1));

    if (c_path == nullptr) [[unlikely]] {
      destroy_ipc_message(msg);
      set_ipc_data(msg, 0, FS_CODE_E_ILL_ARGS);
      return;
    }

    fs_file_info info;
    int          result = vfs_get_info(c_path, info);

    if (result != FS_CODE_S_OK) [[unlikely]] {
      destroy_ipc_message(msg);
      set_ipc_data(msg, 0, result);
      return;
    }

    destroy_ipc_message(msg);
    set_ipc_data(msg, 0, FS_CODE_S_OK);
    set_ipc_data_array(msg, 1, &info, sizeof(info));
  }

  void create(message_t* msg) {
    assert(get_ipc_data(msg, 0) == FS_MSG_TYPE_CREATE);

    int         type   = get_ipc_data(msg, 1);
    const char* c_path = reinterpret_cast<const char*>(get_ipc_data_ptr(msg, 2));

    if (c_path == nullptr) [[unlikely]] {
      destroy_ipc_message(msg);
      set_ipc_data(msg, 0, FS_CODE_E_ILL_ARGS);
      return;
    }

    int result = vfs_create(c_path, type);

    destroy_ipc_message(msg);
    set_ipc_data(msg, 0, result);
  }

  void remove(message_t* msg) {
    assert(get_ipc_data(msg, 0) == FS_MSG_TYPE_REMOVE);

    const char* c_path = reinterpret_cast<const char*>(get_ipc_data_ptr(msg, 1));

    if (c_path == nullptr) [[unlikely]] {
      destroy_ipc_message(msg);
      set_ipc_data(msg, 0, FS_CODE_E_ILL_ARGS);
      return;
    }

    int result = vfs_remove(c_path);

    destroy_ipc_message(msg);
    set_ipc_data(msg, 0, result);
  }

  void open(message_t* msg) {
    assert(get_ipc_data(msg, 0) == FS_MSG_TYPE_OPEN);

    const char* c_path = reinterpret_cast<const char*>(get_ipc_data_ptr(msg, 1));

    if (c_path == nullptr) [[unlikely]] {
      destroy_ipc_message(msg);
      set_ipc_data(msg, 0, FS_CODE_E_ILL_ARGS);
      return;
    }

    id_cap_t fd;
    int      result = vfs_open(c_path, fd);

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

    id_cap_t fd = get_ipc_cap(msg, 1);

    if (unwrap_sysret(sys_cap_type(fd)) != CAP_ID) [[unlikely]] {
      destroy_ipc_message(msg);
      set_ipc_data(msg, 0, FS_CODE_E_ILL_ARGS);
      return;
    }

    int result = vfs_close(fd);

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

    if (size > FS_READ_MAX_SIZE) [[unlikely]] {
      destroy_ipc_message(msg);
      set_ipc_data(msg, 0, FS_CODE_E_ILL_ARGS);
      return;
    }

    std::unique_ptr<char[]> buffer = std::make_unique<char[]>(size);
    std::streamsize         act_size;
    int                     result = vfs_read(fd, buffer.get(), size, act_size);

    if (result != FS_CODE_S_OK) [[unlikely]] {
      destroy_ipc_message(msg);
      set_ipc_data(msg, 0, result);
      return;
    }

    destroy_ipc_message(msg);
    set_ipc_data(msg, 0, FS_CODE_S_OK);
    set_ipc_data(msg, 1, act_size);
    set_ipc_data_array(msg, 2, buffer.get(), act_size);
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

    if (size > FS_WRITE_MAX_SIZE) [[unlikely]] {
      destroy_ipc_message(msg);
      set_ipc_data(msg, 0, FS_CODE_E_ILL_ARGS);
      return;
    }

    const char* data = reinterpret_cast<const char*>(get_ipc_data_ptr(msg, 3));

    if (data == nullptr) [[unlikely]] {
      destroy_ipc_message(msg);
      set_ipc_data(msg, 0, FS_CODE_E_ILL_ARGS);
      return;
    }

    std::streamsize act_size;
    int             result = vfs_write(fd, std::string_view(data, size), act_size);

    destroy_ipc_message(msg);
    set_ipc_data(msg, 0, result);
    set_ipc_data(msg, 1, act_size);
  }

  void seek(message_t* msg) {
    assert(get_ipc_data(msg, 0) == FS_MSG_TYPE_SEEK);

    id_cap_t fd = get_ipc_cap(msg, 1);

    if (unwrap_sysret(sys_cap_type(fd)) != CAP_ID) [[unlikely]] {
      destroy_ipc_message(msg);
      set_ipc_data(msg, 0, FS_CODE_E_ILL_ARGS);
      return;
    }

    std::streamoff offset = get_ipc_data(msg, 2);
    int            whence = get_ipc_data(msg, 3);

    int result = vfs_seek(fd, offset, whence);

    destroy_ipc_message(msg);
    set_ipc_data(msg, 0, result);
  }

  void tell(message_t* msg) {
    assert(get_ipc_data(msg, 0) == FS_MSG_TYPE_TELL);

    id_cap_t fd = get_ipc_cap(msg, 1);

    if (unwrap_sysret(sys_cap_type(fd)) != CAP_ID) [[unlikely]] {
      destroy_ipc_message(msg);
      set_ipc_data(msg, 0, FS_CODE_E_ILL_ARGS);
      return;
    }

    std::streampos pos;
    int            result = vfs_tell(fd, pos);

    if (result != FS_CODE_S_OK) [[unlikely]] {
      destroy_ipc_message(msg);
      set_ipc_data(msg, 0, result);
      return;
    }

    destroy_ipc_message(msg);
    set_ipc_data(msg, 0, FS_CODE_S_OK);
    set_ipc_data(msg, 1, pos);
  }

  // clang-format off

  constexpr void (*const table[])(message_t*) = {
    [0]                   = nullptr,
    [FS_MSG_TYPE_MOUNT]   = mount,
    [FS_MSG_TYPE_UNMOUNT] = unmount,
    [FS_MSG_TYPE_MOUNTED] = mounted,
    [FS_MSG_TYPE_INFO]    = info,
    [FS_MSG_TYPE_CREATE]  = create,
    [FS_MSG_TYPE_REMOVE]  = remove,
    [FS_MSG_TYPE_OPEN]    = open,
    [FS_MSG_TYPE_CLOSE]   = close,
    [FS_MSG_TYPE_READ]    = read,
    [FS_MSG_TYPE_WRITE]   = write,
    [FS_MSG_TYPE_SEEK]    = seek,
    [FS_MSG_TYPE_TELL]    = tell,
  };

  // clang-format on

  void proc_msg(message_t* msg) {
    assert(msg != nullptr);

    uintptr_t msg_type = get_ipc_data(msg, 0);

    if (msg_type >= std::size(table)) [[unlikely]] {
      destroy_ipc_message(msg);
      set_ipc_data(msg, 0, FS_CODE_E_ILL_ARGS);
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
