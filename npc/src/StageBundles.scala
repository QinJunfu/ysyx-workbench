package npc

import chisel3._

/** ALU operations selected by IDU and executed by EXU. */
object AluOp {
  val Width = 4
  val Add   = 0.U(Width.W)
  val Sub   = 1.U(Width.W)
  val Sll   = 2.U(Width.W)
  val Slt   = 3.U(Width.W)
  val Sltu  = 4.U(Width.W)
  val Xor   = 5.U(Width.W)
  val Srl   = 6.U(Width.W)
  val Sra   = 7.U(Width.W)
  val Or    = 8.U(Width.W)
  val And   = 9.U(Width.W)
  val CopyB = 10.U(Width.W)
}

/** The source EXU provides to WBU for a non-load GPR write. */
object WritebackSource {
  val Width   = 2
  val Alu     = 0.U(Width.W)
  val PcPlus4 = 1.U(Width.W)
  val CsrRead = 2.U(Width.W)
}

/** Conditional branch comparison selected by IDU. */
object BranchOp {
  val Width = 3
  val None  = 0.U(Width.W)
  val Eq    = 1.U(Width.W)
  val Ne    = 2.U(Width.W)
  val Lt    = 3.U(Width.W)
  val Ge    = 4.U(Width.W)
  val Ltu   = 5.U(Width.W)
  val Geu   = 6.U(Width.W)
}

/** CSR read-modify-write operation selected by IDU. */
object CsrOp {
  val Width = 3
  val None  = 0.U(Width.W)
  val Rw    = 1.U(Width.W)
  val Rs    = 2.U(Width.W)
  val Rc    = 3.U(Width.W)
}

/** A decoded instruction travels through the single-cycle combinational path. */
class DecodedInstruction extends Bundle {
  val pc         = UInt(32.W)
  val inst       = UInt(32.W)
  val rs1Index   = UInt(5.W)
  val rs2Index   = UInt(5.W)
  val rdIndex    = UInt(5.W)
  val funct3     = UInt(3.W)
  val immI       = UInt(32.W)
  val immS       = UInt(32.W)
  val immB       = UInt(32.W)
  val immJ       = UInt(32.W)
  val aluImm     = UInt(32.W)
  val csrAddress = UInt(12.W)

  val usesRs1  = Bool()
  val usesRs2  = Bool()
  val writesRd = Bool()
  val writeGpr = Bool()
  val illegal  = Bool()

  val usePcAsAluLeft   = Bool()
  val useImmAsAluRight = Bool()
  val aluOp            = UInt(AluOp.Width.W)
  val writebackSource  = UInt(WritebackSource.Width.W)

  val branchOp = UInt(BranchOp.Width.W)
  val isJal    = Bool()
  val isJalr   = Bool()
  val isLoad   = Bool()
  val isStore  = Bool()
  val isEcall  = Bool()
  val isEbreak = Bool()
  val isMret   = Bool()

  val isCsr          = Bool()
  val csrOp          = UInt(CsrOp.Width.W)
  val csrUseImm      = Bool()
  val csrWriteEnable = Bool()
}

/** EXU results consumed by LSU and WBU in the same clock cycle. */
class ExuResult extends Bundle {
  val nextPc        = UInt(32.W)
  val aluResult     = UInt(32.W)
  val writebackData = UInt(32.W)
  val memoryAddr    = UInt(32.W)
  val csrWriteData  = UInt(32.W)
  val illegal       = Bool()
}

/** LSU's physical bus requests and logical load result. */
class LsuResult extends Bundle {
  val alignedAddress = UInt(32.W)
  val address2       = UInt(32.W)
  val read0Valid     = Bool()
  val read1Valid     = Bool()
  val write0Valid    = Bool()
  val write0Data     = UInt(32.W)
  val write0Mask     = UInt(4.W)
  val write1Valid    = Bool()
  val write1Data     = UInt(32.W)
  val write1Mask     = UInt(4.W)
  val loadData       = UInt(32.W)
  val logicalMask    = UInt(4.W)
}

/** One architectural retirement event exported by WBU. */
class RetireInfo extends Bundle {
  val valid  = Bool()
  val pc     = UInt(32.W)
  val inst   = UInt(32.W)
  val nextPc = UInt(32.W)
  val gprs   = Vec(16, UInt(32.W))

  val memTraceValid = Bool()
  val memTraceWrite = Bool()
  val memTraceAddr  = UInt(32.W)
  val memTraceData  = UInt(32.W)
  val memTraceMask  = UInt(4.W)

  val halt     = Bool()
  val haltCode = UInt(32.W)
  val invalid  = Bool()
}
