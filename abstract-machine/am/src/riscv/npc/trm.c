#include <am.h>
#include <klib-macros.h>
#include <riscv/riscv.h>

extern char _heap_start;
int main(const char *args);

extern char _pmem_start;
#define PMEM_SIZE (128 * 1024 * 1024)
#define PMEM_END  ((uintptr_t)&_pmem_start + PMEM_SIZE)

Area heap = RANGE(&_heap_start, PMEM_END);
static const char mainargs[MAINARGS_MAX_LEN] = TOSTRING(MAINARGS_PLACEHOLDER); // defined in CFLAGS

void putch(char ch) {
  outb(0x10000000, ch);
}

void halt(int code) {
  // The simulator reads a0 when the ebreak retires, matching the AM trap ABI.
  asm volatile("mv a0, %0\n\tebreak" : : "r"(code) : "a0", "memory");
  while (1);
}

void _trm_init() {
  int ret = main(mainargs);
  halt(ret);
}
