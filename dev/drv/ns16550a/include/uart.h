#ifndef DEV_NS16550A_UART_H_
#define DEV_NS16550A_UART_H_

#define UART_RBR 0 // I:   Receive Buffer Register
#define UART_THR 0 // O:   Transmitter Holding Register
#define UART_DLL 0 // O:   Divisor Latch Low
#define UART_IER 1 // I/O: Interrupt Enable Register
#define UART_DLM 1 // O:   Divisor Latch High
#define UART_FCR 2 // O:   FIFO Control Register
#define UART_IIR 2 // I/O: Interrupt Identification Register
#define UART_LCR 3 // O:   Line Control Register
#define UART_MCR 4 // O:   Modem Control Register
#define UART_LSR 5 // I:   Line Status Register
#define UART_MSR 6 // I:   Modem Status Register
#define UART_SCR 7 // I/O: Scratch Register
#define UART_MDR 8 // I/O: Mode Register

#define UART_IER_RX_ENABLE (1 << 0)
#define UART_IER_TX_ENABLE (1 << 1)

#define UART_FCR_FIFO_ENABLE (1 << 0)
#define UART_FCR_FIFO_CLEAR  (3 << 1)

#define UART_LCR_EIGHT_BITS (3 << 0)
#define UART_LCR_BAUD_LATCH (1 << 7)

#define UART_LSR_DR             (1 << 0) // Data Ready
#define UART_LSR_OE             (1 << 2) // Overrun Error
#define UART_LSR_PE             (1 << 3) // Parity Error
#define UART_LSR_FE             (1 << 4) // Frame Error
#define UART_LSR_BI             (1 << 5) // Break Interrupt
#define UART_LSR_THRE           (1 << 6) // THR Empty
#define UART_LSR_TEMT           (1 << 7) // Transmitter Empty
#define UART_LSR_FIFOE          (1 << 8) // FIFO Error
#define UART_LSR_BRK_ERROR_BITS (UART_LSR_OE | UART_LSR_PE | UART_LSR_FE | UART_LSR_BI)

#include <stdint.h>

void init_uart(uintptr_t base_addr, uint32_t frequency, uint32_t baudrate, uint32_t reg_shift, uint32_t reg_width, uint32_t reg_offset);
void uart_putc(int ch);
int  uart_getc(void);

#endif // DEV_NS16550A_UART_H_
