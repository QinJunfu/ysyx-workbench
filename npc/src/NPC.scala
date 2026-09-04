package npc

/** Default RTL top instantiated by the SystemVerilog simulation wrapper. */
class NPC(resetPc: BigInt = BigInt("80000000", 16)) extends Rv32eCore(resetPc)
