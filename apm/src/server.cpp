#include <apm/ipc.h>
#include <apm/server.h>
#include <apm/task_manager.h>
#include <crt/global.h>
#include <cstring>
#include <istream>
#include <libcaprese/syscall.h>
#include <memory>
#include <service/fs.h>
#include <service/mm.h>
#include <sstream>
#include <string>

endpoint_cap_t apm_ep_cap;

namespace {
  void create(message_buffer_t* msg_buf) {
    assert(msg_buf->data[msg_buf->cap_part_length] == APM_MSG_TYPE_CREATE);

    if (msg_buf->cap_part_length != 0 || msg_buf->data_part_length < 3) [[unlikely]] {
      msg_buf_destroy(msg_buf);
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = APM_CODE_E_ILL_ARGS;
      return;
    }

    int         flags     = static_cast<int>(msg_buf->data[1]);
    char*       str_start = reinterpret_cast<char*>(&msg_buf->data[2]);
    std::string path      = str_start;
    std::string name      = str_start + path.size() + 1;

    if (__fs_ep_cap == 0) [[unlikely]] {
      if (!task_exists("fs")) {
        msg_buf_destroy(msg_buf);
        msg_buf->data_part_length               = 1;
        msg_buf->data[msg_buf->cap_part_length] = APM_CODE_E_NO_SUCH_FILE;
        return;
      }

      const task& fs_task = lookup_task("fs");
      __fs_ep_cap         = fs_task.get_ep_cap().get();
    }

    id_cap_t fd = fs_open(path.c_str());

    if (fd == 0) [[unlikely]] {
      msg_buf_destroy(msg_buf);
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = APM_CODE_E_NO_SUCH_FILE;
      return;
    }

    // TODO: fstream
    std::string data;
    {
      std::unique_ptr<char[]> buf = std::make_unique<char[]>(0x1000);
      while (true) {
        size_t read_size = fs_read(fd, buf.get(), 0x1000);
        if (read_size == static_cast<size_t>(-1)) [[unlikely]] {
          msg_buf_destroy(msg_buf);
          msg_buf->data_part_length               = 1;
          msg_buf->data[msg_buf->cap_part_length] = APM_CODE_E_FAILURE;
          return;
        }
        if (read_size == 0) [[unlikely]] {
          break;
        }
        data.append(buf.get(), read_size);
        if (read_size < 0x1000) [[unlikely]] {
          break;
        }
      }
    }

    std::istringstream stream(data, std::ios_base::binary);
    if (!create_task(name, std::ref<std::istream>(stream), flags)) [[unlikely]] {
      msg_buf_destroy(msg_buf);
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = APM_CODE_E_FAILURE;
      return;
    }

     const task& task = lookup_task(name);

    msg_buf->cap_part_length                = 1;
    msg_buf->data[0]                        = unwrap_sysret(sys_task_cap_copy(task.get_task_cap().get()));
    msg_buf->data_part_length               = 1;
    msg_buf->data[msg_buf->cap_part_length] = APM_CODE_S_OK;
  }

  void lookup(message_buffer_t* msg_buf) {
    assert(msg_buf->data[msg_buf->cap_part_length] == APM_MSG_TYPE_LOOKUP);

    if (msg_buf->cap_part_length != 0 || msg_buf->data_part_length < 2) [[unlikely]] {
      msg_buf_destroy(msg_buf);
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = APM_CODE_E_ILL_ARGS;
      return;
    }

    char*       str_start = reinterpret_cast<char*>(&msg_buf->data[1]);
    std::string name      = str_start;

    if (!task_exists(name)) [[unlikely]] {
      msg_buf_destroy(msg_buf);
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = APM_CODE_E_NO_SUCH_TASK;
      return;
    }

    const task& task = lookup_task(name);

    msg_buf->cap_part_length                = 1;
    msg_buf->data[0]                        = unwrap_sysret(sys_endpoint_cap_copy(task.get_ep_cap().get()));
    msg_buf->data_part_length               = 1;
    msg_buf->data[msg_buf->cap_part_length] = APM_CODE_S_OK;
  }

  void attach(message_buffer_t* msg_buf) {
    assert(msg_buf->data[msg_buf->cap_part_length] == APM_MSG_TYPE_ATTACH);

    if (msg_buf->cap_part_length != 2 || msg_buf->data_part_length < 2) [[unlikely]] {
      msg_buf_destroy(msg_buf);
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = APM_CODE_E_ILL_ARGS;
      return;
    }

    task_cap_t task_cap = msg_buf->data[0];

    if (unwrap_sysret(sys_cap_type(task_cap)) != CAP_TASK) [[unlikely]] {
      msg_buf_destroy(msg_buf);
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = APM_CODE_E_ILL_ARGS;
      return;
    }

    endpoint_cap_t ep_cap = msg_buf->data[1];

    if (unwrap_sysret(sys_cap_type(ep_cap)) != CAP_ENDPOINT) [[unlikely]] {
      msg_buf_destroy(msg_buf);
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = APM_CODE_E_ILL_ARGS;
      return;
    }

    char*       str_start = reinterpret_cast<char*>(&msg_buf->data[msg_buf->cap_part_length + 1]);
    std::string name      = str_start;

    if (!attach_task(name, task_cap, ep_cap)) [[unlikely]] {
      msg_buf_destroy(msg_buf);
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = APM_CODE_E_FAILURE;
      return;
    }

    msg_buf->cap_part_length                = 0;
    msg_buf->data_part_length               = 1;
    msg_buf->data[msg_buf->cap_part_length] = APM_CODE_S_OK;
  }

  // clang-format off

  void (*const table[])(message_buffer_t*) = {
    [0]                   = nullptr,
    [APM_MSG_TYPE_CREATE] = create,
    [APM_MSG_TYPE_LOOKUP] = lookup,
    [APM_MSG_TYPE_ATTACH] = attach,
  };

  // clang-format on

  void proc_msg(message_buffer_t* msg_buf) {
    assert(msg_buf != nullptr);

    if (msg_buf->data_part_length == 0) [[unlikely]] {
      msg_buf_destroy(msg_buf);
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = APM_CODE_E_ILL_ARGS;
      return;
    }

    uintptr_t msg_type = msg_buf->data[msg_buf->cap_part_length];

    if (msg_type < APM_MSG_TYPE_CREATE || msg_type > APM_MSG_TYPE_ATTACH) [[unlikely]] {
      msg_buf_destroy(msg_buf);
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
