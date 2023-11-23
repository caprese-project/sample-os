#include <libcaprese/ipc.h>
#include <libcaprese/syscall.h>
#include <mm/ipc.h>
#include <mm/server.h>
#include <stdbool.h>

static void attach(endpoint_cap_t ep_cap, message_buffer_t* msg_buf) {
  (void)ep_cap;
  (void)msg_buf;
}

static void detach(endpoint_cap_t ep_cap, message_buffer_t* msg_buf) {
  (void)ep_cap;
  (void)msg_buf;
}

static void fetch(endpoint_cap_t ep_cap, message_buffer_t* msg_buf) {
  (void)ep_cap;
  (void)msg_buf;
}

noreturn void run(endpoint_cap_t ep_cap) {
  while (true) {
    message_buffer_t msg_buf;

    sysret_t sysret = sys_endpoint_cap_receive(ep_cap, &msg_buf);
    if (sysret.error != 0) {
      continue;
    }

    uintptr_t msg_type = msg_buf.data[msg_buf.cap_part_length];

    switch (msg_type) {
      case MM_MSG_TYPE_ATTACH:
        attach(ep_cap, &msg_buf);
        break;
      case MM_MSG_TYPE_DETACH:
        detach(ep_cap, &msg_buf);
        break;
      case MM_MSG_TYPE_FETCH:
        fetch(ep_cap, &msg_buf);
        break;
    }
  }
}
