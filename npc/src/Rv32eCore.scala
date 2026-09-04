package npc

import chisel3._
import chisel3.util._

/**
  * Top-level interface for the single-cycle RV32E core.
  *
  * Instruction and data reads are 32-bit, word-aligned bus transactions.  A
  * potentially unaligned data access may consume two adjacent read words or
  * generate two adjacent masked write transactions.  The host is expected to
  * drive read data combinationally when the corresponding valid is asserted.
  */
class Rv32eCoreIO extends Bundle {
  val imemAddr = Output(UInt(32.W))
  val imemData = Input(UInt(32.W))

  val dmemAddr       = Output(UInt(32.W))
  val dmemRdata      = Input(UInt(32.W))
  val dmemReadValid  = Output(Bool())
  val dmemAddr2      = Output(UInt(32.W))
  val dmemRdata2     = Input(UInt(32.W))
  val dmemReadValid2 = Output(Bool())

  val dmemWrite0Valid = Output(Bool())
  val dmemWrite0Addr  = Output(UInt(32.W))
  val dmemWrite0Data  = Output(UInt(32.W))
  val dmemWrite0Mask  = Output(UInt(4.W))
  val dmemWrite1Valid = Output(Bool())
  val dmemWrite1Addr  = Output(UInt(32.W))
  val dmemWrite1Data  = Output(UInt(32.W))
  val dmemWrite1Mask  = Output(UInt(4.W))

  // A logical (rather than split bus) memory operation for mtrace.
  val memTraceValid = Output(Bool())
  val memTraceWrite = Output(Bool())
  val memTraceAddr  = Output(UInt(32.W))
  val memTraceData  = Output(UInt(32.W))
  val memTraceMask  = Output(UInt(4.W))

  // Values describe the instruction which commits on the current clock edge.
  val retireValid = Output(Bool())
  val retirePc    = Output(UInt(32.W))
  val retireInst  = Output(UInt(32.W))
  val retireDnPc  = Output(UInt(32.W))
  val retireGpr0  = Output(UInt(32.W))
  val retireGpr1  = Output(UInt(32.W))
  val retireGpr2  = Output(UInt(32.W))
  val retireGpr3  = Output(UInt(32.W))
  val retireGpr4  = Output(UInt(32.W))
  val retireGpr5  = Output(UInt(32.W))
  val retireGpr6  = Output(UInt(32.W))
  val retireGpr7  = Output(UInt(32.W))
  val retireGpr8  = Output(UInt(32.W))
  val retireGpr9  = Output(UInt(32.W))
  val retireGpr10 = Output(UInt(32.W))
  val retireGpr11 = Output(UInt(32.W))
  val retireGpr12 = Output(UInt(32.W))
  val retireGpr13 = Output(UInt(32.W))
  val retireGpr14 = Output(UInt(32.W))
  val retireGpr15 = Output(UInt(32.W))

  // halt is asserted together with the final retire event.  invalid denotes
  // an unsupported encoding or an RV32I register number outside RV32E.
  val halt     = Output(Bool())
  val haltCode = Output(UInt(32.W))
  val invalid  = Output(Bool())
}

/** A single-cycle implementation of the RV32E integer ISA. */
class Rv32eCore(val resetPc: BigInt = BigInt("80000000", 16)) extends Module {
  require(resetPc >= 0 && resetPc < (BigInt(1) << 32), "resetPc must fit in 32 bits")

  val io = IO(new Rv32eCoreIO)

  val pc      = RegInit(resetPc.U(32.W))
  val gprs    = RegInit(VecInit(Seq.fill(16)(0.U(32.W))))
  val halted  = RegInit(false.B)

  // The small CSR set is sufficient for RV32E AM code and simple trap tests.
  val mstatus  = RegInit(0.U(32.W))
  val mtvec    = RegInit(resetPc.U(32.W))
  val mscratch = RegInit(0.U(32.W))
  val mepc     = RegInit(0.U(32.W))
  val mcause   = RegInit(0.U(32.W))
  val mip      = RegInit(0.U(32.W))
  val mcycle   = RegInit(0.U(64.W))
  val minstret = RegInit(0.U(64.W))

  val inst   = io.imemData
  val opcode = inst(6, 0)
  val rd     = inst(11, 7)
  val funct3 = inst(14, 12)
  val rs1    = inst(19, 15)
  val rs2    = inst(24, 20)
  val funct7 = inst(31, 25)

