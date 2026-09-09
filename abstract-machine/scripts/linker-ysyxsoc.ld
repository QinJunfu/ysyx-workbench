#ifndef YSYX_ROM_REGION
#define YSYX_ROM_REGION mrom
#endif

#ifndef YSYX_RAM_REGION
#define YSYX_RAM_REGION sram
#endif

#ifndef YSYX_XIP
#define YSYX_XIP 0
#endif

#ifndef YSYX_STACK_SIZE
#define YSYX_STACK_SIZE 0x800
#endif

#ifndef YSYXSOC_SDRAM_SIZE
#define YSYXSOC_SDRAM_SIZE 0x08000000
#endif

ENTRY(_start_fsbl)

MEMORY {
  mrom  (rx)  : ORIGIN = 0x20000000, LENGTH = 4K
  flash (rx)  : ORIGIN = 0x30000000, LENGTH = 16M
  sram  (rwx) : ORIGIN = 0x0f000000, LENGTH = 8K
  psram (rwx) : ORIGIN = 0x80000000, LENGTH = 4M
  sdram (rwx) : ORIGIN = 0xa0000000, LENGTH = YSYXSOC_SDRAM_SIZE
}

/* These aliases are the stable interface used by additional linker scripts. */
REGION_ALIAS("ROM", YSYX_ROM_REGION);
REGION_ALIAS("RAM", YSYX_RAM_REGION);
REGION_ALIAS("BOOT_RAM", sram);

