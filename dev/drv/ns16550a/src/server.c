#include "server.h"

#include "uart.h"

#include <assert.h>
#include <crt/global.h>
#include <internal/branch.h>
#include <libcaprese/syscall.h>
#include <service/mm.h>
#include <stdbool.h>
#include <uart/ipc.h>

static void proc_putc(message_buffer_t* msg_buf) {
  assert(msg_buf != NULL);

  __if_unlikely (msg_buf->cap_part_length != 0 || msg_buf->data_part_length < 2) {
    msg_buf_destroy(msg_buf);
    msg_buf->data_part_length = 1;
    msg_buf->data[0]          = UART_CODE_E_ILL_ARGS;
    return;
  }

  int ch = (char)(msg_buf->data[msg_buf->cap_part_length + 1] & 0xff);
  uart_putc(ch);

  msg_buf->data_part_length = 1;
  msg_buf->data[0]          = UART_CODE_S_OK;
}

static void proc_getc(message_buffer_t* msg_buf) {
  assert(msg_buf != NULL);

  __if_unlikely (msg_buf->cap_part_length != 0 || msg_buf->data_part_length < 1) {
    msg_buf_destroy(msg_buf);
    msg_buf->data_part_length = 1;
    msg_buf->data[0]          = UART_CODE_E_ILL_ARGS;
    return;
  }

  int ch = uart_getc();

  msg_buf->data_part_length = 2;
  msg_buf->data[0]          = UART_CODE_S_OK;
  msg_buf->data[1]          = (uintptr_t)((intptr_t)ch);
}

static void (*const table[])(message_buffer_t*) = {
  [0]                  = NULL,
  [UART_MSG_TYPE_PUTC] = proc_putc,
  [UART_MSG_TYPE_GETC] = proc_getc,
};

static void proc_msg(message_buffer_t* msg_buf) {
  assert(msg_buf != NULL);

  __if_unlikely (msg_buf->data_part_length == 0) {
    msg_buf_destroy(msg_buf);
    msg_buf->data_part_length = 1;
    msg_buf->data[0]          = UART_CODE_E_ILL_ARGS;
    return;
  }

  uintptr_t msg_type = msg_buf->data[msg_buf->cap_part_length];

  __if_unlikely (msg_type < UART_MSG_TYPE_PUTC || msg_type > UART_MSG_TYPE_GETC) {
    msg_buf_destroy(msg_buf);
    msg_buf->data_part_length = 1;
    msg_buf->data[0]          = UART_CODE_E_ILL_ARGS;
  } else {
    table[msg_type](msg_buf);
  }
}

noreturn void run() {
  message_buffer_t msg_buf;
  sysret_t         sysret;

  sysret.error = SYS_E_UNKNOWN;
  while (true) {
    __if_unlikely (unwrap_sysret(sys_task_cap_get_free_slot_count(__this_task_cap)) < 0x10) {
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
