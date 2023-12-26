#include <cerrno>
#include <crt/global.h>
#include <cstring>
#include <fs/fs_table.h>
#include <fs/ipc.h>
#include <fs/server.h>
#include <libcaprese/syscall.h>
#include <service/mm.h>
#include <string>

namespace {
  constexpr size_t max_size = ([]() {
    message_buffer_t msg_buf;
    return (std::size(msg_buf.data) - 2) * sizeof(uintptr_t);
  })();

  void mount(message_buffer_t* msg_buf) {
    assert(msg_buf->data[msg_buf->cap_part_length] == FS_MSG_TYPE_MOUNT);

    if (msg_buf->cap_part_length != 1 || msg_buf->data_part_length < 2) [[unlikely]] {
      msg_buf_destroy(msg_buf);
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = FS_CODE_E_ILL_ARGS;
      return;
    }

    endpoint_cap_t ep_cap = msg_buf->data[0];
    if (unwrap_sysret(sys_cap_type(ep_cap)) != CAP_ENDPOINT) [[unlikely]] {
      msg_buf_destroy(msg_buf);
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = FS_CODE_E_ILL_ARGS;
      return;
    }

    char*       c_path = reinterpret_cast<char*>(&msg_buf->data[msg_buf->cap_part_length + 1]);
    std::string path(c_path, strnlen(c_path, sizeof(uintptr_t) * (msg_buf->data_part_length - 1)));

    if (path.empty()) [[unlikely]] {
      msg_buf_destroy(msg_buf);
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = FS_CODE_E_ILL_ARGS;
      return;
    }

    if (path.back() != '/') {
      path.push_back('/');
    }

    id_cap_t id_cap = unwrap_sysret(sys_id_cap_create());

    int result = mount_fs(id_cap, path, ep_cap);
    if (result != FS_CODE_S_OK) [[unlikely]] {
      sys_cap_destroy(id_cap);

      msg_buf->cap_part_length  = 0;
      msg_buf->data_part_length = 1;
      msg_buf->data[0]          = result;
    } else {
      msg_buf->cap_part_length  = 1;
      msg_buf->data[0]          = unwrap_sysret(sys_id_cap_copy(id_cap));
      msg_buf->data_part_length = 1;
      msg_buf->data[1]          = FS_CODE_S_OK;
    }
  }

  void unmount(message_buffer_t* msg_buf) {
    assert(msg_buf->data[msg_buf->cap_part_length] == FS_MSG_TYPE_UNMOUNT);

    if (msg_buf->cap_part_length != 1) [[unlikely]] {
      msg_buf_destroy(msg_buf);
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = FS_CODE_E_ILL_ARGS;
      return;
    }

    id_cap_t id_cap = msg_buf->data[0];

    int result = unmount_fs(id_cap);

    sys_cap_destroy(id_cap);

    if (result != FS_CODE_S_OK) [[unlikely]] {
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = result;
    } else {
      sys_cap_destroy(id_cap);
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = FS_CODE_S_OK;
    }
  }

  void mounted(message_buffer_t* msg_buf) {
    assert(msg_buf->data[msg_buf->cap_part_length] == FS_MSG_TYPE_MOUNTED);

    if (msg_buf->cap_part_length != 0 || msg_buf->data_part_length < 2) [[unlikely]] {
      msg_buf_destroy(msg_buf);
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = FS_CODE_E_ILL_ARGS;
      return;
    }

    char*       c_path = reinterpret_cast<char*>(&msg_buf->data[1]);
    std::string path(c_path, strnlen(c_path, sizeof(uintptr_t) * (msg_buf->data_part_length - 1)));

    if (path.empty()) [[unlikely]] {
      msg_buf_destroy(msg_buf);
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = FS_CODE_E_ILL_ARGS;
      return;
    }

    if (path.back() != '/') {
      path.push_back('/');
    }

    bool result = mounted_fs(path);

    msg_buf->cap_part_length  = 0;
    msg_buf->data_part_length = 2;
    msg_buf->data[0]          = FS_CODE_S_OK;
    msg_buf->data[1]          = result;
  }

  void open(message_buffer_t* msg_buf) {
    assert(msg_buf->data[msg_buf->cap_part_length] == FS_MSG_TYPE_OPEN);

    if (msg_buf->cap_part_length != 0 || msg_buf->data_part_length < 2) [[unlikely]] {
      msg_buf_destroy(msg_buf);
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = FS_CODE_E_ILL_ARGS;
      return;
    }

    char*       c_path = reinterpret_cast<char*>(&msg_buf->data[1]);
    std::string path(c_path, strnlen(c_path, sizeof(uintptr_t) * (msg_buf->data_part_length - 1)));

    if (path.empty()) [[unlikely]] {
      msg_buf_destroy(msg_buf);
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = FS_CODE_E_ILL_ARGS;
      return;
    }

    id_cap_t fd_cap = open_fs(path);

    if (fd_cap == 0) [[unlikely]] {
      msg_buf_destroy(msg_buf);
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = errno;
    } else {
      msg_buf->cap_part_length  = 1;
      msg_buf->data[0]          = unwrap_sysret(sys_id_cap_copy(fd_cap));
      msg_buf->data_part_length = 1;
      msg_buf->data[1]          = FS_CODE_S_OK;
    }
  }

