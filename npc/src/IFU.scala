package npc

import chisel3._

/** Instruction fetch unit. It owns the only PC and halt state in the core. */
class IFU(resetPc: BigInt) extends Module {
  val io = IO(new Bundle {
    val imemData = Input(UInt(32.W))

    val nextPc = Input(UInt(32.W))
    val halt   = Input(Bool())

    val active   = Output(Bool())
    val pc       = Output(UInt(32.W))
    val inst     = Output(UInt(32.W))
    val imemAddr = Output(UInt(32.W))
  })

  val pc     = RegInit(resetPc.U(32.W))
  val halted = RegInit(false.B)

  io.active   := !halted
  io.pc       := pc
  io.inst     := io.imemData
  io.imemAddr := pc

  when(!halted) {
    pc := io.nextPc
    when(io.halt) {
      halted := true.B
    }
  }
}
