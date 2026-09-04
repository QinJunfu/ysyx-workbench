package gcd

import chisel3._

/** GCD calculator with an explicit valid/ready/bits interface.
  *
  * One transaction is accepted at a time. Once a result is available, all output fields remain stable until the
  * consumer accepts it.
  */
class DecoupledGcd(width: Int) extends Module {
  val input  = IO(new GcdInputPort(width))
  val output = IO(new GcdOutputPort(width))

  val xInitial    = RegInit(0.U(width.W))
  val yInitial    = RegInit(0.U(width.W))
  val x           = RegInit(0.U(width.W))
  val y           = RegInit(0.U(width.W))
  val gcdValue    = RegInit(0.U(width.W))
  val busy        = RegInit(false.B)
  val resultValid = RegInit(false.B)

  input.ready        := !busy
  output.valid       := resultValid
  output.bits.value1 := xInitial
  output.bits.value2 := yInitial
  output.bits.gcd    := gcdValue

  when(!busy) {
    when(input.valid && input.ready) {
      x           := input.bits.value1
      y           := input.bits.value2
      xInitial    := input.bits.value1
      yInitial    := input.bits.value2
      busy        := true.B
      resultValid := false.B
    }
  }.elsewhen(!resultValid) {
    when(x === 0.U || y === 0.U) {
      when(x === 0.U) {
        gcdValue := y
      }.otherwise {
        gcdValue := x
      }
      resultValid := true.B
    }.elsewhen(x > y) {
      x := x - y
    }.otherwise {
      y := y - x
    }
  }.otherwise {
    when(output.ready) {
      busy        := false.B
      resultValid := false.B
    }
  }
}