  val immI = Cat(Fill(20, inst(31)), inst(31, 20))
  val immS = Cat(Fill(20, inst(31)), inst(31, 25), inst(11, 7))
  val immB = Cat(Fill(19, inst(31)), inst(31), inst(7), inst(30, 25), inst(11, 8), 0.U(1.W))
  val immU = Cat(inst(31, 12), 0.U(12.W))
  val immJ = Cat(Fill(11, inst(31)), inst(31), inst(19, 12), inst(20), inst(30, 21), 0.U(1.W))

  val rs1Valid = rs1 < 16.U
  val rs2Valid = rs2 < 16.U
  val rdValid  = rd < 16.U
  val rs1Value = Mux(rs1Valid, gprs(rs1(3, 0)), 0.U(32.W))
  val rs2Value = Mux(rs2Valid, gprs(rs2(3, 0)), 0.U(32.W))

  val nextPc         = WireDefault(pc + 4.U)
  val writeGpr       = WireDefault(false.B)
  val writeData      = WireDefault(0.U(32.W))
  val usesRs1        = WireDefault(false.B)
  val usesRs2        = WireDefault(false.B)
  val writesRd       = WireDefault(false.B)
  val illegal        = WireDefault(true.B)
  val isLoad         = WireDefault(false.B)
  val isStore        = WireDefault(false.B)
  val isEbreak       = WireDefault(false.B)
  val isEcall        = WireDefault(false.B)
  val csrWriteEnable = WireDefault(false.B)
  val csrWriteData   = WireDefault(0.U(32.W))

  val memAddr         = WireDefault(0.U(32.W))
  val accessBytes     = WireDefault(0.U(3.W))
  val logicalByteMask = WireDefault(0.U(4.W))
  val loadData        = WireDefault(0.U(32.W))

  val csrAddr      = inst(31, 20)
  val csrReadData  = WireDefault(0.U(32.W))
  val csrSupported = WireDefault(false.B)
  switch(csrAddr) {
    is("h300".U) { csrReadData := mstatus; csrSupported := true.B }
    is("h301".U) { csrReadData := "h40000010".U; csrSupported := true.B } // RV32 + E
    is("h305".U) { csrReadData := mtvec; csrSupported := true.B }
    is("h340".U) { csrReadData := mscratch; csrSupported := true.B }
    is("h341".U) { csrReadData := mepc; csrSupported := true.B }
    is("h342".U) { csrReadData := mcause; csrSupported := true.B }
    is("h344".U) { csrReadData := mip; csrSupported := true.B }
    is("hB00".U) { csrReadData := mcycle(31, 0); csrSupported := true.B }
    is("hB02".U) { csrReadData := minstret(31, 0); csrSupported := true.B }
    is("hF14".U) { csrReadData := 0.U; csrSupported := true.B }
  }

