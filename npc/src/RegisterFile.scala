package npc

import chisel3._

/** RV32E register file with sixteen 32-bit general-purpose registers. */
class RegisterFile extends Module {
  val io = IO(new Bundle {
    val rs1Index = Input(UInt(5.W))
    val rs2Index = Input(UInt(5.W))
    val rdIndex  = Input(UInt(5.W))

    val rs1Data  = Output(UInt(32.W))
    val rs2Data  = Output(UInt(32.W))
    val rs1Valid = Output(Bool())
    val rs2Valid = Output(Bool())
    val rdValid  = Output(Bool())

    val writeEnable = Input(Bool())
    val writeData   = Input(UInt(32.W))

    // The retirement view includes this cycle's writeback.
    val retireValues  = Output(Vec(16, UInt(32.W)))
    val currentValues = Output(Vec(16, UInt(32.W)))
  })

  val registers = RegInit(
    VecInit(
      0.U(32.W),
      0.U(32.W),
      0.U(32.W),
      0.U(32.W),
      0.U(32.W),
      0.U(32.W),
      0.U(32.W),
      0.U(32.W),
      0.U(32.W),
      0.U(32.W),
      0.U(32.W),
      0.U(32.W),
      0.U(32.W),
      0.U(32.W),
      0.U(32.W),
      0.U(32.W)
    )
  )

  val nextValues = Wire(Vec(16, UInt(32.W)))
  for (index <- 0 until 16) {
    nextValues(index)       := registers(index)
    io.currentValues(index) := registers(index)
  }

  io.rs1Valid := io.rs1Index < 16.U
  io.rs2Valid := io.rs2Index < 16.U
  io.rdValid  := io.rdIndex < 16.U
  io.rs1Data  := Mux(io.rs1Valid, registers(io.rs1Index(3, 0)), 0.U(32.W))
  io.rs2Data  := Mux(io.rs2Valid, registers(io.rs2Index(3, 0)), 0.U(32.W))

  nextValues(0) := 0.U
  when(io.writeEnable && io.rdValid && io.rdIndex =/= 0.U) {
    nextValues(io.rdIndex(3, 0)) := io.writeData
  }

  for (index <- 0 until 16) {
    io.retireValues(index) := nextValues(index)
  }

  for (index <- 1 until 16) {
    registers(index) := nextValues(index)
  }
  registers(0) := 0.U
}
