#include <apm/ipc.h>
#include <apm/ramfs.h>
#include <apm/server.h>
#include <apm/task_manager.h>
#include <crt/global.h>
#include <istream>
#include <libcaprese/syscall.h>
#include <service/mm.h>
#include <string.h>

extern "C" {
  extern const char _ramfs_start[];
  extern const char _ramfs_end[];
}

endpoint_cap_t apm_ep_cap;

namespace {
  void create(message_buffer_t* msg_buf) {
    assert(msg_buf->data[msg_buf->cap_part_length] == APM_MSG_TYPE_CREATE);

    if (msg_buf->cap_part_length != 0 || msg_buf->data_part_length < 3) [[unlikely]] {
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = APM_CODE_E_ILL_ARGS;
      return;
    }

    int         flags       = static_cast<int>(msg_buf->data[1]);
    char*       start_start = reinterpret_cast<char*>(&msg_buf->data[2]);
    std::string path        = start_start;
    std::string name        = start_start + path.size() + 1;

    if (path.starts_with("/init/")) [[unlikely]] {
      ramfs              fs(_ramfs_start, _ramfs_end);
      std::istringstream stream = fs.open_file(path.substr(6).c_str());

      if (!stream) [[unlikely]] {
        msg_buf->data_part_length               = 1;
        msg_buf->data[msg_buf->cap_part_length] = APM_CODE_E_NO_SUCH_FILE;
        return;
      }

      if (!create_task(name, std::ref<std::istream>(stream), flags)) [[unlikely]] {
        msg_buf->data_part_length               = 1;
        msg_buf->data[msg_buf->cap_part_length] = APM_CODE_E_FAILURE;
        return;
      }

      const task& task = lookup_task(name);

      msg_buf->cap_part_length                = 1;
      msg_buf->data[0]                        = unwrap_sysret(sys_task_cap_copy(task.get_task_cap().get()));
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = APM_CODE_S_OK;
    } else {
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = APM_CODE_E_NO_SUCH_FILE;
    }
  }

  void lookup(message_buffer_t* msg_buf) {
    assert(msg_buf->data[msg_buf->cap_part_length] == APM_MSG_TYPE_LOOKUP);

    // TODO: impl

    msg_buf->data_part_length               = 1;
    msg_buf->data[msg_buf->cap_part_length] = APM_CODE_E_FAILURE;
  }

  // clang-format off

  void (*const table[])(message_buffer_t*) = {
    [0]                   = nullptr,
    [APM_MSG_TYPE_CREATE] = create,
    [APM_MSG_TYPE_LOOKUP] = lookup,
  };

  // clang-format on

  void proc_msg(message_buffer_t* msg_buf) {
    assert(msg_buf != nullptr);

    if (msg_buf->data_part_length == 0) [[unlikely]] {
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = APM_CODE_E_ILL_ARGS;
      return;
    }

    uintptr_t msg_type = msg_buf->data[msg_buf->cap_part_length];

    if (msg_type < APM_MSG_TYPE_CREATE || msg_type > APM_MSG_TYPE_LOOKUP) [[unlikely]] {
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = APM_CODE_E_ILL_ARGS;
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
      sysret = sys_endpoint_cap_reply_and_receive(apm_ep_cap, &msg_buf);
    } else {
      sysret = sys_endpoint_cap_receive(apm_ep_cap, &msg_buf);
    }

    if (sysret_succeeded(sysret)) {
      proc_msg(&msg_buf);
    }
  }
}
