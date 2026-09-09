package npc

import chisel3._

/** Single-cycle RV32E core organized as IFU, IDU, EXU, LSU, and WBU.
  *
  * The five units are connected only by combinational signals. IFU's PC and WBU's register/CSR files are the
  * architectural state updated at the clock edge, so one active clock cycle still retires exactly one instruction.
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
  ifu.io.run      := true.B
  ifu.io.nextPc   := wbu.io.nextPc
  ifu.io.halt     := wbu.io.halt

  idu.io.pc   := ifu.io.pc
  idu.io.inst := ifu.io.inst

  wbu.io.active := ifu.io.active
  wbu.io.decoded <> idu.io.decoded
  wbu.io.exu <> exu.io.result
  wbu.io.lsu <> lsu.io.result

  exu.io.decoded <> idu.io.decoded
  exu.io.rs1Data      := wbu.io.rs1Data
  exu.io.rs2Data      := wbu.io.rs2Data
  exu.io.csrReadData  := wbu.io.csrReadData
  exu.io.csrSupported := wbu.io.csrSupported
  exu.io.mtvec        := wbu.io.mtvec
  exu.io.mepc         := wbu.io.mepc

  lsu.io.decoded <> idu.io.decoded
  lsu.io.address    := exu.io.result.memoryAddr
  lsu.io.storeData  := wbu.io.rs2Data
  lsu.io.loadValid  := wbu.io.loadValid
  lsu.io.storeValid := wbu.io.storeValid
  lsu.io.readData0  := io.dmemRdata
  lsu.io.readData1  := io.dmemRdata2

  io.imemAddr        := ifu.io.imemAddr
  io.dmemAddr        := lsu.io.result.alignedAddress
  io.dmemLogicalAddr := lsu.io.result.logicalAddress
  io.dmemAccessSize  := lsu.io.result.accessSize
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
  io.retireGprs <> wbu.io.retire.gprs
  io.halt        := wbu.io.retire.halt
  io.haltCode    := wbu.io.retire.haltCode
  io.invalid     := wbu.io.retire.invalid
}
