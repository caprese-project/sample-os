#include <apm/ipc.h>
#include <apm/server.h>
#include <apm/task_manager.h>
#include <crt/global.h>
#include <cstring>
#include <istream>
#include <libcaprese/ipc.h>
#include <libcaprese/syscall.h>
#include <memory>
#include <service/fs.h>
#include <service/mm.h>
#include <sstream>
#include <string>
#include <vector>

endpoint_cap_t apm_ep_cap;

namespace {
  uint32_t xorshift() {
    static uint32_t x = 123456789;
    static uint32_t y = 362436069;
    static uint32_t z = 521288629;
    static uint32_t w = 88675123;
    uint32_t        t;
    t = x ^ (x << 11);
    x = y;
    y = z;
    z = w;
    w ^= t ^ (t >> 8) ^ (w >> 19);
    return w;
  }

  std::string rand_name() {
    std::string name;
    name.reserve(32);
    for (size_t i = 0; i < 32; ++i) {
      name.push_back(static_cast<char>(xorshift() % 26 + 'a'));
    }
    return name;
  }

  void create(message_t* msg) {
    assert(get_ipc_data(msg, 0) == APM_MSG_TYPE_CREATE);

    int flags = static_cast<int>(get_ipc_data(msg, 1));
    int argc  = static_cast<int>(get_ipc_data(msg, 2));

    size_t      index = 3;
    std::string path  = reinterpret_cast<const char*>(get_ipc_data_ptr(msg, index));

    index += (path.size() + 1 + sizeof(uintptr_t) - 1) / sizeof(uintptr_t);
    std::string name = reinterpret_cast<const char*>(get_ipc_data_ptr(msg, index));

    index += (name.size() + 1 + sizeof(uintptr_t) - 1) / sizeof(uintptr_t);
    std::vector<std::string> args;
    for (int i = 0; i < argc; ++i) {
      args.emplace_back(reinterpret_cast<const char*>(get_ipc_data_ptr(msg, index)));
      index += (args.back().size() + 1 + sizeof(uintptr_t) - 1) / sizeof(uintptr_t);
    }

    if (__fs_ep_cap == 0) [[unlikely]] {
      if (!task_exists("fs")) {
        destroy_ipc_message(msg);
        set_ipc_data(msg, 0, APM_CODE_E_NO_SUCH_FILE);
        return;
      }

      const task& fs_task = lookup_task("fs");
      __fs_ep_cap         = fs_task.get_ep_cap().get();
    }

    id_cap_t fd = fs_open(path.c_str());

    if (fd == 0) [[unlikely]] {
      destroy_ipc_message(msg);
      set_ipc_data(msg, 0, APM_CODE_E_NO_SUCH_FILE);
      return;
    }

    // TODO: fstream
    std::string data;
    {
      std::unique_ptr<char[]> buf = std::make_unique<char[]>(0x1000);
      while (true) {
        size_t read_size = fs_read(fd, buf.get(), 0x1000);
        if (read_size == static_cast<size_t>(-1)) [[unlikely]] {
          destroy_ipc_message(msg);
          set_ipc_data(msg, 0, APM_CODE_E_FAILURE);
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

    if (name.empty()) {
      name = "{" + rand_name() + "}";
    }

    std::istringstream stream(data, std::ios_base::binary);
    if (!create_task(name, std::ref<std::istream>(stream), flags, msg->header.sender_id, args)) [[unlikely]] {
      destroy_ipc_message(msg);
      set_ipc_data(msg, 0, APM_CODE_E_FAILURE);
      return;
    }

    const task& task = lookup_task(name);

    destroy_ipc_message(msg);
    set_ipc_data(msg, 0, APM_CODE_S_OK);
    set_ipc_cap(msg, 1, unwrap_sysret(sys_task_cap_copy(task.get_task_cap().get())), false);
  }

  void lookup(message_t* msg) {
    assert(get_ipc_data(msg, 0) == APM_MSG_TYPE_LOOKUP);

    std::string name = reinterpret_cast<const char*>(get_ipc_data_ptr(msg, 1));

    if (!task_exists(name)) [[unlikely]] {
      destroy_ipc_message(msg);
      set_ipc_data(msg, 0, APM_CODE_E_NO_SUCH_TASK);
      return;
    }

    const task& task = lookup_task(name);

    destroy_ipc_message(msg);
    set_ipc_data(msg, 0, APM_CODE_S_OK);
    set_ipc_cap(msg, 1, unwrap_sysret(sys_endpoint_cap_copy(task.get_ep_cap().get())), false);
  }

  void attach(message_t* msg) {
    assert(get_ipc_data(msg, 0) == APM_MSG_TYPE_ATTACH);

    task_cap_t task_cap = move_ipc_cap(msg, 1);

    if (unwrap_sysret(sys_cap_type(task_cap)) != CAP_TASK) [[unlikely]] {
      destroy_ipc_message(msg);
      set_ipc_data(msg, 0, APM_CODE_E_ILL_ARGS);
      return;
    }

    endpoint_cap_t ep_cap = move_ipc_cap(msg, 2);

    if (unwrap_sysret(sys_cap_type(ep_cap)) != CAP_ENDPOINT) [[unlikely]] {
      destroy_ipc_message(msg);
      set_ipc_data(msg, 0, APM_CODE_E_ILL_ARGS);
      return;
    }

    std::string name = reinterpret_cast<const char*>(get_ipc_data_ptr(msg, 3));

    if (!attach_task(name, task_cap, ep_cap)) [[unlikely]] {
      destroy_ipc_message(msg);
      set_ipc_data(msg, 0, APM_CODE_E_FAILURE);
      return;
    }

    destroy_ipc_message(msg);
    set_ipc_data(msg, 0, APM_CODE_S_OK);
  }

  void setenv(message_t* msg) {
    assert(get_ipc_data(msg, 0) == APM_MSG_TYPE_SETENV);

    task_cap_t task_cap = get_ipc_cap(msg, 1);

    if (unwrap_sysret(sys_cap_type(task_cap)) != CAP_TASK) [[unlikely]] {
      destroy_ipc_message(msg);
      set_ipc_data(msg, 0, APM_CODE_E_ILL_ARGS);
      return;
    }

    std::string env   = reinterpret_cast<const char*>(get_ipc_data_ptr(msg, 2));
    std::string value = reinterpret_cast<const char*>(get_ipc_data_ptr(msg, 2 + (env.size() + 1 + sizeof(uintptr_t) - 1) / sizeof(uintptr_t)));

    uint32_t tid = unwrap_sysret(sys_task_cap_tid(task_cap));

    if (!task_exists(tid)) [[unlikely]] {
      destroy_ipc_message(msg);
      set_ipc_data(msg, 0, APM_CODE_E_NO_SUCH_TASK);
      return;
    }

    task& task = lookup_task(tid);

    if (!task.set_env(env, value)) [[unlikely]] {
      destroy_ipc_message(msg);
      set_ipc_data(msg, 0, APM_CODE_E_FAILURE);
      return;
    }

    destroy_ipc_message(msg);
    set_ipc_data(msg, 0, APM_CODE_S_OK);
  }

  void getenv(message_t* msg) {
    assert(get_ipc_data(msg, 0) == APM_MSG_TYPE_GETENV);

    task_cap_t task_cap = get_ipc_cap(msg, 1);

    if (unwrap_sysret(sys_cap_type(task_cap)) != CAP_TASK) [[unlikely]] {
      destroy_ipc_message(msg);
      set_ipc_data(msg, 0, APM_CODE_E_ILL_ARGS);
      return;
    }

    std::string env = reinterpret_cast<const char*>(get_ipc_data_ptr(msg, 2));

    uint32_t tid = unwrap_sysret(sys_task_cap_tid(task_cap));

    if (!task_exists(tid)) [[unlikely]] {
      destroy_ipc_message(msg);
      set_ipc_data(msg, 0, APM_CODE_E_NO_SUCH_TASK);
      return;
    }

    task& task = lookup_task(tid);

    std::string value;
    if (!task.get_env(env, value)) [[unlikely]] {
      destroy_ipc_message(msg);
      set_ipc_data(msg, 0, APM_CODE_E_FAILURE);
      return;
    }

    destroy_ipc_message(msg);
    set_ipc_data(msg, 0, APM_CODE_S_OK);
    set_ipc_data(msg, 1, value.size() + 1);
    set_ipc_data_array(msg, 2, value.c_str(), value.size() + 1);
  }

  // clang-format off

  constexpr void (*const table[])(message_t*) = {
    [0]                   = nullptr,
    [APM_MSG_TYPE_CREATE] = create,
    [APM_MSG_TYPE_LOOKUP] = lookup,
    [APM_MSG_TYPE_ATTACH] = attach,
    [APM_MSG_TYPE_SETENV] = setenv,
    [APM_MSG_TYPE_GETENV] = getenv,
  };

  // clang-format on

  void proc_msg(message_t* msg) {
    assert(msg != nullptr);

    if (msg->header.msg_type != MSG_TYPE_IPC) {
      return;
    }

    uintptr_t msg_type = get_ipc_data(msg, 0);

    if (msg_type < APM_MSG_TYPE_CREATE || msg_type > APM_MSG_TYPE_GETENV) [[unlikely]] {
      destroy_ipc_message(msg);
      set_ipc_data(msg, 0, APM_CODE_E_ILL_ARGS);
    } else {
      table[msg_type](msg);
    }
  }
} // namespace

[[noreturn]] void run() {
  message_t* msg = new_ipc_message(0x100 + APM_ENV_MAX_LEN);
  sysret_t   sysret;

  sysret.error = SYS_E_UNKNOWN;
  while (true) {
    if (unwrap_sysret(sys_task_cap_get_free_slot_count(__this_task_cap)) < 0x10) [[unlikely]] {
      cap_space_cap_t cap_space_cap = mm_fetch_and_create_cap_space_object();
      unwrap_sysret(sys_task_cap_insert_cap_space(__this_task_cap, cap_space_cap));
    }

    if (sysret_succeeded(sysret)) {
      sysret = sys_endpoint_cap_reply_and_receive(apm_ep_cap, msg);
    } else {
      sysret = sys_endpoint_cap_receive(apm_ep_cap, msg);
    }

    if (sysret_succeeded(sysret)) {
      proc_msg(msg);
    }
  }
}
