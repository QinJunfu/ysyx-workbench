package npc

import chisel3._
import chisel3.util._

/** Execute unit for ALU operations, control flow, addresses, and CSR values. */
class EXU extends Module {
  val io = IO(new Bundle {
    val decoded = Input(new DecodedInstruction)
    val rs1Data = Input(UInt(32.W))
    val rs2Data = Input(UInt(32.W))

    val csrReadData  = Input(UInt(32.W))
    val csrSupported = Input(Bool())
    val mtvec        = Input(UInt(32.W))
    val mepc         = Input(UInt(32.W))

    val result = Output(new ExuResult)
  })

  val aluLeft  = Mux(io.decoded.usePcAsAluLeft, io.decoded.pc, io.rs1Data)
  val aluRight = Mux(io.decoded.useImmAsAluRight, io.decoded.aluImm, io.rs2Data)
  val shamt    = aluRight(4, 0)

  // A left shift is a right shift of the bit-reversed operand, so SLL, SRL and
  // SRA share one right-shift network instead of instantiating three separate
  // barrel shifters.  The arithmetic fill bit is selected inside the network,
  // which keeps the sign extension of SRA on the same hardware as the zero
  // fill of SRL.  The operand is reversed exactly when the opcode is SLL, so
  // only that path reverses the result back.
  val isShiftLeft  = io.decoded.aluOp === AluOp.Sll
  val isArithShift = io.decoded.aluOp === AluOp.Sra
  val shiftIn      = Mux(isShiftLeft, Reverse(aluLeft), aluLeft)
  // Selecting the fill bit once keeps the per-stage fill pure wiring.
  val fillBit      = isArithShift && shiftIn(31)

  val shiftNet = {
    var stage: UInt = shiftIn
    for (bit <- 0 until log2Ceil(32)) {
      val n       = 1 << bit
      val dropped = stage >> n
      stage = Mux(shamt(bit), Cat(Fill(n, fillBit), dropped), stage)
    }
    stage
  }

  // One set of comparisons serves both the ALU's SLT/SLTU and every branch.
  // IDU never selects the PC or an immediate for a conditional branch, so on a
  // branch instruction aluLeft/aluRight are exactly rs1Data/rs2Data; and
  // branchOp is None for every ALU operation, so takeBranch is forced low there
  // whatever these comparisons say.  Computing them once removes the duplicate
  // set of 32-bit comparators.
  val cmpEq  = aluLeft === aluRight
  val cmpLt  = aluLeft.asSInt < aluRight.asSInt
  val cmpLtu = aluLeft < aluRight

  val aluResult = WireDefault(0.U(32.W))
  switch(io.decoded.aluOp) {
    is(AluOp.Add) { aluResult := aluLeft + aluRight }
    is(AluOp.Sub) { aluResult := aluLeft - aluRight }
    is(AluOp.Sll) { aluResult := Reverse(shiftNet) }
    is(AluOp.Slt) { aluResult := cmpLt.asUInt }
    is(AluOp.Sltu) { aluResult := cmpLtu.asUInt }
    is(AluOp.Xor) { aluResult := aluLeft ^ aluRight }
    is(AluOp.Srl) { aluResult := shiftNet }
    is(AluOp.Sra) { aluResult := shiftNet }
    is(AluOp.Or) { aluResult := aluLeft | aluRight }
    is(AluOp.And) { aluResult := aluLeft & aluRight }
    is(AluOp.CopyB) { aluResult := aluRight }
  }

  // GE is the complement of LT and GEU the complement of LTU, so the negated
  // branch conditions reuse the same comparators.
  val takeBranch = WireDefault(false.B)
  switch(io.decoded.branchOp) {
    is(BranchOp.Eq) { takeBranch := cmpEq }
    is(BranchOp.Ne) { takeBranch := !cmpEq }
    is(BranchOp.Lt) { takeBranch := cmpLt }
    is(BranchOp.Ge) { takeBranch := !cmpLt }
    is(BranchOp.Ltu) { takeBranch := cmpLtu }
    is(BranchOp.Geu) { takeBranch := !cmpLtu }
  }

  val nextPc = WireDefault(io.decoded.pc + 4.U)
  when(takeBranch) {
    nextPc := io.decoded.pc + io.decoded.immB
  }.elsewhen(io.decoded.isJal) {
    nextPc := io.decoded.pc + io.decoded.immJ
  }.elsewhen(io.decoded.isJalr) {
    nextPc := (io.rs1Data + io.decoded.immI) & "hfffffffe".U(32.W)
  }.elsewhen(io.decoded.isEcall) {
    nextPc := io.mtvec
  }.elsewhen(io.decoded.isMret) {
    nextPc := io.mepc
  }

  val csrOperand   = Mux(io.decoded.csrUseImm, Cat(0.U(27.W), io.decoded.rs1Index), io.rs1Data)
  val csrWriteData = WireDefault(io.csrReadData)
  switch(io.decoded.csrOp) {
    is(CsrOp.Rw) { csrWriteData := csrOperand }
    is(CsrOp.Rs) { csrWriteData := io.csrReadData | csrOperand }
    is(CsrOp.Rc) { csrWriteData := io.csrReadData & ~csrOperand }
  }

  val writebackData = WireDefault(aluResult)
  switch(io.decoded.writebackSource) {
    is(WritebackSource.PcPlus4) { writebackData := io.decoded.pc + 4.U }
    is(WritebackSource.CsrRead) { writebackData := io.csrReadData }
  }

  io.result.nextPc        := nextPc
  io.result.aluResult     := aluResult
  io.result.writebackData := writebackData
  io.result.memoryAddr    := io.rs1Data + Mux(io.decoded.isStore, io.decoded.immS, io.decoded.immI)
  io.result.csrWriteData  := csrWriteData
  io.result.illegal       := io.decoded.illegal || (io.decoded.isCsr && !io.csrSupported)
}
