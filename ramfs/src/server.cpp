#include <crt/global.h>
#include <cstdio>
#include <fs/ipc.h>
#include <libcaprese/ipc.h>
#include <libcaprese/syscall.h>
#include <memory>
#include <ramfs/directory.h>
#include <ramfs/file.h>
#include <ramfs/fs.h>
#include <ramfs/server.h>
#include <service/mm.h>

id_cap_t ramfs_id_cap;

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
    int          result = ramfs_get_info(c_path, info);

    destroy_ipc_message(msg);
    set_ipc_data(msg, 0, result);

    if (result != 0) [[unlikely]] {
      return;
    }

    set_ipc_data_array(msg, 1, &info, sizeof(info));
  }

  void create(message_t* msg) {
    assert(get_ipc_data(msg, 0) == FS_MSG_TYPE_CREATE);

    int         type   = get_ipc_data(msg, 2);
    const char* c_path = reinterpret_cast<const char*>(get_ipc_data_ptr(msg, 3));

    if (c_path == nullptr) [[unlikely]] {
      destroy_ipc_message(msg);
      set_ipc_data(msg, 0, FS_CODE_E_ILL_ARGS);
      return;
    }

    int result = ramfs_create(c_path, type);

    destroy_ipc_message(msg);
    set_ipc_data(msg, 0, result);
  }

  void remove(message_t* msg) {
    assert(get_ipc_data(msg, 0) == FS_MSG_TYPE_REMOVE);

    const char* c_path = reinterpret_cast<const char*>(get_ipc_data_ptr(msg, 2));
    if (c_path == nullptr) [[unlikely]] {
      destroy_ipc_message(msg);
      set_ipc_data(msg, 0, FS_CODE_E_ILL_ARGS);
      return;
    }

    int result = ramfs_remove(c_path);

    destroy_ipc_message(msg);
    set_ipc_data(msg, 0, result);
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
    int      result = ramfs_open(c_path, fd);

    destroy_ipc_message(msg);
    set_ipc_data(msg, 0, result);

    if (result != 0) [[unlikely]] {
      return;
    }

    set_ipc_cap(msg, 1, unwrap_sysret(sys_id_cap_copy(fd)), false);
  }

  void close(message_t* msg) {
    assert(get_ipc_data(msg, 0) == FS_MSG_TYPE_CLOSE);

    id_cap_t id = get_ipc_cap(msg, 1);
    if (unwrap_sysret(sys_cap_type(id)) != CAP_ID) [[unlikely]] {
      destroy_ipc_message(msg);
      set_ipc_data(msg, 0, FS_CODE_E_ILL_ARGS);
      return;
    }

    if (unwrap_sysret(sys_id_cap_compare(id, ramfs_id_cap)) != 0) [[unlikely]] {
      destroy_ipc_message(msg);
      set_ipc_data(msg, 0, FS_CODE_E_ILL_ARGS);
      return;
    }

    int result = ramfs_close(id);

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
    int                     result = ramfs_read(fd, buffer.get(), len, act_size);

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
    int             result = ramfs_write(fd, std::string_view(data, len), act_size);

    destroy_ipc_message(msg);
    set_ipc_data(msg, 0, result);

    if (result != FS_CODE_S_OK && result != FS_CODE_E_EOF) [[unlikely]] {
      return;
    }

    set_ipc_data(msg, 1, act_size);
  }

  void seek(message_t* msg) {
    assert(get_ipc_data(msg, 0) == FS_MSG_TYPE_SEEK);

    id_cap_t fd = get_ipc_cap(msg, 2);
    if (unwrap_sysret(sys_cap_type(fd)) != CAP_ID) [[unlikely]] {
      destroy_ipc_message(msg);
      set_ipc_data(msg, 0, FS_CODE_E_ILL_ARGS);
      return;
    }

    std::streamoff offset = get_ipc_data(msg, 3);
    int            whence = get_ipc_data(msg, 4);

    std::ios_base::seekdir dir;
    switch (whence) {
      case SEEK_SET:
        dir = std::ios_base::beg;
        break;
      case SEEK_CUR:
        dir = std::ios_base::cur;
        break;
      case SEEK_END:
        dir = std::ios_base::end;
        break;
      default:
        destroy_ipc_message(msg);
        set_ipc_data(msg, 0, FS_CODE_E_ILL_ARGS);
        return;
    }

    int result = ramfs_seek(fd, offset, dir);

    destroy_ipc_message(msg);
    set_ipc_data(msg, 0, result);
  }

  void tell(message_t* msg) {
    assert(get_ipc_data(msg, 0) == FS_MSG_TYPE_TELL);

    id_cap_t fd = get_ipc_cap(msg, 2);
    if (unwrap_sysret(sys_cap_type(fd)) != CAP_ID) [[unlikely]] {
      destroy_ipc_message(msg);
      set_ipc_data(msg, 0, FS_CODE_E_ILL_ARGS);
      return;
    }

    std::streampos pos;
    int            result = ramfs_tell(fd, pos);

    destroy_ipc_message(msg);
    set_ipc_data(msg, 0, result);

    if (result != FS_CODE_S_OK) [[unlikely]] {
      return;
    }

    set_ipc_data(msg, 1, pos);
  }

  // clang-format off

  constexpr void (*const table[])(message_t*) = {
    [0]                   = nullptr,
    [FS_MSG_TYPE_MOUNT]   = nullptr,
    [FS_MSG_TYPE_UNMOUNT] = nullptr,
    [FS_MSG_TYPE_MOUNTED] = nullptr,
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
    id_cap_t id = get_ipc_cap(msg, 1);
    if (unwrap_sysret(sys_cap_type(id)) != CAP_ID) [[unlikely]] {
      destroy_ipc_message(msg);
      set_ipc_data(msg, 0, FS_CODE_E_ILL_ARGS);
      return;
    }

    if (unwrap_sysret(sys_id_cap_compare(id, ramfs_id_cap)) != 0) [[unlikely]] {
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
