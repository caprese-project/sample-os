#include <cons/file.h>
#include <cons/server.h>
#include <crt/global.h>
#include <fs/ipc.h>
#include <libcaprese/cxx/id_map.h>
#include <libcaprese/syscall.h>
#include <service/mm.h>
#include <string>

id_cap_t cons_id_cap;

namespace {
  void open(message_buffer_t* msg_buf) {
    assert(msg_buf->data[msg_buf->cap_part_length] == FS_MSG_TYPE_OPEN);

    if (msg_buf->cap_part_length != 1 || msg_buf->data_part_length < 2) [[unlikely]] {
      msg_buf_destroy(msg_buf);
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = FS_CODE_E_ILL_ARGS;
      return;
    }

    const char* c_path = reinterpret_cast<const char*>(&msg_buf->data[msg_buf->cap_part_length + 1]);

    id_cap_t fd;
    int      result = cons_open(c_path, &fd);

    if (result != FS_CODE_S_OK) [[unlikely]] {
      msg_buf_destroy(msg_buf);
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = result;
      return;
    }

    msg_buf->cap_part_length                = 1;
    msg_buf->data[0]                        = unwrap_sysret(sys_id_cap_copy(fd));
    msg_buf->data_part_length               = 1;
    msg_buf->data[msg_buf->cap_part_length] = FS_CODE_S_OK;
  }

  void close(message_buffer_t* msg_buf) {
    assert(msg_buf->data[msg_buf->cap_part_length] == FS_MSG_TYPE_CLOSE);

    if (msg_buf->cap_part_length != 2) [[unlikely]] {
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = FS_CODE_E_ILL_ARGS;
      return;
    }

    id_cap_t fd = msg_buf->data[1];
    if (unwrap_sysret(sys_cap_type(fd)) != CAP_ID) [[unlikely]] {
      msg_buf_destroy(msg_buf);
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = FS_CODE_E_ILL_ARGS;
      return;
    }

    int result = cons_close(fd);

    msg_buf_destroy(msg_buf);

    msg_buf->data_part_length               = 1;
    msg_buf->data[msg_buf->cap_part_length] = result;
  }

  void read(message_buffer_t* msg_buf) {
    assert(msg_buf->data[msg_buf->cap_part_length] == FS_MSG_TYPE_READ);

    if (msg_buf->cap_part_length != 2 || msg_buf->data_part_length < 2) [[unlikely]] {
      msg_buf_destroy(msg_buf);
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = FS_CODE_E_ILL_ARGS;
      return;
    }

    id_cap_t fd = msg_buf->data[1];

    if (unwrap_sysret(sys_cap_type(fd)) != CAP_ID) [[unlikely]] {
      msg_buf_destroy(msg_buf);
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = FS_CODE_E_ILL_ARGS;
      return;
    }

    std::streamsize size = std::min(msg_buf->data[msg_buf->cap_part_length + 1], (std::size(msg_buf->data) - 2) * sizeof(uintptr_t));

    msg_buf->data[0] = cons_read(fd, &msg_buf->data[2], size, &msg_buf->data[1]);

    sys_cap_destroy(fd);

    msg_buf->cap_part_length = 0;
    if (msg_buf->data[0] != FS_CODE_S_OK) [[unlikely]] {
      msg_buf->data_part_length = 1;
    } else {
      msg_buf->data_part_length = 2 + (msg_buf->data[1] + sizeof(uintptr_t) - 1) / sizeof(uintptr_t);
    }
  }

  void write(message_buffer_t* msg_buf) {
    assert(msg_buf->data[msg_buf->cap_part_length] == FS_MSG_TYPE_WRITE);

    if (msg_buf->cap_part_length != 2 || msg_buf->data_part_length < 2) [[unlikely]] {
      msg_buf_destroy(msg_buf);
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = FS_CODE_E_ILL_ARGS;
      return;
    }

    id_cap_t fd = msg_buf->data[1];

    if (unwrap_sysret(sys_cap_type(fd)) != CAP_ID) [[unlikely]] {
      msg_buf_destroy(msg_buf);
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = FS_CODE_E_ILL_ARGS;
      return;
    }

    std::streamsize size = std::min(msg_buf->data[msg_buf->cap_part_length + 1], (std::size(msg_buf->data) - 4) * sizeof(uintptr_t));

    msg_buf->data[0] = cons_write(fd, &msg_buf->data[msg_buf->cap_part_length + 2], size, &msg_buf->data[1]);

    sys_cap_destroy(fd);

    msg_buf->cap_part_length = 0;
    if (msg_buf->data[0] != FS_CODE_S_OK) [[unlikely]] {
      msg_buf->data_part_length = 1;
    } else {
      msg_buf->data_part_length = 2;
    }
  }

  void (*const table[])(message_buffer_t*) = {
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

  void proc_msg(message_buffer_t* msg_buf) {
    if (msg_buf->cap_part_length == 0 || msg_buf->data_part_length == 0) [[unlikely]] {
      msg_buf_destroy(msg_buf);
      msg_buf->data_part_length = 1;
      msg_buf->data[0]          = FS_CODE_E_ILL_ARGS;
      return;
    }

    id_cap_t id = msg_buf->data[0];
    if (unwrap_sysret(sys_cap_type(id)) != CAP_ID) [[unlikely]] {
      msg_buf_destroy(msg_buf);
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = FS_CODE_E_ILL_ARGS;
      return;
    }

    if (unwrap_sysret(sys_id_cap_compare(id, cons_id_cap)) != 0) [[unlikely]] {
      msg_buf_destroy(msg_buf);
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = FS_CODE_E_ILL_ARGS;
      return;
    }

    uintptr_t msg_type = msg_buf->data[msg_buf->cap_part_length];

    if (msg_type < FS_MSG_TYPE_OPEN || msg_type > FS_MSG_TYPE_RMDIR) [[unlikely]] {
      msg_buf_destroy(msg_buf);
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = FS_CODE_E_UNSUPPORTED;
      return;
    }

    if (table[msg_type] == NULL) [[unlikely]] {
      msg_buf_destroy(msg_buf);
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = FS_CODE_E_UNSUPPORTED;
      return;
    }

    table[msg_type](msg_buf);

    if (msg_buf->data[msg_buf->cap_part_length] == FS_CODE_S_OK) {
      sys_cap_destroy(id);
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
