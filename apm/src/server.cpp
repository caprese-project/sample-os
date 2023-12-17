#include <apm/server.h>
#include <crt/global.h>
#include <libcaprese/syscall.h>
#include <service/mm.h>

endpoint_cap_t apm_ep_cap;

namespace {
  void proc_msg(message_buffer_t* msg_buf) {
    assert(msg_buf != nullptr);
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
