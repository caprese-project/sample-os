#include <libcaprese/ipc.h>
#include <libcaprese/syscall.h>
#include <mm/internal_heap.h>
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
  assert(msg_buf->data[msg_buf->cap_part_length] == MM_MSG_TYPE_FETCH);

  size_t   size      = msg_buf->data[msg_buf->cap_part_length + 1];
  size_t   alignment = msg_buf->data[msg_buf->cap_part_length + 2];
  uint32_t flags     = msg_buf->data[msg_buf->cap_part_length + 3];

  (void)size;
  (void)alignment;
  (void)flags;

  (void)ep_cap;
}

static void retrieve(endpoint_cap_t ep_cap, message_buffer_t* msg_buf) {
  assert(msg_buf->data[msg_buf->cap_part_length] == MM_MSG_TYPE_RETRIEVE);

  uintptr_t addr = msg_buf->data[msg_buf->cap_part_length + 1];
  size_t    size = msg_buf->data[msg_buf->cap_part_length + 2];

  for (size_t i = 0; i < num_mem_caps; ++i) {
    uintptr_t phys_addr = unwrap_sysret(sys_mem_cap_phys_addr(mem_caps[i]));
    size_t    mem_size  = 1 << unwrap_sysret(sys_mem_cap_size_bit(mem_caps[i]));
    uintptr_t used_size = unwrap_sysret(sys_mem_cap_used_size(mem_caps[i]));
    uintptr_t start     = phys_addr + used_size;
    uintptr_t end       = phys_addr + mem_size;

    if (start <= addr && addr + size <= end) {
      msg_buf->data[0] = mem_caps[i];
    }
  }

  (void)ep_cap;
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
      case MM_MSG_TYPE_RETRIEVE:
        retrieve(ep_cap, &msg_buf);
        break;
    }
  }
}
