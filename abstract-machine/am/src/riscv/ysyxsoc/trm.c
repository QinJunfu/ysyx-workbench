#include <am.h>
#include <klib-macros.h>
#include <riscv/riscv.h>
#include <ysyxsoc.h>

extern char __heap_start;
extern char __heap_end;
int main(const char *args);

Area heap = RANGE(&__heap_start, &__heap_end);
static const char mainargs[MAINARGS_MAX_LEN] = TOSTRING(MAINARGS_PLACEHOLDER);

void ysyxsoc_uart_init(void) {
  /* A non-zero divisor is required before the transmitter can make progress. */
  outb(YSYXSOC_UART_LCR, YSYXSOC_UART_LCR_DLAB | YSYXSOC_UART_LCR_8N1);
  outb(YSYXSOC_UART_DLM, 0);
  outb(YSYXSOC_UART_DLL, 1);
  outb(YSYXSOC_UART_LCR, YSYXSOC_UART_LCR_8N1);
  outb(YSYXSOC_UART_IER, 0);
}

void putch(char ch) {
  while ((inb(YSYXSOC_UART_LSR) & YSYXSOC_UART_LSR_THRE) == 0) {}
  outb(YSYXSOC_UART_THR, (uint8_t)ch);
}

void halt(int code) {
  asm volatile("mv a0, %0\n\tebreak" : : "r"(code) : "a0", "memory");
  while (1) {}
}

static void put_unsigned(uint32_t value) {
  char digits[10];
  unsigned int count = 0;

  do {
    digits[count++] = '0' + value % 10;
    value /= 10;
  } while (value != 0);

  while (count != 0) putch(digits[--count]);
}

static void print_machine_id(void) {
  uint32_t mvendorid;
  uint32_t marchid;

  asm volatile("csrr %0, mvendorid" : "=r"(mvendorid));
  asm volatile("csrr %0, marchid" : "=r"(marchid));
  putch((char)(mvendorid >> 24));
  putch((char)(mvendorid >> 16));
  putch((char)(mvendorid >> 8));
  putch((char)mvendorid);
  putch('_');
  put_unsigned(marchid);
  putch('\n');
}

void _trm_init(void) {
  ysyxsoc_uart_init();
  print_machine_id();
  halt(main(mainargs));
}
