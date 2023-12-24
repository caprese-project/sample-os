#include "uart.h"

static uintptr_t uart_base_addr;

static volatile uint8_t* get_reg(uintptr_t reg) {
  return (uint8_t*)(uart_base_addr + reg);
}

static uint8_t read(uintptr_t reg) {
  return *get_reg(reg);
}

static void write(uintptr_t reg, uint8_t value) {
  *get_reg(reg) = value;
}

void init_uart(uintptr_t base_addr) {
  uart_base_addr = base_addr;

  write(UART_IER, 0x00);
  write(UART_LCR, UART_LCR_BAUD_LATCH);
  write(UART_LCR, UART_LCR_EIGHT_BITS);
  write(UART_FCR, UART_FCR_FIFO_ENABLE);
  write(UART_MCR, 0x00);
  read(UART_LSR);
  read(UART_RBR);
  write(UART_SCR, 0x00);
}

void uart_putc(int ch) {
  while ((read(UART_LSR) & UART_LSR_THRE) == 0) {
    // Busy wait.
  }
  write(UART_THR, ch);
}

int uart_getc(void) {
  if (read(UART_LSR) & UART_LSR_DR) {
    return read(UART_RBR);
  }
  return -1;
}
