package npc

import chisel3._

/** Top-level interface for the single-cycle RV32E core.
  *
  * Instruction and data reads are 32-bit, word-aligned bus transactions. A potentially unaligned data access may use
  * two adjacent read words or two adjacent masked write transactions.
  */
class Rv32eCoreIO extends Bundle {
  val imemAddr = Output(UInt(32.W))
  val imemData = Input(UInt(32.W))

  val dmemAddr        = Output(UInt(32.W))
  val dmemLogicalAddr = Output(UInt(32.W))
  val dmemAccessSize  = Output(UInt(3.W))
  val dmemRdata       = Input(UInt(32.W))
  val dmemReadValid   = Output(Bool())
  val dmemAddr2       = Output(UInt(32.W))
  val dmemRdata2      = Input(UInt(32.W))
  val dmemReadValid2  = Output(Bool())

  val dmemWrite0Valid = Output(Bool())
  val dmemWrite0Addr  = Output(UInt(32.W))
  val dmemWrite0Data  = Output(UInt(32.W))
  val dmemWrite0Mask  = Output(UInt(4.W))
  val dmemWrite1Valid = Output(Bool())
  val dmemWrite1Addr  = Output(UInt(32.W))
  val dmemWrite1Data  = Output(UInt(32.W))
  val dmemWrite1Mask  = Output(UInt(4.W))

  // A logical, unsplit memory operation used by the host mtrace facility.
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
  val retireGprs  = Output(Vec(16, UInt(32.W)))

  // halt and invalid describe the current retire event.
  val halt     = Output(Bool())
  val haltCode = Output(UInt(32.W))
  val invalid  = Output(Bool())
}
