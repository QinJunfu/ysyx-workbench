#include <stddef.h>
#include <stdint.h>

extern uint8_t __ssbl_lma_start[];
extern uint8_t __ssbl_lma_end[];
extern uint8_t __ssbl_vma_start[];

extern uint8_t __text_lma_start[];
extern uint8_t __text_vma_start[];
extern uint8_t __text_vma_end[];
extern uint8_t __rodata_lma_start[];
extern uint8_t __rodata_vma_start[];
extern uint8_t __rodata_vma_end[];
extern uint8_t __data_lma_start[];
extern uint8_t __data_vma_start[];
extern uint8_t __data_vma_end[];
extern uint8_t __bss_start[];
extern uint8_t __bss_end[];

extern uint8_t __data_extra_lma_start[];
extern uint8_t __data_extra_vma_start[];
extern uint8_t __data_extra_vma_end[];
extern uint8_t __bss_extra_start[];
extern uint8_t __bss_extra_end[];

__attribute__((section(".boot.fsbl.copy"), noinline))
static void boot_copy(uint8_t *dst, const uint8_t *src, uintptr_t size) {
  if (dst == src) return;

  while (size >= sizeof(uint32_t) &&
         (((uintptr_t)dst | (uintptr_t)src) & (sizeof(uint32_t) - 1)) == 0) {
    *(volatile uint32_t *)dst = *(const volatile uint32_t *)src;
    dst += sizeof(uint32_t);
    src += sizeof(uint32_t);
    size -= sizeof(uint32_t);
  }
  while (size-- != 0) *dst++ = *src++;
}

__attribute__((section(".boot.ssbl.copy"), noinline))
static void app_copy(uint8_t *dst, const uint8_t *src, uintptr_t size) {
  if (dst == src) return;

  while (size >= sizeof(uint32_t) &&
         (((uintptr_t)dst | (uintptr_t)src) & (sizeof(uint32_t) - 1)) == 0) {
    *(volatile uint32_t *)dst = *(const volatile uint32_t *)src;
    dst += sizeof(uint32_t);
    src += sizeof(uint32_t);
    size -= sizeof(uint32_t);
  }
  while (size-- != 0) *dst++ = *src++;
}

__attribute__((section(".boot.ssbl.zero"), noinline))
static void app_zero(uint8_t *dst, uintptr_t size) {
  while (size >= sizeof(uint32_t) &&
         ((uintptr_t)dst & (sizeof(uint32_t) - 1)) == 0) {
    *(volatile uint32_t *)dst = 0;
    dst += sizeof(uint32_t);
    size -= sizeof(uint32_t);
  }
  while (size-- != 0) *dst++ = 0;
}

__attribute__((section(".boot.fsbl")))
void __am_boot_load_ssbl(void) {
  uintptr_t size = (uintptr_t)__ssbl_lma_end - (uintptr_t)__ssbl_lma_start;
  boot_copy(__ssbl_vma_start, __ssbl_lma_start, size);
}

__attribute__((section(".boot.ssbl.helpers"), noinline))
static void load_section(uint8_t *vma_start, uint8_t *vma_end,
                         const uint8_t *lma_start) {
  uintptr_t size = (uintptr_t)vma_end - (uintptr_t)vma_start;
  app_copy(vma_start, lma_start, size);
}

__attribute__((section(".boot.ssbl.helpers"), noinline))
static void zero_section(uint8_t *start, uint8_t *end) {
  uintptr_t size = (uintptr_t)end - (uintptr_t)start;
  app_zero(start, size);
}

__attribute__((section(".boot.ssbl")))
void __am_boot_load_app(void) {
  load_section(__text_vma_start, __text_vma_end, __text_lma_start);
  load_section(__rodata_vma_start, __rodata_vma_end, __rodata_lma_start);

  if ((uintptr_t)__data_extra_vma_end > (uintptr_t)__data_extra_vma_start) {
    load_section(__data_extra_vma_start, __data_extra_vma_end,
                 __data_extra_lma_start);
  }
  load_section(__data_vma_start, __data_vma_end, __data_lma_start);

  if ((uintptr_t)__bss_extra_end > (uintptr_t)__bss_extra_start) {
    zero_section(__bss_extra_start, __bss_extra_end);
  }
  zero_section(__bss_start, __bss_end);
}
