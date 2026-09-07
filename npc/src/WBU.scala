package npc

import chisel3._

/** Writeback unit owns architectural register and CSR updates and exposes the single retirement event used by the host.
  */
class WBU(resetPc: BigInt) extends Module {
  val io = IO(new Bundle {
    val active  = Input(Bool())
    val decoded = Input(new DecodedInstruction)
    val exu     = Input(new ExuResult)
    val lsu     = Input(new LsuResult)

    val rs1Data      = Output(UInt(32.W))
    val rs2Data      = Output(UInt(32.W))
    val csrReadData  = Output(UInt(32.W))
    val csrSupported = Output(Bool())
    val mtvec        = Output(UInt(32.W))
    val mepc         = Output(UInt(32.W))

    val effectiveInvalid = Output(Bool())
    val loadValid        = Output(Bool())
    val storeValid       = Output(Bool())
    val nextPc           = Output(UInt(32.W))
    val halt             = Output(Bool())
    val retire           = Output(new RetireInfo)
  })

  val registerFile = Module(new RegisterFile)
  val csrFile      = Module(new CsrFile(resetPc))

  registerFile.io.rs1Index := io.decoded.rs1Index
  registerFile.io.rs2Index := io.decoded.rs2Index
  registerFile.io.rdIndex  := io.decoded.rdIndex

  csrFile.io.readAddress := io.decoded.csrAddress

  val badRegister      = (io.decoded.usesRs1 && !registerFile.io.rs1Valid) ||
    (io.decoded.usesRs2 && !registerFile.io.rs2Valid) ||
    (io.decoded.writesRd && !registerFile.io.rdValid)
  val effectiveInvalid = io.exu.illegal || badRegister
  val validLoad        = io.decoded.isLoad && !effectiveInvalid
  val validStore       = io.decoded.isStore && !effectiveInvalid
  val validCsrWrite    = io.decoded.csrWriteEnable && !effectiveInvalid
  val commitGprWrite   = io.decoded.writeGpr && !effectiveInvalid &&
    registerFile.io.rdValid && io.decoded.rdIndex =/= 0.U
  val haltEvent        = io.active && (io.decoded.isEbreak || effectiveInvalid)

  val writebackData = Mux(io.decoded.isLoad, io.lsu.loadData, io.exu.writebackData)
  registerFile.io.writeEnable := io.active && commitGprWrite
  registerFile.io.writeData   := writebackData

  csrFile.io.active             := io.active
  csrFile.io.retiredInstruction := !effectiveInvalid
  csrFile.io.writeEnable        := validCsrWrite
  csrFile.io.writeAddress       := io.decoded.csrAddress
  csrFile.io.writeData          := io.exu.csrWriteData
  csrFile.io.ecall              := io.decoded.isEcall && !effectiveInvalid
  csrFile.io.ecallPc            := io.decoded.pc

  val traceData = WireDefault(io.lsu.loadData)
  when(validStore) {
    traceData := registerFile.io.rs2Data
  }

  val haltCode = WireDefault(0.U(32.W))
  when(effectiveInvalid) {
    haltCode := 1.U
  }.elsewhen(io.decoded.isEbreak) {
    haltCode := registerFile.io.currentValues(10)
  }

  io.rs1Data      := registerFile.io.rs1Data
  io.rs2Data      := registerFile.io.rs2Data
  io.csrReadData  := csrFile.io.readData
  io.csrSupported := csrFile.io.readSupported
  io.mtvec        := csrFile.io.mtvec
  io.mepc         := csrFile.io.mepc

  io.effectiveInvalid := effectiveInvalid
  io.loadValid        := validLoad
  io.storeValid       := validStore
  io.nextPc           := io.exu.nextPc
  io.halt             := haltEvent

  io.retire.valid         := io.active
  io.retire.pc            := io.decoded.pc
  io.retire.inst          := io.decoded.inst
  io.retire.nextPc        := io.exu.nextPc
  io.retire.gprs          := registerFile.io.retireValues
  io.retire.memTraceValid := validLoad || validStore
  io.retire.memTraceWrite := validStore
  io.retire.memTraceAddr  := io.exu.memoryAddr
  io.retire.memTraceData  := traceData
  io.retire.memTraceMask  := io.lsu.logicalMask
  io.retire.halt          := haltEvent
  io.retire.haltCode      := haltCode
  io.retire.invalid       := io.active && effectiveInvalid
}
