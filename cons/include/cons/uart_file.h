#ifndef CONS_UART_FILE_H_
#define CONS_UART_FILE_H_

#include <libcaprese/cap.h>

void uart_putc(endpoint_cap_t ep_cap, char ch);
char uart_getc(endpoint_cap_t ep_cap);

#endif // CONS_UART_FILE_H_
