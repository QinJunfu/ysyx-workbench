package npc

/** CPU top module for student ID 24100022.
  *
  * This is the single top module emitted for every platform: the Direct NPC simulation testbench instantiates it by
  * this name, and ysyxSoC instantiates the same name through its BlackBox wrapper. The platforms differ only in
  * constructor arguments, never in the module name: the reset vector comes from the Makefile's `--reset-pc`, and
  * `useNarrowAddresses` is enabled only for ysyxSoC, which drives the CPU with a narrow physical address space.
  */
class ysyx_24100022(
  resetPc:            BigInt = BigInt("20000000", 16),
  useNarrowAddresses: Boolean = false)
    extends NPC(resetPc = resetPc, useNarrowAddresses = useNarrowAddresses)
