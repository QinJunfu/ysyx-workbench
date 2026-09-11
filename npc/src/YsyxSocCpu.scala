package npc

/** ysyxSoC CPU boundary for student ID 24100022.
  *
  * NPC already exposes exactly the interface that ysyxSoC/spec/cpu-interface.md prescribes (clock, reset,
  * io_interrupt, a full AXI4 master and a full AXI4 slave), so the SoC top is that same module under the module name
  * the SoC expects. The only SoC-specific detail is the wider physical address space and the MROM reset vector.
  */
class ysyx_24100022(resetPc: BigInt = BigInt("20000000", 16))
    extends NPC(resetPc = resetPc, useNarrowAddresses = true)
