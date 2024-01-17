#include <cons/support/uart.h>
#include <libcaprese/syscall.h>
#include <uart/ipc.h>

void uart_putc(endpoint_cap_t ep_cap, char ch) {
  char       msg_buf[sizeof(message_t) + sizeof(uintptr_t) * 2];
  message_t* msg               = reinterpret_cast<message_t*>(msg_buf);
  msg->header.payload_length   = 0;
  msg->header.payload_capacity = sizeof(uintptr_t) * 2;

  set_ipc_data(msg, 0, UART_MSG_TYPE_PUTC);
  set_ipc_data(msg, 1, static_cast<uintptr_t>(ch) & 0xff);

  sys_endpoint_cap_call(ep_cap, msg);
}

char uart_getc(endpoint_cap_t ep_cap) {
  char       msg_buf[sizeof(message_t) + sizeof(uintptr_t) * 2];
  message_t* msg               = reinterpret_cast<message_t*>(msg_buf);
  msg->header.payload_length   = 0;
  msg->header.payload_capacity = sizeof(uintptr_t) * 2;

  set_ipc_data(msg, 0, UART_MSG_TYPE_GETC);

  sys_endpoint_cap_call(ep_cap, msg);

  if (get_ipc_data(msg, 0) != UART_CODE_S_OK) {
    return static_cast<char>(-1);
  }

  return static_cast<char>(get_ipc_data(msg, 1));
}
