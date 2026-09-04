package gcd

import chisel3._

/** Subtraction-based GCD calculator with a simple load/result interface.
  *
  * While y is nonzero, the larger register loses the smaller one on each clock. When y reaches zero, x contains the
  * result.
  */
class GCD extends Module {
  val io = IO(new Bundle {
    val value1        = Input(UInt(16.W))
    val value2        = Input(UInt(16.W))
    val loadingValues = Input(Bool())
    val outputGCD     = Output(UInt(16.W))
    val outputValid   = Output(Bool())
  })

  val x = RegInit(0.U(16.W))
  val y = RegInit(0.U(16.W))

  when(io.loadingValues) {
    x := io.value1
    y := io.value2
  }.elsewhen(y =/= 0.U) {
    when(x > y) {
      x := x - y
    }.otherwise {
      y := y - x
    }
  }

  io.outputGCD   := x
  io.outputValid := y === 0.U
}
