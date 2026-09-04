package npc

import chisel3._
import chisel3.util._

/** Decodes one RV32E instruction and produces its non-memory control signals. */
class InstructionUnit extends Module {
  val io = IO(new Bundle {
    val pc       = Input(UInt(32.W))
    val inst     = Input(UInt(32.W))
    val rs1Value = Input(UInt(32.W))
    val rs2Value = Input(UInt(32.W))
    val loadData = Input(UInt(32.W))

    val csrReadData  = Input(UInt(32.W))
    val csrSupported = Input(Bool())
    val mtvec        = Input(UInt(32.W))
    val mepc         = Input(UInt(32.W))

    val rs1Index = Output(UInt(5.W))
    val rs2Index = Output(UInt(5.W))
    val rdIndex  = Output(UInt(5.W))
    val funct3   = Output(UInt(3.W))

    val usesRs1   = Output(Bool())
    val usesRs2   = Output(Bool())
    val writesRd  = Output(Bool())
    val writeGpr  = Output(Bool())
    val writeData = Output(UInt(32.W))
    val nextPc    = Output(UInt(32.W))
    val illegal   = Output(Bool())

    val isLoad         = Output(Bool())
    val isStore        = Output(Bool())
    val memoryAddr     = Output(UInt(32.W))
    val isEbreak       = Output(Bool())
    val isEcall        = Output(Bool())
    val csrAddress     = Output(UInt(12.W))
    val csrWriteEnable = Output(Bool())
    val csrWriteData   = Output(UInt(32.W))
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

  val nextPc         = Wire(UInt(32.W))
  val writeGpr       = Wire(Bool())
  val writeData      = Wire(UInt(32.W))
  val usesRs1        = Wire(Bool())
  val usesRs2        = Wire(Bool())
  val writesRd       = Wire(Bool())
  val illegal        = Wire(Bool())
  val isLoad         = Wire(Bool())
  val isStore        = Wire(Bool())
  val isEbreak       = Wire(Bool())
  val isEcall        = Wire(Bool())
  val csrWriteEnable = Wire(Bool())
  val csrWriteData   = Wire(UInt(32.W))
  val memoryAddr     = Wire(UInt(32.W))

  nextPc         := io.pc + 4.U
  writeGpr       := false.B
  writeData      := 0.U(32.W)
  usesRs1        := false.B
  usesRs2        := false.B
  writesRd       := false.B
  illegal        := true.B
  isLoad         := false.B
  isStore        := false.B
  isEbreak       := false.B
  isEcall        := false.B
  csrWriteEnable := false.B
  csrWriteData   := 0.U(32.W)
  memoryAddr     := 0.U(32.W)

  when(opcode === "b0110111".U) { // LUI
    illegal   := false.B
    writesRd  := true.B
    writeGpr  := true.B
    writeData := immU
  }.elsewhen(opcode === "b0010111".U) { // AUIPC
    illegal   := false.B
    writesRd  := true.B
    writeGpr  := true.B
    writeData := io.pc + immU
  }.elsewhen(opcode === "b1101111".U) { // JAL
    illegal   := false.B
    writesRd  := true.B
    writeGpr  := true.B
    writeData := io.pc + 4.U
    nextPc    := io.pc + immJ
  }.elsewhen(opcode === "b1100111".U) { // JALR
    illegal   := false.B
    usesRs1   := true.B
    writesRd  := true.B
    writeGpr  := true.B
    writeData := io.pc + 4.U
    when(funct3 === 0.U) {
      nextPc := (io.rs1Value + immI) & "hfffffffe".U(32.W)
    }.otherwise {
      illegal := true.B
    }
  }.elsewhen(opcode === "b1100011".U) { // conditional branch
    val takeBranch  = Wire(Bool())
    val legalBranch = funct3 === "b000".U || funct3 === "b001".U || funct3 === "b100".U ||
      funct3 === "b101".U || funct3 === "b110".U || funct3 === "b111".U
    takeBranch := false.B
    illegal    := false.B
    usesRs1    := true.B
    usesRs2    := true.B
    switch(funct3) {
      is("b000".U) { takeBranch := io.rs1Value === io.rs2Value }
      is("b001".U) { takeBranch := io.rs1Value =/= io.rs2Value }
      is("b100".U) { takeBranch := io.rs1Value.asSInt < io.rs2Value.asSInt }
      is("b101".U) { takeBranch := io.rs1Value.asSInt >= io.rs2Value.asSInt }
      is("b110".U) { takeBranch := io.rs1Value < io.rs2Value }
      is("b111".U) { takeBranch := io.rs1Value >= io.rs2Value }
    }
    when(!legalBranch) {
      illegal := true.B
    }
    when(takeBranch) {
      nextPc := io.pc + immB
    }
  }.elsewhen(opcode === "b0000011".U) { // LOAD
    val legalLoad = funct3 === "b000".U || funct3 === "b001".U || funct3 === "b010".U ||
      funct3 === "b100".U || funct3 === "b101".U
    illegal    := false.B
    usesRs1    := true.B
    writesRd   := true.B
    writeGpr   := true.B
    isLoad     := true.B
    memoryAddr := io.rs1Value + immI
    when(!legalLoad) {
      illegal := true.B
    }
  }.elsewhen(opcode === "b0100011".U) { // STORE
    val legalStore = funct3 === "b000".U || funct3 === "b001".U || funct3 === "b010".U
    illegal    := false.B
    usesRs1    := true.B
    usesRs2    := true.B
    isStore    := true.B
    memoryAddr := io.rs1Value + immS
    when(!legalStore) {
      illegal := true.B
    }
  }.elsewhen(opcode === "b0010011".U) { // OP-IMM
    illegal  := false.B
    usesRs1  := true.B
    writesRd := true.B
    writeGpr := true.B
    switch(funct3) {
      is("b000".U) { writeData := io.rs1Value + immI }
      is("b010".U) { writeData := (io.rs1Value.asSInt < immI.asSInt).asUInt }
      is("b011".U) { writeData := (io.rs1Value < immI).asUInt }
      is("b100".U) { writeData := io.rs1Value ^ immI }
      is("b110".U) { writeData := io.rs1Value | immI }
      is("b111".U) { writeData := io.rs1Value & immI }
      is("b001".U) {
        when(funct7 === 0.U) {
          writeData := (io.rs1Value << io.inst(24, 20))(31, 0)
        }.otherwise {
          illegal := true.B
        }
      }
      is("b101".U) {
        when(funct7 === 0.U) {
          writeData := io.rs1Value >> io.inst(24, 20)
        }.elsewhen(funct7 === "b0100000".U) {
          writeData := (io.rs1Value.asSInt >> io.inst(24, 20)).asUInt
        }.otherwise {
          illegal := true.B
        }
      }
    }
  }.elsewhen(opcode === "b0110011".U) { // OP
    illegal  := false.B
    usesRs1  := true.B
    usesRs2  := true.B
    writesRd := true.B
    writeGpr := true.B
    switch(funct3) {
      is("b000".U) {
        when(funct7 === 0.U) {
          writeData := io.rs1Value + io.rs2Value
        }.elsewhen(funct7 === "b0100000".U) {
          writeData := io.rs1Value - io.rs2Value
        }.otherwise {
          illegal := true.B
        }
      }
      is("b001".U) {
        when(funct7 === 0.U) {
          writeData := (io.rs1Value << io.rs2Value(4, 0))(31, 0)
        }.otherwise {
          illegal := true.B
        }
      }
      is("b010".U) {
        when(funct7 === 0.U) {
          writeData := (io.rs1Value.asSInt < io.rs2Value.asSInt).asUInt
        }.otherwise {
          illegal := true.B
        }
      }
      is("b011".U) {
        when(funct7 === 0.U) {
          writeData := (io.rs1Value < io.rs2Value).asUInt
        }.otherwise {
          illegal := true.B
        }
      }
      is("b100".U) {
        when(funct7 === 0.U) {
          writeData := io.rs1Value ^ io.rs2Value
        }.otherwise {
          illegal := true.B
        }
      }
      is("b101".U) {
        when(funct7 === 0.U) {
          writeData := io.rs1Value >> io.rs2Value(4, 0)
        }.elsewhen(funct7 === "b0100000".U) {
          writeData := (io.rs1Value.asSInt >> io.rs2Value(4, 0)).asUInt
        }.otherwise {
          illegal := true.B
        }
      }
      is("b110".U) {
        when(funct7 === 0.U) {
          writeData := io.rs1Value | io.rs2Value
        }.otherwise {
          illegal := true.B
        }
      }
      is("b111".U) {
        when(funct7 === 0.U) {
          writeData := io.rs1Value & io.rs2Value
        }.otherwise {
          illegal := true.B
        }
      }
    }
  }.elsewhen(opcode === "b0001111".U) { // FENCE and FENCE.I
    when(funct3 === "b000".U || funct3 === "b001".U) {
      illegal := false.B
    }
  }.elsewhen(opcode === "b1110011".U) { // SYSTEM / Zicsr
    when(funct3 === "b000".U) {
      when(io.inst === "h00000073".U) { // ECALL from M-mode
        illegal := false.B
        isEcall := true.B
        nextPc  := io.mtvec
      }.elsewhen(io.inst === "h00100073".U) { // EBREAK
        illegal  := false.B
        isEbreak := true.B
      }.elsewhen(io.inst === "h30200073".U) { // MRET
        illegal := false.B
        nextPc  := io.mepc
      }
    }.elsewhen(funct3 === "b001".U || funct3 === "b010".U || funct3 === "b011".U) {
      usesRs1   := true.B
      writesRd  := true.B
      writeGpr  := true.B
      writeData := io.csrReadData
      when(io.csrSupported) {
        illegal := false.B
        when(funct3 === "b001".U) {
          csrWriteEnable := true.B
          csrWriteData   := io.rs1Value
        }.elsewhen(funct3 === "b010".U) {
          csrWriteEnable := rs1 =/= 0.U
          csrWriteData   := io.csrReadData | io.rs1Value
        }.otherwise {
          csrWriteEnable := rs1 =/= 0.U
          csrWriteData   := io.csrReadData & ~io.rs1Value
        }
      }
    }.elsewhen(funct3 === "b101".U || funct3 === "b110".U || funct3 === "b111".U) {
      val zimm = Cat(0.U(27.W), rs1)
      writesRd  := true.B
      writeGpr  := true.B
      writeData := io.csrReadData
      when(io.csrSupported) {
        illegal := false.B
        when(funct3 === "b101".U) {
          csrWriteEnable := true.B
          csrWriteData   := zimm
        }.elsewhen(funct3 === "b110".U) {
          csrWriteEnable := rs1 =/= 0.U
          csrWriteData   := io.csrReadData | zimm
        }.otherwise {
          csrWriteEnable := rs1 =/= 0.U
          csrWriteData   := io.csrReadData & ~zimm
        }
      }
    }
  }

  when(isLoad) {
    writeData := io.loadData
  }

  io.rs1Index       := rs1
  io.rs2Index       := rs2
  io.rdIndex        := rd
  io.funct3         := funct3
  io.usesRs1        := usesRs1
  io.usesRs2        := usesRs2
  io.writesRd       := writesRd
  io.writeGpr       := writeGpr
  io.writeData      := writeData
  io.nextPc         := nextPc
  io.illegal        := illegal
  io.isLoad         := isLoad
  io.isStore        := isStore
  io.memoryAddr     := memoryAddr
  io.isEbreak       := isEbreak
  io.isEcall        := isEcall
  io.csrAddress     := io.inst(31, 20)
  io.csrWriteEnable := csrWriteEnable
  io.csrWriteData   := csrWriteData
}
