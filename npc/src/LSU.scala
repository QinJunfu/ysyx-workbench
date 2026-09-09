package npc

import chisel3._
import chisel3.util._

/** Load/store unit that converts logical RV32 accesses into aligned bus transactions. */
class LSU extends Module {
  val io = IO(new Bundle {
    val decoded    = Input(new DecodedInstruction)
    val address    = Input(UInt(32.W))
    val storeData  = Input(UInt(32.W))
    val loadValid  = Input(Bool())
    val storeValid = Input(Bool())
    val readData0  = Input(UInt(32.W))
    val readData1  = Input(UInt(32.W))

    val result = Output(new LsuResult)
  })

  val accessBytes = WireDefault(0.U(3.W))
  val accessSize  = WireDefault(0.U(3.W))
  val logicalMask = WireDefault(0.U(4.W))
  when(io.decoded.isLoad) {
    switch(io.decoded.funct3) {
      is("b000".U) { accessBytes := 1.U; accessSize := 0.U; logicalMask := "b0001".U }
      is("b001".U) { accessBytes := 2.U; accessSize := 1.U; logicalMask := "b0011".U }
      is("b010".U) { accessBytes := 4.U; accessSize := 2.U; logicalMask := "b1111".U }
      is("b100".U) { accessBytes := 1.U; accessSize := 0.U; logicalMask := "b0001".U }
      is("b101".U) { accessBytes := 2.U; accessSize := 1.U; logicalMask := "b0011".U }
    }
  }.elsewhen(io.decoded.isStore) {
    switch(io.decoded.funct3) {
      is("b000".U) { accessBytes := 1.U; accessSize := 0.U; logicalMask := "b0001".U }
      is("b001".U) { accessBytes := 2.U; accessSize := 1.U; logicalMask := "b0011".U }
      is("b010".U) { accessBytes := 4.U; accessSize := 2.U; logicalMask := "b1111".U }
    }
  }

  val byteOffset  = io.address(1, 0)
  val crossesWord = (byteOffset +& accessBytes) > 4.U
  val readBus     = Cat(io.readData1, io.readData0)
  val shiftedLoad = WireDefault(readBus)
  switch(byteOffset) {
    is(1.U) { shiftedLoad := Cat(0.U(8.W), readBus(63, 8)) }
    is(2.U) { shiftedLoad := Cat(0.U(16.W), readBus(63, 16)) }
    is(3.U) { shiftedLoad := Cat(0.U(24.W), readBus(63, 24)) }
  }

  val loadData = WireDefault(0.U(32.W))
  when(io.decoded.isLoad) {
    switch(io.decoded.funct3) {
      is("b000".U) { loadData := Cat(Fill(24, shiftedLoad(7)), shiftedLoad(7, 0)) }
      is("b001".U) { loadData := Cat(Fill(16, shiftedLoad(15)), shiftedLoad(15, 0)) }
      is("b010".U) { loadData := shiftedLoad(31, 0) }
      is("b100".U) { loadData := Cat(0.U(24.W), shiftedLoad(7, 0)) }
      is("b101".U) { loadData := Cat(0.U(16.W), shiftedLoad(15, 0)) }
    }
  }

  val writeData0 = WireDefault(io.storeData)
  val writeData1 = WireDefault(0.U(32.W))
  val writeMask0 = WireDefault(logicalMask)
  val writeMask1 = WireDefault(0.U(4.W))
  switch(byteOffset) {
    is(1.U) {
      writeData0 := Cat(io.storeData(23, 0), 0.U(8.W))
      writeData1 := Cat(0.U(24.W), io.storeData(31, 24))
      writeMask0 := Cat(logicalMask(2, 0), 0.U(1.W))
      writeMask1 := Cat(0.U(3.W), logicalMask(3))
    }
    is(2.U) {
      writeData0 := Cat(io.storeData(15, 0), 0.U(16.W))
      writeData1 := Cat(0.U(16.W), io.storeData(31, 16))
      writeMask0 := Cat(logicalMask(1, 0), 0.U(2.W))
      writeMask1 := Cat(0.U(2.W), logicalMask(3, 2))
    }
    is(3.U) {
      writeData0 := Cat(io.storeData(7, 0), 0.U(24.W))
      writeData1 := Cat(0.U(8.W), io.storeData(31, 8))
      writeMask0 := Cat(logicalMask(0), 0.U(3.W))
      writeMask1 := Cat(0.U(1.W), logicalMask(3, 1))
    }
  }

  io.result.logicalAddress := io.address
  io.result.accessSize     := accessSize
  io.result.alignedAddress := Cat(io.address(31, 2), 0.U(2.W))
  io.result.address2       := io.result.alignedAddress + 4.U
  io.result.read0Valid     := io.loadValid
  io.result.read1Valid     := io.loadValid && crossesWord
  io.result.write0Valid    := io.storeValid && writeMask0.orR
  io.result.write0Data     := writeData0
  io.result.write0Mask     := writeMask0
  io.result.write1Valid    := io.storeValid && writeMask1.orR
  io.result.write1Data     := writeData1
  io.result.write1Mask     := writeMask1
  io.result.loadData       := loadData
  io.result.logicalMask    := logicalMask
}