  void close(message_buffer_t* msg_buf) {
    assert(msg_buf->data[msg_buf->cap_part_length] == FS_MSG_TYPE_CLOSE);

    if (msg_buf->cap_part_length != 1) [[unlikely]] {
      msg_buf_destroy(msg_buf);
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = FS_CODE_E_ILL_ARGS;
      return;
    }

    id_cap_t fd_cap = msg_buf->data[0];
    if (unwrap_sysret(sys_cap_type(fd_cap)) != CAP_ID) [[unlikely]] {
      msg_buf_destroy(msg_buf);
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = FS_CODE_E_ILL_ARGS;
      return;
    }

    int result = close_fs(fd_cap);

    sys_cap_destroy(fd_cap);

    msg_buf->cap_part_length = 0;
    if (result != FS_CODE_S_OK) [[unlikely]] {
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = result;
    } else {
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = FS_CODE_S_OK;
    }
  }

  void read(message_buffer_t* msg_buf) {
    assert(msg_buf->data[msg_buf->cap_part_length] == FS_MSG_TYPE_READ);

    if (msg_buf->cap_part_length != 1 || msg_buf->data_part_length < 2) [[unlikely]] {
      msg_buf_destroy(msg_buf);
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = FS_CODE_E_ILL_ARGS;
      return;
    }

    id_cap_t fd_cap = msg_buf->data[0];
    if (unwrap_sysret(sys_cap_type(fd_cap)) != CAP_ID) [[unlikely]] {
      msg_buf_destroy(msg_buf);
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = FS_CODE_E_ILL_ARGS;
      return;
    }

    size_t size = msg_buf->data[msg_buf->cap_part_length + 1];
    if (size > max_size) [[unlikely]] {
      msg_buf_destroy(msg_buf);
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = FS_CODE_E_ILL_ARGS;
      return;
    }

    int result = read_fs(fd_cap, &msg_buf->data[2], size, &msg_buf->data[1]);

    sys_cap_destroy(fd_cap);
    msg_buf->cap_part_length = 0;
    if (result != FS_CODE_S_OK && result != FS_CODE_E_EOF) [[unlikely]] {
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = result;
    } else {
      msg_buf->data_part_length = 2 + (msg_buf->data[1] + sizeof(uintptr_t) - 1) / sizeof(uintptr_t);
      msg_buf->data[0]          = result;
    }
  }

  void write(message_buffer_t* msg_buf) {
    assert(msg_buf->data[msg_buf->cap_part_length] == FS_MSG_TYPE_WRITE);

    if (msg_buf->cap_part_length != 1 || msg_buf->data_part_length < 2) [[unlikely]] {
      msg_buf_destroy(msg_buf);
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = FS_CODE_E_ILL_ARGS;
      return;
    }

    id_cap_t fd_cap = msg_buf->data[0];
    if (unwrap_sysret(sys_cap_type(fd_cap)) != CAP_ID) [[unlikely]] {
      msg_buf_destroy(msg_buf);
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = FS_CODE_E_ILL_ARGS;
      return;
    }

    size_t size = msg_buf->data[msg_buf->cap_part_length + 1];
    if (size > max_size) [[unlikely]] {
      msg_buf_destroy(msg_buf);
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = FS_CODE_E_ILL_ARGS;
      return;
    }

    int result = write_fs(fd_cap, &msg_buf->data[msg_buf->cap_part_length + 2], size, &msg_buf->data[1]);

    sys_cap_destroy(fd_cap);
    msg_buf->cap_part_length = 0;
    if (result != FS_CODE_S_OK && result != FS_CODE_E_EOF) [[unlikely]] {
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = result;
    } else {
      msg_buf->data_part_length = 2;
      msg_buf->data[0]          = result;
    }
  }

  void seek(message_buffer_t* msg_buf) {
    assert(msg_buf->data[msg_buf->cap_part_length] == FS_MSG_TYPE_SEEK);
  }

  void tell(message_buffer_t* msg_buf) {
    assert(msg_buf->data[msg_buf->cap_part_length] == FS_MSG_TYPE_TELL);
  }

  void mkdir(message_buffer_t* msg_buf) {
    assert(msg_buf->data[msg_buf->cap_part_length] == FS_MSG_TYPE_MKDIR);
  }

  void rmdir(message_buffer_t* msg_buf) {
    assert(msg_buf->data[msg_buf->cap_part_length] == FS_MSG_TYPE_RMDIR);
  }

  // clang-format off

  void (*const table[])(message_buffer_t*) = {
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

  void proc_msg(message_buffer_t* msg_buf) {
    assert(msg_buf != nullptr);

    if (msg_buf->data_part_length == 0) [[unlikely]] {
      msg_buf_destroy(msg_buf);
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = FS_CODE_E_ILL_ARGS;
      return;
    }

    uintptr_t msg_type = msg_buf->data[msg_buf->cap_part_length];

    if (msg_type < FS_MSG_TYPE_MOUNT || msg_type > FS_MSG_TYPE_RMDIR) [[unlikely]] {
      msg_buf_destroy(msg_buf);
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = FS_CODE_E_ILL_ARGS;
    } else {
      table[msg_type](msg_buf);
    }
  }
} // namespace

[[noreturn]] void run() {
  message_buffer_t msg_buf;
  sysret_t         sysret;

  sysret.error = SYS_E_UNKNOWN;
  while (true) {
    if (unwrap_sysret(sys_task_cap_get_free_slot_count(__this_task_cap)) < 0x10) [[unlikely]] {
      cap_space_cap_t cap_space_cap = mm_fetch_and_create_cap_space_object();
      unwrap_sysret(sys_task_cap_insert_cap_space(__this_task_cap, cap_space_cap));
    }

    if (sysret_succeeded(sysret)) {
      sysret = sys_endpoint_cap_reply_and_receive(__this_ep_cap, &msg_buf);
    } else {
      sysret = sys_endpoint_cap_receive(__this_ep_cap, &msg_buf);
    }

    if (sysret_succeeded(sysret)) {
      proc_msg(&msg_buf);
    }
  }
}
