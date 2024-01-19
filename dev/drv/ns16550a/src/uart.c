#include "uart.h"

static uintptr_t uart_base_addr;
static uint32_t  uart_freq;
static uint32_t  uart_baud;
static uint32_t  uart_shift;
static uint32_t  uart_width;

static volatile uint8_t* get_reg(uintptr_t reg) {
  uint32_t offset = reg << uart_shift;
  return (uint8_t*)(uart_base_addr + offset);
}

static uint8_t read8(const volatile void* addr) {
  uint8_t value;
  asm volatile("lb %0, 0(%1)" : "=r"(value) : "r"(addr));
  asm volatile("fence i,r" : : : "memory");
  return value;
}

static uint8_t read16(const volatile void* addr) {
  uint16_t value;
  asm volatile("lh %0, 0(%1)" : "=r"(value) : "r"(addr));
  asm volatile("fence i,r" : : : "memory");
  return value;
}

static uint8_t read32(const volatile void* addr) {
  uint32_t value;
  asm volatile("lw %0, 0(%1)" : "=r"(value) : "r"(addr));
  asm volatile("fence i,r" : : : "memory");
  return value;
}

static void write8(volatile void* addr, uint8_t value) {
  asm volatile("fence w,o" : : : "memory");
  asm volatile("sb %0, 0(%1)" : : "r"(value), "r"(addr));
}

static void write16(volatile void* addr, uint16_t value) {
  asm volatile("fence w,o" : : : "memory");
  asm volatile("sh %0, 0(%1)" : : "r"(value), "r"(addr));
}

static void write32(volatile void* addr, uint32_t value) {
  asm volatile("fence w,o" : : : "memory");
  asm volatile("sw %0, 0(%1)" : : "r"(value), "r"(addr));
}

static uint8_t read(uintptr_t reg) {
  if (uart_width == 1) {
    return read8(get_reg(reg));
  } else if (uart_width == 2) {
    return (uint8_t)read16(get_reg(reg));
  } else if (uart_width == 4) {
    return (uint8_t)read32(get_reg(reg));
  } else {
    return -1;
  }
}

static void write(uintptr_t reg, uint8_t value) {
  if (uart_width == 1) {
    write8(get_reg(reg), value);
  } else if (uart_width == 2) {
    write16(get_reg(reg), value);
  } else if (uart_width == 4) {
    write32(get_reg(reg), value);
  }
}

void init_uart(uintptr_t base_addr, uint32_t frequency, uint32_t baudrate, uint32_t reg_shift, uint32_t reg_width, uint32_t reg_offset) {
  uart_base_addr = base_addr + reg_offset;
  uart_freq      = frequency;
  uart_baud      = baudrate;
  uart_shift     = reg_shift;
  uart_width     = reg_width;

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
