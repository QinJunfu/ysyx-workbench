.section .boot.entry, "ax"
.globl _start_fsbl
.type _start_fsbl, @function

_start_fsbl:
  mv s0, zero
  la sp, __fsbl_stack_pointer
  call __am_boot_load_ssbl
  fence.i
  la t0, _start_ssbl
  jr t0

.size _start_fsbl, . - _start_fsbl

.section .boot.ssbl, "ax"
.globl _start_ssbl
.type _start_ssbl, @function

_start_ssbl:
  la sp, __ssbl_stack_pointer
  call __am_boot_load_app
  fence.i

  .option push
  .option norelax
  la gp, __global_pointer$
  .option pop
  la sp, __stack_pointer
  tail _trm_init

.size _start_ssbl, . - _start_ssbl
