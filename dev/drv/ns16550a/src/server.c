#include "server.h"

#include "uart.h"

#include <assert.h>
#include <crt/global.h>
#include <internal/branch.h>
#include <libcaprese/syscall.h>
#include <service/mm.h>
#include <stdbool.h>
#include <uart/ipc.h>

static void proc_putc(message_t* msg) {
  assert(msg != NULL);
  assert(get_ipc_data(msg, 0) == UART_MSG_TYPE_PUTC);

  int ch = (char)(get_ipc_data(msg, 1) & 0xff);
  uart_putc(ch);

  destroy_ipc_message(msg);
  set_ipc_data(msg, 0, UART_CODE_S_OK);
}

static void proc_getc(message_t* msg) {
  assert(msg != NULL);
  assert(get_ipc_data(msg, 0) == UART_MSG_TYPE_GETC);

  int ch = uart_getc();

  destroy_ipc_message(msg);
  set_ipc_data(msg, 0, UART_CODE_S_OK);
  set_ipc_data(msg, 1, (uintptr_t)((intptr_t)ch));
}

static void (*const table[])(message_t*) = {
  [0]                  = NULL,
  [UART_MSG_TYPE_PUTC] = proc_putc,
  [UART_MSG_TYPE_GETC] = proc_getc,
};

static void proc_msg(message_t* msg) {
  assert(msg != NULL);

  uintptr_t msg_type = get_ipc_data(msg, 0);

  __if_unlikely (msg_type < UART_MSG_TYPE_PUTC || msg_type > UART_MSG_TYPE_GETC) {
    destroy_ipc_message(msg);
    set_ipc_data(msg, 0, UART_CODE_E_ILL_ARGS);
    return;
  }

  table[msg_type](msg);
}

noreturn void run() {
  message_t* msg = new_ipc_message(sizeof(uintptr_t) * 2);
  sysret_t   sysret;

  sysret.error = SYS_E_UNKNOWN;
  while (true) {
    __if_unlikely (unwrap_sysret(sys_task_cap_get_free_slot_count(__this_task_cap)) < 0x10) {
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
