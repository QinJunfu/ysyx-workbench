package npc

import chisel3._
import chisel3.util._

/** The small machine-mode CSR set used by the RV32E core. */
class CsrFile(resetPc: BigInt) extends Module {
  require(resetPc >= 0 && resetPc < (BigInt(1) << 32), "resetPc must fit in 32 bits")

  val io = IO(new Bundle {
    val readAddress   = Input(UInt(12.W))
    val readData      = Output(UInt(32.W))
    val readSupported = Output(Bool())
    val mtvec         = Output(UInt(32.W))
    val mepc          = Output(UInt(32.W))

    val active             = Input(Bool())
    val retiredInstruction = Input(Bool())
    val writeEnable        = Input(Bool())
    val writeAddress       = Input(UInt(12.W))
    val writeData          = Input(UInt(32.W))
    val ecall              = Input(Bool())
    val ecallPc            = Input(UInt(32.W))
  })

  val mstatus  = RegInit(0.U(32.W))
  val mtvec    = RegInit(resetPc.U(32.W))
  val mscratch = RegInit(0.U(32.W))
  val mepc     = RegInit(0.U(32.W))
  val mcause   = RegInit(0.U(32.W))
  val mip      = RegInit(0.U(32.W))
  val mcycle   = RegInit(0.U(64.W))
  val minstret = RegInit(0.U(64.W))

  io.readData      := 0.U(32.W)
  io.readSupported := false.B
  switch(io.readAddress) {
    is("h300".U) { io.readData := mstatus; io.readSupported := true.B }
    is("h301".U) { io.readData := "h40000010".U(32.W); io.readSupported := true.B }
    is("h305".U) { io.readData := mtvec; io.readSupported := true.B }
    is("h340".U) { io.readData := mscratch; io.readSupported := true.B }
    is("h341".U) { io.readData := mepc; io.readSupported := true.B }
    is("h342".U) { io.readData := mcause; io.readSupported := true.B }
    is("h344".U) { io.readData := mip; io.readSupported := true.B }
    is("hB00".U) { io.readData := mcycle(31, 0); io.readSupported := true.B }
    is("hB02".U) { io.readData := minstret(31, 0); io.readSupported := true.B }
    is("hF14".U) { io.readData := 0.U(32.W); io.readSupported := true.B }
  }

  io.mtvec := mtvec
  io.mepc  := mepc

  when(io.active) {
    mcycle := mcycle + 1.U
    when(io.retiredInstruction) {
      minstret := minstret + 1.U
    }

    when(io.ecall) {
      mepc   := io.ecallPc
      mcause := 11.U
    }.elsewhen(io.writeEnable) {
      switch(io.writeAddress) {
        is("h300".U) { mstatus := io.writeData }
        is("h305".U) { mtvec := io.writeData }
        is("h340".U) { mscratch := io.writeData }
        is("h341".U) { mepc := io.writeData }
        is("h342".U) { mcause := io.writeData }
        is("h344".U) { mip := io.writeData }
        is("hB00".U) { mcycle := Cat(mcycle(63, 32), io.writeData) }
        is("hB02".U) { minstret := Cat(minstret(63, 32), io.writeData) }
      }
    }
  }
}
