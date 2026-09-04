package npc

import chisel3._

/** Single-cycle RV32E core.
  *
  * This module owns only the architectural PC and halt state. Register state, CSR state, instruction decode, and
  * aligned memory transactions each live in their own module so their responsibilities remain visible at the top level.
  */
class Rv32eCore(val resetPc: BigInt = BigInt("80000000", 16)) extends Module {
  require(resetPc >= 0 && resetPc < (BigInt(1) << 32), "resetPc must fit in 32 bits")

  val io = IO(new Rv32eCoreIO)

  val pc     = RegInit(resetPc.U(32.W))
  val halted = RegInit(false.B)

  val registerFile    = Module(new RegisterFile)
  val csrFile         = Module(new CsrFile(resetPc))
  val instructionUnit = Module(new InstructionUnit)
  val loadStoreUnit   = Module(new LoadStoreUnit)

  // Read register indices originate in the instruction decoder.
  registerFile.io.rs1Index := instructionUnit.io.rs1Index
  registerFile.io.rs2Index := instructionUnit.io.rs2Index
  registerFile.io.rdIndex  := instructionUnit.io.rdIndex

  // Feed current architectural state into the instruction decoder.
  instructionUnit.io.pc           := pc
  instructionUnit.io.inst         := io.imemData
  instructionUnit.io.rs1Value     := registerFile.io.rs1Data
  instructionUnit.io.rs2Value     := registerFile.io.rs2Data
  instructionUnit.io.csrReadData  := csrFile.io.readData
  instructionUnit.io.csrSupported := csrFile.io.readSupported
  instructionUnit.io.mtvec        := csrFile.io.mtvec
  instructionUnit.io.mepc         := csrFile.io.mepc

  val badRegister      = (instructionUnit.io.usesRs1 && !registerFile.io.rs1Valid) ||
    (instructionUnit.io.usesRs2 && !registerFile.io.rs2Valid) ||
    (instructionUnit.io.writesRd && !registerFile.io.rdValid)
  val effectiveInvalid = instructionUnit.io.illegal || badRegister
  val validLoad        = instructionUnit.io.isLoad && !effectiveInvalid
  val validStore       = instructionUnit.io.isStore && !effectiveInvalid
  val validCsrWrite    = instructionUnit.io.csrWriteEnable && !effectiveInvalid
  val commitGprWrite   = instructionUnit.io.writeGpr && !effectiveInvalid &&
    registerFile.io.rdValid && instructionUnit.io.rdIndex =/= 0.U

  // The load/store unit is combinational and returns split bus transfers.
  loadStoreUnit.io.rawLoad    := instructionUnit.io.isLoad
  loadStoreUnit.io.rawStore   := instructionUnit.io.isStore
  loadStoreUnit.io.loadValid  := validLoad
  loadStoreUnit.io.storeValid := validStore
  loadStoreUnit.io.funct3     := instructionUnit.io.funct3
  loadStoreUnit.io.address    := instructionUnit.io.memoryAddr
  loadStoreUnit.io.storeData  := registerFile.io.rs2Data
  loadStoreUnit.io.readData0  := io.dmemRdata
  loadStoreUnit.io.readData1  := io.dmemRdata2
  instructionUnit.io.loadData := loadStoreUnit.io.loadData

  // Register writes are committed only while the core is running.
  registerFile.io.writeEnable := !halted && commitGprWrite
  registerFile.io.writeData   := instructionUnit.io.writeData

  // CSR writes and counters share the same retirement boundary as the PC.
  csrFile.io.readAddress        := instructionUnit.io.csrAddress
  csrFile.io.active             := !halted
  csrFile.io.retiredInstruction := !effectiveInvalid
  csrFile.io.writeEnable        := validCsrWrite
  csrFile.io.writeAddress       := instructionUnit.io.csrAddress
  csrFile.io.writeData          := instructionUnit.io.csrWriteData
  csrFile.io.ecall              := instructionUnit.io.isEcall && !effectiveInvalid
  csrFile.io.ecallPc            := pc

  val haltEvent = !halted && (instructionUnit.io.isEbreak || effectiveInvalid)
  val traceData = Wire(UInt(32.W))
  val haltCode  = Wire(UInt(32.W))
  traceData := loadStoreUnit.io.loadData
  haltCode  := 0.U(32.W)
  when(validStore) {
    traceData := registerFile.io.rs2Data
  }
  when(effectiveInvalid) {
    haltCode := 1.U(32.W)
  }.elsewhen(instructionUnit.io.isEbreak) {
    haltCode := registerFile.io.currentValues(10)
  }

  io.imemAddr        := pc
  io.dmemAddr        := loadStoreUnit.io.alignedAddress
  io.dmemAddr2       := loadStoreUnit.io.address2
  io.dmemReadValid   := loadStoreUnit.io.read0Valid
  io.dmemReadValid2  := loadStoreUnit.io.read1Valid
  io.dmemWrite0Valid := loadStoreUnit.io.write0Valid
  io.dmemWrite0Addr  := loadStoreUnit.io.alignedAddress
  io.dmemWrite0Data  := loadStoreUnit.io.write0Data
  io.dmemWrite0Mask  := loadStoreUnit.io.write0Mask
  io.dmemWrite1Valid := loadStoreUnit.io.write1Valid
  io.dmemWrite1Addr  := loadStoreUnit.io.address2
  io.dmemWrite1Data  := loadStoreUnit.io.write1Data
  io.dmemWrite1Mask  := loadStoreUnit.io.write1Mask

  io.memTraceValid := validLoad || validStore
  io.memTraceWrite := validStore
  io.memTraceAddr  := instructionUnit.io.memoryAddr
  io.memTraceData  := traceData
  io.memTraceMask  := loadStoreUnit.io.logicalMask

  io.retireValid := !halted
  io.retirePc    := pc
  io.retireInst  := io.imemData
  io.retireDnPc  := instructionUnit.io.nextPc
  io.retireGpr0  := registerFile.io.retireValues(0)
  io.retireGpr1  := registerFile.io.retireValues(1)
  io.retireGpr2  := registerFile.io.retireValues(2)
  io.retireGpr3  := registerFile.io.retireValues(3)
  io.retireGpr4  := registerFile.io.retireValues(4)
  io.retireGpr5  := registerFile.io.retireValues(5)
  io.retireGpr6  := registerFile.io.retireValues(6)
  io.retireGpr7  := registerFile.io.retireValues(7)
  io.retireGpr8  := registerFile.io.retireValues(8)
  io.retireGpr9  := registerFile.io.retireValues(9)
  io.retireGpr10 := registerFile.io.retireValues(10)
  io.retireGpr11 := registerFile.io.retireValues(11)
  io.retireGpr12 := registerFile.io.retireValues(12)
  io.retireGpr13 := registerFile.io.retireValues(13)
  io.retireGpr14 := registerFile.io.retireValues(14)
  io.retireGpr15 := registerFile.io.retireValues(15)
  io.halt        := haltEvent
  io.haltCode    := haltCode
  io.invalid     := !halted && effectiveInvalid

  when(!halted) {
    pc := instructionUnit.io.nextPc
    when(haltEvent) {
      halted := true.B
    }
  }
}
