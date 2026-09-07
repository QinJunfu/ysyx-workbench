package npc

import chisel3._

/**
  * Single-cycle RV32E core organized as IFU, IDU, EXU, LSU, and WBU.
  *
  * The five units are connected only by combinational signals. IFU's PC and
  * WBU's register/CSR files are the architectural state updated at the clock
  * edge, so one active clock cycle still retires exactly one instruction.
  */
class Rv32eCore(val resetPc: BigInt = BigInt("80000000", 16)) extends Module {
  require(resetPc >= 0 && resetPc < (BigInt(1) << 32), "resetPc must fit in 32 bits")

  val io = IO(new Rv32eCoreIO)

  val ifu = Module(new IFU(resetPc))
  val idu = Module(new IDU)
  val exu = Module(new EXU)
  val lsu = Module(new LSU)
  val wbu = Module(new WBU(resetPc))

  ifu.io.imemData := io.imemData
  ifu.io.nextPc   := wbu.io.nextPc
  ifu.io.halt     := wbu.io.halt

  idu.io.pc   := ifu.io.pc
  idu.io.inst := ifu.io.inst

  wbu.io.active  := ifu.io.active
  wbu.io.decoded := idu.io.decoded
  wbu.io.exu     := exu.io.result
  wbu.io.lsu     := lsu.io.result

  exu.io.decoded      := idu.io.decoded
  exu.io.rs1Data      := wbu.io.rs1Data
  exu.io.rs2Data      := wbu.io.rs2Data
  exu.io.csrReadData  := wbu.io.csrReadData
  exu.io.csrSupported := wbu.io.csrSupported
  exu.io.mtvec        := wbu.io.mtvec
  exu.io.mepc         := wbu.io.mepc

  lsu.io.decoded    := idu.io.decoded
  lsu.io.address    := exu.io.result.memoryAddr
  lsu.io.storeData  := wbu.io.rs2Data
  lsu.io.loadValid  := wbu.io.loadValid
  lsu.io.storeValid := wbu.io.storeValid
  lsu.io.readData0  := io.dmemRdata
  lsu.io.readData1  := io.dmemRdata2

  io.imemAddr        := ifu.io.imemAddr
  io.dmemAddr        := lsu.io.result.alignedAddress
  io.dmemAddr2       := lsu.io.result.address2
  io.dmemReadValid   := lsu.io.result.read0Valid
  io.dmemReadValid2  := lsu.io.result.read1Valid
  io.dmemWrite0Valid := lsu.io.result.write0Valid
  io.dmemWrite0Addr  := lsu.io.result.alignedAddress
  io.dmemWrite0Data  := lsu.io.result.write0Data
  io.dmemWrite0Mask  := lsu.io.result.write0Mask
  io.dmemWrite1Valid := lsu.io.result.write1Valid
  io.dmemWrite1Addr  := lsu.io.result.address2
  io.dmemWrite1Data  := lsu.io.result.write1Data
  io.dmemWrite1Mask  := lsu.io.result.write1Mask

  io.memTraceValid := wbu.io.retire.memTraceValid
  io.memTraceWrite := wbu.io.retire.memTraceWrite
  io.memTraceAddr  := wbu.io.retire.memTraceAddr
  io.memTraceData  := wbu.io.retire.memTraceData
  io.memTraceMask  := wbu.io.retire.memTraceMask

  io.retireValid := wbu.io.retire.valid
  io.retirePc    := wbu.io.retire.pc
  io.retireInst  := wbu.io.retire.inst
  io.retireDnPc  := wbu.io.retire.nextPc
  io.retireGpr0  := wbu.io.retire.gprs(0)
  io.retireGpr1  := wbu.io.retire.gprs(1)
  io.retireGpr2  := wbu.io.retire.gprs(2)
  io.retireGpr3  := wbu.io.retire.gprs(3)
  io.retireGpr4  := wbu.io.retire.gprs(4)
  io.retireGpr5  := wbu.io.retire.gprs(5)
  io.retireGpr6  := wbu.io.retire.gprs(6)
  io.retireGpr7  := wbu.io.retire.gprs(7)
  io.retireGpr8  := wbu.io.retire.gprs(8)
  io.retireGpr9  := wbu.io.retire.gprs(9)
  io.retireGpr10 := wbu.io.retire.gprs(10)
  io.retireGpr11 := wbu.io.retire.gprs(11)
  io.retireGpr12 := wbu.io.retire.gprs(12)
  io.retireGpr13 := wbu.io.retire.gprs(13)
  io.retireGpr14 := wbu.io.retire.gprs(14)
  io.retireGpr15 := wbu.io.retire.gprs(15)
  io.halt        := wbu.io.retire.halt
  io.haltCode    := wbu.io.retire.haltCode
  io.invalid     := wbu.io.retire.invalid
}
