#include <cons/uart_file.h>
#include <libcaprese/syscall.h>
#include <uart/ipc.h>

void uart_putc(endpoint_cap_t ep_cap, char ch) {
  message_buffer_t msg_buf;

  msg_buf.cap_part_length  = 0;
  msg_buf.data_part_length = 2;

  msg_buf.data[0] = UART_MSG_TYPE_PUTC;
  msg_buf.data[1] = static_cast<uintptr_t>(ch) & 0xff;

  sys_endpoint_cap_call(ep_cap, &msg_buf);
}

char uart_getc(endpoint_cap_t ep_cap) {
  message_buffer_t msg_buf;

  char ch = static_cast<char>(-1);

  while (ch == static_cast<char>(-1)) {
    msg_buf.cap_part_length  = 0;
    msg_buf.data_part_length = 1;

    msg_buf.data[0] = UART_MSG_TYPE_GETC;

    sys_endpoint_cap_call(ep_cap, &msg_buf);

    if (msg_buf.data_part_length == 0 || msg_buf.data[0] != UART_CODE_S_OK) [[unlikely]] {
      return static_cast<char>(-1);
    }

    ch = static_cast<char>(msg_buf.data[1]);
  }

  switch (ch) {
    case '\b':
    case '\x7f':
      uart_putc(ep_cap, '\b');
      uart_putc(ep_cap, ' ');
      uart_putc(ep_cap, '\b');
      break;
    case '\r':
      ch = '\n';
      uart_putc(ep_cap, '\n');
      break;
    default:
      uart_putc(ep_cap, ch);
      break;
  }

  return ch;
}