  when(opcode === "b0110111".U) { // LUI
    illegal := false.B
    writesRd := true.B
    writeGpr := true.B
    writeData := immU
  }.elsewhen(opcode === "b0010111".U) { // AUIPC
    illegal := false.B
    writesRd := true.B
    writeGpr := true.B
    writeData := pc + immU
  }.elsewhen(opcode === "b1101111".U) { // JAL
    illegal := false.B
    writesRd := true.B
    writeGpr := true.B
    writeData := pc + 4.U
    nextPc := pc + immJ
  }.elsewhen(opcode === "b1100111".U) { // JALR
    illegal := false.B
    usesRs1 := true.B
    writesRd := true.B
    writeGpr := true.B
    writeData := pc + 4.U
    when(funct3 === 0.U) {
      nextPc := (rs1Value + immI) & "hfffffffe".U(32.W)
    }.otherwise {
      illegal := true.B
    }
  }.elsewhen(opcode === "b1100011".U) { // conditional branches
    illegal := false.B
    usesRs1 := true.B
    usesRs2 := true.B
    val takeBranch = WireDefault(false.B)
    switch(funct3) {
      is("b000".U) { takeBranch := rs1Value === rs2Value } // BEQ
      is("b001".U) { takeBranch := rs1Value =/= rs2Value } // BNE
      is("b100".U) { takeBranch := rs1Value.asSInt < rs2Value.asSInt } // BLT
      is("b101".U) { takeBranch := rs1Value.asSInt >= rs2Value.asSInt } // BGE
      is("b110".U) { takeBranch := rs1Value < rs2Value } // BLTU
      is("b111".U) { takeBranch := rs1Value >= rs2Value } // BGEU
    }
    when(!(funct3 === "b000".U || funct3 === "b001".U || funct3 === "b100".U || funct3 === "b101".U || funct3 === "b110".U || funct3 === "b111".U)) {
      illegal := true.B
    }
    when(takeBranch) {
      nextPc := pc + immB
    }
  }.elsewhen(opcode === "b0000011".U) { // LOAD
    illegal := false.B
    usesRs1 := true.B
    writesRd := true.B
    writeGpr := true.B
    isLoad := true.B
    memAddr := rs1Value + immI
    switch(funct3) {
      is("b000".U) { // LB
        accessBytes := 1.U
        logicalByteMask := "b0001".U
      }
      is("b001".U) { // LH
        accessBytes := 2.U
        logicalByteMask := "b0011".U
      }
      is("b010".U) { // LW
        accessBytes := 4.U
        logicalByteMask := "b1111".U
      }
      is("b100".U) { // LBU
        accessBytes := 1.U
        logicalByteMask := "b0001".U
      }
      is("b101".U) { // LHU
        accessBytes := 2.U
        logicalByteMask := "b0011".U
      }
    }
    when(!(funct3 === "b000".U || funct3 === "b001".U || funct3 === "b010".U || funct3 === "b100".U || funct3 === "b101".U)) {
      illegal := true.B
    }
  }.elsewhen(opcode === "b0100011".U) { // STORE
    illegal := false.B
    usesRs1 := true.B
    usesRs2 := true.B
    isStore := true.B
    memAddr := rs1Value + immS
    switch(funct3) {
      is("b000".U) { // SB
        accessBytes := 1.U
        logicalByteMask := "b0001".U
      }
      is("b001".U) { // SH
        accessBytes := 2.U
        logicalByteMask := "b0011".U
      }
      is("b010".U) { // SW
        accessBytes := 4.U
        logicalByteMask := "b1111".U
      }
    }
    when(!(funct3 === "b000".U || funct3 === "b001".U || funct3 === "b010".U)) {
      illegal := true.B
    }
  }.elsewhen(opcode === "b0010011".U) { // OP-IMM
    illegal := false.B
    usesRs1 := true.B
    writesRd := true.B
    writeGpr := true.B
    switch(funct3) {
      is("b000".U) { writeData := rs1Value + immI } // ADDI
      is("b010".U) { writeData := (rs1Value.asSInt < immI.asSInt).asUInt } // SLTI
      is("b011".U) { writeData := (rs1Value < immI).asUInt } // SLTIU
      is("b100".U) { writeData := rs1Value ^ immI } // XORI
      is("b110".U) { writeData := rs1Value | immI } // ORI
      is("b111".U) { writeData := rs1Value & immI } // ANDI
      is("b001".U) { // SLLI
        when(funct7 === 0.U) {
          writeData := (rs1Value << inst(24, 20))(31, 0)
        }.otherwise {
          illegal := true.B
        }
      }
      is("b101".U) { // SRLI/SRAI
        when(funct7 === 0.U) {
          writeData := rs1Value >> inst(24, 20)
        }.elsewhen(funct7 === "b0100000".U) {
          writeData := (rs1Value.asSInt >> inst(24, 20)).asUInt
        }.otherwise {
          illegal := true.B
        }
      }
    }
    when(!(funct3 === "b000".U || funct3 === "b001".U || funct3 === "b010".U || funct3 === "b011".U || funct3 === "b100".U || funct3 === "b101".U || funct3 === "b110".U || funct3 === "b111".U)) {
      illegal := true.B
    }
  }.elsewhen(opcode === "b0110011".U) { // OP
    illegal := false.B
    usesRs1 := true.B
    usesRs2 := true.B
    writesRd := true.B
    writeGpr := true.B
    switch(funct3) {
      is("b000".U) {
        when(funct7 === 0.U) {
          writeData := rs1Value + rs2Value // ADD
        }.elsewhen(funct7 === "b0100000".U) {
          writeData := rs1Value - rs2Value // SUB
        }.otherwise {
          illegal := true.B
        }
      }
      is("b001".U) {
        when(funct7 === 0.U) {
          writeData := (rs1Value << rs2Value(4, 0))(31, 0) // SLL
        }.otherwise {
          illegal := true.B
        }
      }
      is("b010".U) {
        when(funct7 === 0.U) {
          writeData := (rs1Value.asSInt < rs2Value.asSInt).asUInt // SLT
        }.otherwise {
          illegal := true.B
        }
      }
      is("b011".U) {
        when(funct7 === 0.U) {
          writeData := (rs1Value < rs2Value).asUInt // SLTU
        }.otherwise {
          illegal := true.B
        }
      }
      is("b100".U) {
        when(funct7 === 0.U) {
          writeData := rs1Value ^ rs2Value // XOR
        }.otherwise {
          illegal := true.B
        }
      }
      is("b101".U) {
        when(funct7 === 0.U) {
          writeData := rs1Value >> rs2Value(4, 0) // SRL
        }.elsewhen(funct7 === "b0100000".U) {
          writeData := (rs1Value.asSInt >> rs2Value(4, 0)).asUInt // SRA
        }.otherwise {
          illegal := true.B
        }
      }
      is("b110".U) {
        when(funct7 === 0.U) {
          writeData := rs1Value | rs2Value // OR
        }.otherwise {
          illegal := true.B
        }
      }
      is("b111".U) {
        when(funct7 === 0.U) {
          writeData := rs1Value & rs2Value // AND
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
      when(inst === "h00000073".U) { // ECALL from M-mode
        illegal := false.B
        isEcall := true.B
        nextPc := mtvec
      }.elsewhen(inst === "h00100073".U) { // EBREAK
        illegal := false.B
        isEbreak := true.B
      }.elsewhen(inst === "h30200073".U) { // MRET
        illegal := false.B
        nextPc := mepc
      }
    }.elsewhen(funct3 === "b001".U || funct3 === "b010".U || funct3 === "b011".U) {
      usesRs1 := true.B
      writesRd := true.B
      writeGpr := true.B
      writeData := csrReadData
      when(csrSupported) {
        illegal := false.B
        when(funct3 === "b001".U) { // CSRRW
          csrWriteEnable := true.B
          csrWriteData := rs1Value
        }.elsewhen(funct3 === "b010".U) { // CSRRS
          csrWriteEnable := rs1 =/= 0.U
          csrWriteData := csrReadData | rs1Value
        }.otherwise { // CSRRC
          csrWriteEnable := rs1 =/= 0.U
          csrWriteData := csrReadData & ~rs1Value
        }
      }
    }.elsewhen(funct3 === "b101".U || funct3 === "b110".U || funct3 === "b111".U) {
      writesRd := true.B
      writeGpr := true.B
      writeData := csrReadData
      val zimm = Cat(0.U(27.W), rs1)
      when(csrSupported) {
        illegal := false.B
        when(funct3 === "b101".U) { // CSRRWI
          csrWriteEnable := true.B
          csrWriteData := zimm
        }.elsewhen(funct3 === "b110".U) { // CSRRSI
          csrWriteEnable := rs1 =/= 0.U
          csrWriteData := csrReadData | zimm
        }.otherwise { // CSRRCI
          csrWriteEnable := rs1 =/= 0.U
          csrWriteData := csrReadData & ~zimm
        }
      }
    }
  }

  val badRegister = (usesRs1 && !rs1Valid) || (usesRs2 && !rs2Valid) || (writesRd && !rdValid)
  val effectiveInvalid = illegal || badRegister
  val validLoad        = isLoad && !effectiveInvalid
  val validStore       = isStore && !effectiveInvalid
  val validCsrWrite    = csrWriteEnable && !effectiveInvalid
  val commitGprWrite   = writeGpr && !effectiveInvalid && rdValid && rd =/= 0.U

  val byteOffset  = memAddr(1, 0)
  val alignedAddr = Cat(memAddr(31, 2), 0.U(2.W))
  val crossesWord = (byteOffset +& accessBytes) > 4.U

  val loadBus = Cat(io.dmemRdata2, io.dmemRdata)
  val shiftedLoad = MuxLookup(
    byteOffset,
    loadBus
  )(
    Seq(
      1.U -> Cat(0.U(8.W), loadBus(63, 8)),
      2.U -> Cat(0.U(16.W), loadBus(63, 16)),
      3.U -> Cat(0.U(24.W), loadBus(63, 24))
    )
  )
  when(isLoad) {
    switch(funct3) {
      is("b000".U) { loadData := Cat(Fill(24, shiftedLoad(7)), shiftedLoad(7, 0)) } // LB
      is("b001".U) { loadData := Cat(Fill(16, shiftedLoad(15)), shiftedLoad(15, 0)) } // LH
      is("b010".U) { loadData := shiftedLoad(31, 0) } // LW
      is("b100".U) { loadData := Cat(0.U(24.W), shiftedLoad(7, 0)) } // LBU
      is("b101".U) { loadData := Cat(0.U(16.W), shiftedLoad(15, 0)) } // LHU
    }
    writeData := loadData
  }

  val storeData0 = MuxLookup(
    byteOffset,
    rs2Value
  )(
    Seq(
      1.U -> Cat(rs2Value(23, 0), 0.U(8.W)),
      2.U -> Cat(rs2Value(15, 0), 0.U(16.W)),
      3.U -> Cat(rs2Value(7, 0), 0.U(24.W))
    )
  )
  val storeData1 = MuxLookup(
    byteOffset,
    0.U(32.W)
  )(
    Seq(
      1.U -> Cat(0.U(24.W), rs2Value(31, 24)),
      2.U -> Cat(0.U(16.W), rs2Value(31, 16)),
      3.U -> Cat(0.U(8.W), rs2Value(31, 8))
    )
  )
  val storeMask0 = MuxLookup(
    byteOffset,
    logicalByteMask
  )(
    Seq(
      1.U -> Cat(logicalByteMask(2, 0), 0.U(1.W)),
      2.U -> Cat(logicalByteMask(1, 0), 0.U(2.W)),
      3.U -> Cat(logicalByteMask(0), 0.U(3.W))
    )
  )
  val storeMask1 = MuxLookup(
    byteOffset,
    0.U(4.W)
  )(
    Seq(
      1.U -> Cat(0.U(3.W), logicalByteMask(3)),
      2.U -> Cat(0.U(2.W), logicalByteMask(3, 2)),
      3.U -> Cat(0.U(1.W), logicalByteMask(3, 1))
    )
  )

  val nextGprs = WireDefault(gprs)
  nextGprs(0) := 0.U
  when(commitGprWrite) {
    nextGprs(rd(3, 0)) := writeData
  }

  val haltEvent = !halted && (isEbreak || effectiveInvalid)

  io.imemAddr := pc
  io.dmemAddr := alignedAddr
  io.dmemAddr2 := alignedAddr + 4.U
  io.dmemReadValid := validLoad
  io.dmemReadValid2 := validLoad && crossesWord
  io.dmemWrite0Addr := alignedAddr
  io.dmemWrite0Data := storeData0
  io.dmemWrite0Mask := storeMask0
  io.dmemWrite0Valid := validStore && storeMask0.orR
  io.dmemWrite1Addr := alignedAddr + 4.U
  io.dmemWrite1Data := storeData1
  io.dmemWrite1Mask := storeMask1
  io.dmemWrite1Valid := validStore && storeMask1.orR

  io.memTraceValid := validLoad || validStore
  io.memTraceWrite := validStore
  io.memTraceAddr := memAddr
  io.memTraceData := Mux(validStore, rs2Value, loadData)
  io.memTraceMask := logicalByteMask

  io.retireValid := !halted
  io.retirePc := pc
  io.retireInst := inst
  io.retireDnPc := nextPc
  io.retireGpr0 := nextGprs(0)
  io.retireGpr1 := nextGprs(1)
  io.retireGpr2 := nextGprs(2)
  io.retireGpr3 := nextGprs(3)
  io.retireGpr4 := nextGprs(4)
  io.retireGpr5 := nextGprs(5)
  io.retireGpr6 := nextGprs(6)
  io.retireGpr7 := nextGprs(7)
  io.retireGpr8 := nextGprs(8)
  io.retireGpr9 := nextGprs(9)
  io.retireGpr10 := nextGprs(10)
  io.retireGpr11 := nextGprs(11)
  io.retireGpr12 := nextGprs(12)
  io.retireGpr13 := nextGprs(13)
  io.retireGpr14 := nextGprs(14)
  io.retireGpr15 := nextGprs(15)
  io.halt := haltEvent
  io.haltCode := Mux(isEbreak && !effectiveInvalid, gprs(10), Mux(effectiveInvalid, 1.U, 0.U))
  io.invalid := !halted && effectiveInvalid

  when(!halted) {
    pc := nextPc
    for (i <- 1 until 16) {
      gprs(i) := nextGprs(i)
    }
    gprs(0) := 0.U

    mcycle := mcycle + 1.U
    when(!effectiveInvalid) {
      minstret := minstret + 1.U
    }

    when(isEcall && !effectiveInvalid) {
      mepc := pc
      mcause := 11.U
    }.elsewhen(validCsrWrite) {
      switch(csrAddr) {
        is("h300".U) { mstatus := csrWriteData }
        is("h305".U) { mtvec := csrWriteData }
        is("h340".U) { mscratch := csrWriteData }
        is("h341".U) { mepc := csrWriteData }
        is("h342".U) { mcause := csrWriteData }
        is("h344".U) { mip := csrWriteData }
        is("hB00".U) { mcycle := Cat(mcycle(63, 32), csrWriteData) }
        is("hB02".U) { minstret := Cat(minstret(63, 32), csrWriteData) }
      }
    }

    when(haltEvent) {
      halted := true.B
    }
  }.otherwise {
    gprs(0) := 0.U
  }
}

/** Default RTL top.  The simulation wrapper can instantiate this module by name. */
class NPC(resetPc: BigInt = BigInt("80000000", 16)) extends Rv32eCore(resetPc)
