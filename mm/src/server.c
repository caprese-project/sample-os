#include <libcaprese/ipc.h>
#include <libcaprese/syscall.h>
#include <mm/internal_heap.h>
#include <mm/ipc.h>
#include <mm/server.h>
#include <stdbool.h>

static void attach(message_buffer_t* msg_buf) {
  assert(msg_buf->data[msg_buf->cap_part_length] == MM_MSG_TYPE_ATTACH);

  if (msg_buf->cap_part_length != 1 || msg_buf->data_part_length < 2) {
    msg_buf->data_part_length               = 1;
    msg_buf->data[msg_buf->cap_part_length] = MM_CODE_E_ILL_ARGS;
    return;
  }

  task_cap_t task_cap  = msg_buf->data[0];
  uintptr_t  heap_root = msg_buf->data[msg_buf->cap_part_length + 1];

  if (unwrap_sysret(sys_cap_type(task_cap)) != CAP_TASK) {
    msg_buf->data_part_length               = 1;
    msg_buf->data[msg_buf->cap_part_length] = MM_CODE_E_ILL_ARGS;
    return;
  }

  (void)task_cap;
  (void)heap_root;

  id_cap_t id_cap        = unwrap_sysret(sys_id_cap_create());
  id_cap_t copied_id_cap = unwrap_sysret(sys_id_cap_copy(id_cap));

  // TODO: impl

  msg_buf->cap_part_length  = 1;
  msg_buf->data_part_length = 1;
  msg_buf->data[0]          = copied_id_cap;
  msg_buf->data[1]          = MM_CODE_S_OK;
}

static void detach(message_buffer_t* msg_buf) {
  assert(msg_buf->data[msg_buf->cap_part_length] == MM_MSG_TYPE_DETACH);

  if (msg_buf->cap_part_length != 1 || msg_buf->data_part_length < 1) {
    msg_buf->data_part_length               = 1;
    msg_buf->data[msg_buf->cap_part_length] = MM_CODE_E_ILL_ARGS;
    return;
  }

  id_cap_t id_cap = msg_buf->data[msg_buf->cap_part_length];

  if (unwrap_sysret(sys_cap_type(id_cap)) != CAP_ID) {
    msg_buf->data_part_length               = 1;
    msg_buf->data[msg_buf->cap_part_length] = MM_CODE_E_ILL_ARGS;
    return;
  }

  // TODO: impl

  msg_buf->cap_part_length  = 0;
  msg_buf->data_part_length = 1;
  msg_buf->data[0]          = MM_CODE_S_OK;
}

static void sbrk(message_buffer_t* msg_buf) {
  assert(msg_buf->data[msg_buf->cap_part_length] == MM_MSG_TYPE_SBRK);

  if (msg_buf->cap_part_length != 1 || msg_buf->data_part_length < 2) {
    msg_buf->data_part_length               = 1;
    msg_buf->data[msg_buf->cap_part_length] = MM_CODE_E_ILL_ARGS;
    return;
  }

  id_cap_t id_cap    = msg_buf->data[0];
  intptr_t increment = msg_buf->data[msg_buf->cap_part_length + 1];

  if (unwrap_sysret(sys_cap_type(id_cap)) != CAP_ID) {
    msg_buf->data_part_length               = 1;
    msg_buf->data[msg_buf->cap_part_length] = MM_CODE_E_ILL_ARGS;
    return;
  }

  (void)id_cap;
  (void)increment;

  // TODO: impl

  msg_buf->cap_part_length  = 0;
  msg_buf->data_part_length = 1;
  msg_buf->data[0]          = MM_CODE_S_OK;
}

static void fetch(message_buffer_t* msg_buf) {
  assert(msg_buf->data[msg_buf->cap_part_length] == MM_MSG_TYPE_FETCH);

  if (msg_buf->cap_part_length != 0 || msg_buf->data_part_length < 4) {
    msg_buf->data_part_length               = 1;
    msg_buf->data[msg_buf->cap_part_length] = MM_CODE_E_ILL_ARGS;
    return;
  }

  size_t   size      = msg_buf->data[1];
  size_t   alignment = msg_buf->data[2];
  uint32_t flags     = msg_buf->data[3];

  (void)size;
  (void)alignment;
  (void)flags;

  // TODO: impl

  msg_buf->cap_part_length  = 1;
  msg_buf->data_part_length = 1;
  msg_buf->data[1]          = MM_CODE_S_OK;
}

static void retrieve(message_buffer_t* msg_buf) {
  assert(msg_buf->data[msg_buf->cap_part_length] == MM_MSG_TYPE_RETRIEVE);

  if (msg_buf->cap_part_length != 0 || msg_buf->data_part_length < 3) {
    msg_buf->data_part_length               = 1;
    msg_buf->data[msg_buf->cap_part_length] = MM_CODE_E_ILL_ARGS;
    return;
  }

  uintptr_t addr = msg_buf->data[1];
  size_t    size = msg_buf->data[2];

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
}

static void revoke(message_buffer_t* msg_buf) {
  assert(msg_buf->data[msg_buf->cap_part_length] == MM_MSG_TYPE_REVOKE);
  (void)msg_buf;
}

static void proc_msg(message_buffer_t* msg_buf) {
  assert(msg_buf != NULL);

  if (msg_buf->data_part_length == 0) {
    msg_buf->data_part_length               = 1;
    msg_buf->data[msg_buf->cap_part_length] = MM_CODE_E_ILL_ARGS;
    return;
  }

  uintptr_t msg_type = msg_buf->data[msg_buf->cap_part_length];

  switch (msg_type) {
    case MM_MSG_TYPE_ATTACH:
      attach(msg_buf);
      break;
    case MM_MSG_TYPE_DETACH:
      detach(msg_buf);
      break;
    case MM_MSG_TYPE_SBRK:
      sbrk(msg_buf);
      break;
    case MM_MSG_TYPE_FETCH:
      fetch(msg_buf);
      break;
    case MM_MSG_TYPE_RETRIEVE:
      retrieve(msg_buf);
      break;
    case MM_MSG_TYPE_REVOKE:
      revoke(msg_buf);
      break;
    default:
      msg_buf->data_part_length               = 1;
      msg_buf->data[msg_buf->cap_part_length] = MM_CODE_E_ILL_ARGS;
      break;
  }
}

noreturn void run(endpoint_cap_t ep_cap) {
  message_buffer_t msg_buf;
  sysret_t         sysret;

  sysret.error = SYS_E_UNKNOWN;
  while (true) {
    if (sysret_succeeded(sysret)) {
      sysret = sys_endpoint_cap_reply_and_receive(ep_cap, &msg_buf);
    } else {
      sysret = sys_endpoint_cap_receive(ep_cap, &msg_buf);
    }

    if (sysret_succeeded(sysret)) {
      proc_msg(&msg_buf);
    }
  }
}