SECTIONS {
  .boot.entry : ALIGN(4) {
    KEEP(*(.boot.entry))
  } > ROM

  .boot.fsbl : ALIGN(4) {
    KEEP(*(.boot.fsbl))
    KEEP(*(.boot.fsbl.*))
  } > ROM

#if YSYX_XIP
  .text : ALIGN(4) {
    __text_vma_start = .;
    *(.text .text.*)
    __text_vma_end = .;
  } > ROM
  __text_lma_start = LOADADDR(.text);
  __text_lma_end = LOADADDR(.text) + SIZEOF(.text);

  .rodata : ALIGN(4) {
    __rodata_vma_start = .;
    *(.rodata .rodata.* .srodata .srodata.*)
    __rodata_vma_end = .;
  } > ROM
  __rodata_lma_start = LOADADDR(.rodata);
  __rodata_lma_end = LOADADDR(.rodata) + SIZEOF(.rodata);
#endif

  .boot.ssbl : ALIGN(4) {
    __ssbl_vma_start = .;
    KEEP(*(.boot.ssbl))
    KEEP(*(.boot.ssbl.*))
    __ssbl_vma_end = .;
  } > BOOT_RAM AT> ROM
  __ssbl_lma_start = LOADADDR(.boot.ssbl);
  __ssbl_lma_end = LOADADDR(.boot.ssbl) + SIZEOF(.boot.ssbl);

#if !YSYX_XIP
  .text : ALIGN(4) {
    __text_vma_start = .;
    *(.text .text.*)
    __text_vma_end = .;
  } > RAM AT> ROM
  __text_lma_start = LOADADDR(.text);
  __text_lma_end = LOADADDR(.text) + SIZEOF(.text);

  .rodata : ALIGN(4) {
    __rodata_vma_start = .;
    *(.rodata .rodata.* .srodata .srodata.*)
    __rodata_vma_end = .;
  } > RAM AT> ROM
  __rodata_lma_start = LOADADDR(.rodata);
  __rodata_lma_end = LOADADDR(.rodata) + SIZEOF(.rodata);
#endif

  .data.extra : ALIGN(8) {
    __data_extra_vma_start = .;

    __fsymtab_start = .;
    KEEP(*(FSymTab))
    __fsymtab_end = .;
    . = ALIGN(8);

    __vsymtab_start = .;
    KEEP(*(VSymTab))
    __vsymtab_end = .;
    . = ALIGN(8);

    __rt_init_start = .;
    KEEP(*(SORT(.rti_fn*)))
    __rt_init_end = .;
    . = ALIGN(8);

    __rt_utest_tc_tab_start = .;
    KEEP(*(UtestTcTab))
    __rt_utest_tc_tab_end = .;
    . = ALIGN(8);

    __am_apps_data_start = .;
    *(__am_apps.data*)
    *(__am_apps.sdata*)
    __am_apps_data_end = .;
    . = ALIGN(8);

    __data_extra_vma_end = .;
  } > RAM AT> ROM
  __data_extra_lma_start = LOADADDR(.data.extra);
  __data_extra_lma_end = LOADADDR(.data.extra) + SIZEOF(.data.extra);

  .data : ALIGN(4) {
    __data_vma_start = .;
    PROVIDE(__global_pointer$ = . + 0x800);
    *(.data .data.* .sdata .sdata.*)
    *(.init_array .init_array.*)
    *(.fini_array .fini_array.*)
    __data_vma_end = .;
  } > RAM AT> ROM
  __data_lma_start = LOADADDR(.data);
  __data_lma_end = LOADADDR(.data) + SIZEOF(.data);

  .bss.extra (NOLOAD) : ALIGN(8) {
    __bss_extra_start = .;
    __am_apps_bss_start = .;
    *(__am_apps.bss*)
    *(__am_apps.sbss*)
    *(__am_apps.scommon*)
    __am_apps_bss_end = .;
    . = ALIGN(8);
    __bss_extra_end = .;
  } > RAM

  .bss (NOLOAD) : ALIGN(8) {
    __bss_start = .;
    *(.bss .bss.* .sbss .sbss.*)
    *(COMMON)
    __bss_end = .;
  } > RAM

  .heap_stack (NOLOAD) : ALIGN(16) {
    __heap_start = .;
    . = ORIGIN(RAM) + LENGTH(RAM) - YSYX_STACK_SIZE;
    __heap_end = .;
    __stack_bottom = .;
    . += YSYX_STACK_SIZE;
    __stack_pointer = .;
  } > RAM

  __fsbl_stack_pointer = ORIGIN(BOOT_RAM) + LENGTH(BOOT_RAM);
  __ssbl_stack_pointer = ORIGIN(BOOT_RAM) + LENGTH(BOOT_RAM);
  __ysyxsoc_sdram_size = LENGTH(sdram);

  PROVIDE(_ssbl_vma_start = __ssbl_vma_start);
  PROVIDE(_ssbl_vma_end = __ssbl_vma_end);
  PROVIDE(_ssbl_lma_start = __ssbl_lma_start);
  PROVIDE(_ssbl_lma_end = __ssbl_lma_end);
  PROVIDE(_text_vma_start = __text_vma_start);
  PROVIDE(_text_vma_end = __text_vma_end);
  PROVIDE(_text_lma_start = __text_lma_start);
  PROVIDE(_rodata_vma_start = __rodata_vma_start);
  PROVIDE(_rodata_vma_end = __rodata_vma_end);
  PROVIDE(_rodata_lma_start = __rodata_lma_start);
  PROVIDE(_data_vma_start = __data_vma_start);
  PROVIDE(_data_vma_end = __data_vma_end);
  PROVIDE(_data_lma_start = __data_lma_start);
  PROVIDE(_bss_start = __bss_start);
  PROVIDE(_bss_end = __bss_end);
  PROVIDE(_heap_start = __heap_start);
  PROVIDE(_heap_end = __heap_end);
  PROVIDE(_stack_pointer = __stack_pointer);
  PROVIDE(_fsbl_stack_pointer = __fsbl_stack_pointer);
  PROVIDE(_ssbl_stack_pointer = __ssbl_stack_pointer);
  PROVIDE(_data_extra_vma_start = __data_extra_vma_start);
  PROVIDE(_data_extra_vma_end = __data_extra_vma_end);
  PROVIDE(_data_extra_lma_start = __data_extra_lma_start);
  PROVIDE(_data_extra_lma_end = __data_extra_lma_end);
  PROVIDE(_bss_extra_start = __bss_extra_start);
  PROVIDE(_bss_extra_end = __bss_extra_end);

  etext = __text_vma_end;
  _etext = __text_vma_end;
  edata = __data_vma_end;
  _edata = __data_vma_end;
  end = __bss_end;
  _end = __bss_end;

  /DISCARD/ : {
    *(.comment)
    *(.eh_frame .eh_frame.*)
    *(.riscv.attributes)
  }
}

ASSERT(__heap_start <= __heap_end, "ysyxsoc RAM overflow: no room for heap/stack")
ASSERT(__stack_pointer <= ORIGIN(RAM) + LENGTH(RAM), "ysyxsoc stack exceeds RAM")
