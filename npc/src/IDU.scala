package npc

import chisel3._
import chisel3.util._

/** Instruction decode unit for the RV32E instruction subset. */
class IDU extends Module {
  val io = IO(new Bundle {
    val pc   = Input(UInt(32.W))
    val inst = Input(UInt(32.W))

    val decoded = Output(new DecodedInstruction)
  })

  val opcode = io.inst(6, 0)
  val rd     = io.inst(11, 7)
  val funct3 = io.inst(14, 12)
  val rs1    = io.inst(19, 15)
  val rs2    = io.inst(24, 20)
  val funct7 = io.inst(31, 25)

  val immI = Cat(Fill(20, io.inst(31)), io.inst(31, 20))
  val immS = Cat(Fill(20, io.inst(31)), io.inst(31, 25), io.inst(11, 7))
  val immB = Cat(Fill(19, io.inst(31)), io.inst(31), io.inst(7), io.inst(30, 25), io.inst(11, 8), 0.U(1.W))
  val immU = Cat(io.inst(31, 12), 0.U(12.W))
  val immJ = Cat(Fill(11, io.inst(31)), io.inst(31), io.inst(19, 12), io.inst(20), io.inst(30, 21), 0.U(1.W))

  val decoded = WireDefault(0.U.asTypeOf(new DecodedInstruction))
  decoded.pc              := io.pc
  decoded.inst            := io.inst
  decoded.rs1Index        := rs1
  decoded.rs2Index        := rs2
  decoded.rdIndex         := rd
  decoded.funct3          := funct3
  decoded.immI            := immI
  decoded.immS            := immS
  decoded.immB            := immB
  decoded.immJ            := immJ
  decoded.aluOp           := AluOp.Add
  decoded.writebackSource := WritebackSource.Alu
  decoded.branchOp        := BranchOp.None
  decoded.csrOp           := CsrOp.None
  decoded.illegal         := true.B

