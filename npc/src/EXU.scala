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

  val aluLeft   = Mux(io.decoded.usePcAsAluLeft, io.decoded.pc, io.rs1Data)
  val aluRight  = Mux(io.decoded.useImmAsAluRight, io.decoded.aluImm, io.rs2Data)
  val aluResult = WireDefault(0.U(32.W))
  switch(io.decoded.aluOp) {
    is(AluOp.Add) { aluResult := aluLeft + aluRight }
    is(AluOp.Sub) { aluResult := aluLeft - aluRight }
    is(AluOp.Sll) { aluResult := (aluLeft << aluRight(4, 0))(31, 0) }
    is(AluOp.Slt) { aluResult := (aluLeft.asSInt < aluRight.asSInt).asUInt }
    is(AluOp.Sltu) { aluResult := (aluLeft < aluRight).asUInt }
    is(AluOp.Xor) { aluResult := aluLeft ^ aluRight }
    is(AluOp.Srl) { aluResult := aluLeft >> aluRight(4, 0) }
    is(AluOp.Sra) { aluResult := (aluLeft.asSInt >> aluRight(4, 0)).asUInt }
    is(AluOp.Or) { aluResult := aluLeft | aluRight }
    is(AluOp.And) { aluResult := aluLeft & aluRight }
    is(AluOp.CopyB) { aluResult := aluRight }
  }

  val takeBranch = WireDefault(false.B)
  switch(io.decoded.branchOp) {
    is(BranchOp.Eq) { takeBranch := io.rs1Data === io.rs2Data }
    is(BranchOp.Ne) { takeBranch := io.rs1Data =/= io.rs2Data }
    is(BranchOp.Lt) { takeBranch := io.rs1Data.asSInt < io.rs2Data.asSInt }
    is(BranchOp.Ge) { takeBranch := io.rs1Data.asSInt >= io.rs2Data.asSInt }
    is(BranchOp.Ltu) { takeBranch := io.rs1Data < io.rs2Data }
    is(BranchOp.Geu) { takeBranch := io.rs1Data >= io.rs2Data }
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