  when(opcode === "b0110111".U) { // LUI
    decoded.illegal          := false.B
    decoded.writesRd         := true.B
    decoded.writeGpr         := true.B
    decoded.useImmAsAluRight := true.B
    decoded.aluImm           := immU
    decoded.aluOp            := AluOp.CopyB
  }.elsewhen(opcode === "b0010111".U) { // AUIPC
    decoded.illegal          := false.B
    decoded.writesRd         := true.B
    decoded.writeGpr         := true.B
    decoded.usePcAsAluLeft   := true.B
    decoded.useImmAsAluRight := true.B
    decoded.aluImm           := immU
  }.elsewhen(opcode === "b1101111".U) { // JAL
    decoded.illegal         := false.B
    decoded.writesRd        := true.B
    decoded.writeGpr        := true.B
    decoded.writebackSource := WritebackSource.PcPlus4
    decoded.isJal           := true.B
  }.elsewhen(opcode === "b1100111".U) { // JALR
    decoded.usesRs1         := true.B
    decoded.writesRd        := true.B
    decoded.writeGpr        := true.B
    decoded.writebackSource := WritebackSource.PcPlus4
    when(funct3 === 0.U) {
      decoded.illegal := false.B
      decoded.isJalr  := true.B
    }
  }.elsewhen(opcode === "b1100011".U) { // conditional branch
    decoded.usesRs1 := true.B
    decoded.usesRs2 := true.B
    switch(funct3) {
      is("b000".U) { decoded.branchOp := BranchOp.Eq; decoded.illegal := false.B }
      is("b001".U) { decoded.branchOp := BranchOp.Ne; decoded.illegal := false.B }
      is("b100".U) { decoded.branchOp := BranchOp.Lt; decoded.illegal := false.B }
      is("b101".U) { decoded.branchOp := BranchOp.Ge; decoded.illegal := false.B }
      is("b110".U) { decoded.branchOp := BranchOp.Ltu; decoded.illegal := false.B }
      is("b111".U) { decoded.branchOp := BranchOp.Geu; decoded.illegal := false.B }
    }
  }.elsewhen(opcode === "b0000011".U) { // LOAD
    decoded.usesRs1          := true.B
    decoded.writesRd         := true.B
    decoded.writeGpr         := true.B
    decoded.useImmAsAluRight := true.B
    decoded.aluImm           := immI
    decoded.isLoad           := true.B
    when(
      funct3 === "b000".U || funct3 === "b001".U || funct3 === "b010".U ||
        funct3 === "b100".U || funct3 === "b101".U
    ) {
      decoded.illegal := false.B
    }
  }.elsewhen(opcode === "b0100011".U) { // STORE
    decoded.usesRs1          := true.B
    decoded.usesRs2          := true.B
    decoded.useImmAsAluRight := true.B
    decoded.aluImm           := immS
    decoded.isStore          := true.B
    when(funct3 === "b000".U || funct3 === "b001".U || funct3 === "b010".U) {
      decoded.illegal := false.B
    }
  }.elsewhen(opcode === "b0010011".U) { // OP-IMM
    decoded.usesRs1          := true.B
    decoded.writesRd         := true.B
    decoded.writeGpr         := true.B
    decoded.useImmAsAluRight := true.B
    decoded.aluImm           := immI
    switch(funct3) {
      is("b000".U) { decoded.aluOp := AluOp.Add; decoded.illegal := false.B }
      is("b010".U) { decoded.aluOp := AluOp.Slt; decoded.illegal := false.B }
      is("b011".U) { decoded.aluOp := AluOp.Sltu; decoded.illegal := false.B }
      is("b100".U) { decoded.aluOp := AluOp.Xor; decoded.illegal := false.B }
      is("b110".U) { decoded.aluOp := AluOp.Or; decoded.illegal := false.B }
      is("b111".U) { decoded.aluOp := AluOp.And; decoded.illegal := false.B }
      is("b001".U) {
        decoded.aluOp   := AluOp.Sll
        decoded.aluImm  := Cat(0.U(27.W), io.inst(24, 20))
        decoded.illegal := funct7 =/= 0.U
      }
      is("b101".U) {
        decoded.aluImm := Cat(0.U(27.W), io.inst(24, 20))
        when(funct7 === 0.U) {
          decoded.aluOp   := AluOp.Srl
          decoded.illegal := false.B
        }.elsewhen(funct7 === "b0100000".U) {
          decoded.aluOp   := AluOp.Sra
          decoded.illegal := false.B
        }
      }
    }
  }.elsewhen(opcode === "b0110011".U) { // OP
    decoded.usesRs1  := true.B
    decoded.usesRs2  := true.B
    decoded.writesRd := true.B
    decoded.writeGpr := true.B
    switch(funct3) {
      is("b000".U) {
        when(funct7 === 0.U) {
          decoded.aluOp   := AluOp.Add
          decoded.illegal := false.B
        }.elsewhen(funct7 === "b0100000".U) {
          decoded.aluOp   := AluOp.Sub
          decoded.illegal := false.B
        }
      }
      is("b001".U) { decoded.aluOp := AluOp.Sll; decoded.illegal := funct7 =/= 0.U }
      is("b010".U) { decoded.aluOp := AluOp.Slt; decoded.illegal := funct7 =/= 0.U }
      is("b011".U) { decoded.aluOp := AluOp.Sltu; decoded.illegal := funct7 =/= 0.U }
      is("b100".U) { decoded.aluOp := AluOp.Xor; decoded.illegal := funct7 =/= 0.U }
      is("b101".U) {
        when(funct7 === 0.U) {
          decoded.aluOp   := AluOp.Srl
          decoded.illegal := false.B
        }.elsewhen(funct7 === "b0100000".U) {
          decoded.aluOp   := AluOp.Sra
          decoded.illegal := false.B
        }
      }
      is("b110".U) { decoded.aluOp := AluOp.Or; decoded.illegal := funct7 =/= 0.U }
      is("b111".U) { decoded.aluOp := AluOp.And; decoded.illegal := funct7 =/= 0.U }
    }
  }.elsewhen(opcode === "b0001111".U) { // FENCE and FENCE.I
    when(funct3 === "b000".U || funct3 === "b001".U) {
      decoded.illegal := false.B
    }
  }.elsewhen(opcode === "b1110011".U) { // SYSTEM / Zicsr
    decoded.csrAddress := io.inst(31, 20)
    when(funct3 === "b000".U) {
      when(io.inst === "h00000073".U) { // ECALL from M-mode
        decoded.illegal := false.B
        decoded.isEcall := true.B
      }.elsewhen(io.inst === "h00100073".U) { // EBREAK
        decoded.illegal  := false.B
        decoded.isEbreak := true.B
      }.elsewhen(io.inst === "h30200073".U) { // MRET
        decoded.illegal := false.B
        decoded.isMret  := true.B
      }
    }.elsewhen(funct3 === "b001".U || funct3 === "b010".U || funct3 === "b011".U) {
      decoded.illegal         := false.B
      decoded.usesRs1         := true.B
      decoded.writesRd        := true.B
      decoded.writeGpr        := true.B
      decoded.writebackSource := WritebackSource.CsrRead
      decoded.isCsr           := true.B
      decoded.csrWriteEnable  := funct3 === "b001".U || rs1 =/= 0.U
      when(funct3 === "b001".U) { decoded.csrOp := CsrOp.Rw }
      when(funct3 === "b010".U) { decoded.csrOp := CsrOp.Rs }
      when(funct3 === "b011".U) { decoded.csrOp := CsrOp.Rc }
    }.elsewhen(funct3 === "b101".U || funct3 === "b110".U || funct3 === "b111".U) {
      decoded.illegal         := false.B
      decoded.writesRd        := true.B
      decoded.writeGpr        := true.B
      decoded.writebackSource := WritebackSource.CsrRead
      decoded.isCsr           := true.B
      decoded.csrUseImm       := true.B
      decoded.csrWriteEnable  := funct3 === "b101".U || rs1 =/= 0.U
      when(funct3 === "b101".U) { decoded.csrOp := CsrOp.Rw }
      when(funct3 === "b110".U) { decoded.csrOp := CsrOp.Rs }
      when(funct3 === "b111".U) { decoded.csrOp := CsrOp.Rc }
    }
  }

  io.decoded := decoded
}
