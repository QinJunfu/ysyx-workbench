package npc

import chisel3._
import chisel3.util._

/** Converts logical RV32 load/store operations into aligned memory transfers. */
class LoadStoreUnit extends Module {
  val io = IO(new Bundle {
    val rawLoad    = Input(Bool())
    val rawStore   = Input(Bool())
    val loadValid  = Input(Bool())
    val storeValid = Input(Bool())
    val funct3     = Input(UInt(3.W))
    val address    = Input(UInt(32.W))
    val storeData  = Input(UInt(32.W))
    val readData0  = Input(UInt(32.W))
    val readData1  = Input(UInt(32.W))

    val alignedAddress = Output(UInt(32.W))
    val address2       = Output(UInt(32.W))
    val read0Valid     = Output(Bool())
    val read1Valid     = Output(Bool())
    val write0Valid    = Output(Bool())
    val write0Data     = Output(UInt(32.W))
    val write0Mask     = Output(UInt(4.W))
    val write1Valid    = Output(Bool())
    val write1Data     = Output(UInt(32.W))
    val write1Mask     = Output(UInt(4.W))
    val loadData       = Output(UInt(32.W))
    val logicalMask    = Output(UInt(4.W))
  })

  val accessBytes = Wire(UInt(3.W))
  val logicalMask = Wire(UInt(4.W))
  accessBytes := 0.U(3.W)
  logicalMask := 0.U(4.W)

  when(io.rawLoad) {
    switch(io.funct3) {
      is("b000".U) { accessBytes := 1.U; logicalMask := "b0001".U }
      is("b001".U) { accessBytes := 2.U; logicalMask := "b0011".U }
      is("b010".U) { accessBytes := 4.U; logicalMask := "b1111".U }
      is("b100".U) { accessBytes := 1.U; logicalMask := "b0001".U }
      is("b101".U) { accessBytes := 2.U; logicalMask := "b0011".U }
    }
  }.elsewhen(io.rawStore) {
    switch(io.funct3) {
      is("b000".U) { accessBytes := 1.U; logicalMask := "b0001".U }
      is("b001".U) { accessBytes := 2.U; logicalMask := "b0011".U }
      is("b010".U) { accessBytes := 4.U; logicalMask := "b1111".U }
    }
  }

  val byteOffset  = io.address(1, 0)
  val crossesWord = (byteOffset +& accessBytes) > 4.U
  val readBus     = Cat(io.readData1, io.readData0)
  val shiftedLoad = Wire(UInt(64.W))
  shiftedLoad := readBus
  switch(byteOffset) {
    is(1.U) { shiftedLoad := Cat(0.U(8.W), readBus(63, 8)) }
    is(2.U) { shiftedLoad := Cat(0.U(16.W), readBus(63, 16)) }
    is(3.U) { shiftedLoad := Cat(0.U(24.W), readBus(63, 24)) }
  }

  val loadData = Wire(UInt(32.W))
  loadData := 0.U(32.W)
  when(io.rawLoad) {
    switch(io.funct3) {
      is("b000".U) { loadData := Cat(Fill(24, shiftedLoad(7)), shiftedLoad(7, 0)) }
      is("b001".U) { loadData := Cat(Fill(16, shiftedLoad(15)), shiftedLoad(15, 0)) }
      is("b010".U) { loadData := shiftedLoad(31, 0) }
      is("b100".U) { loadData := Cat(0.U(24.W), shiftedLoad(7, 0)) }
      is("b101".U) { loadData := Cat(0.U(16.W), shiftedLoad(15, 0)) }
    }
  }

  val writeData0 = Wire(UInt(32.W))
  val writeData1 = Wire(UInt(32.W))
  val writeMask0 = Wire(UInt(4.W))
  val writeMask1 = Wire(UInt(4.W))
  writeData0 := io.storeData
  writeData1 := 0.U(32.W)
  writeMask0 := logicalMask
  writeMask1 := 0.U(4.W)
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

  io.alignedAddress := Cat(io.address(31, 2), 0.U(2.W))
  io.address2       := io.alignedAddress + 4.U
  io.read0Valid     := io.loadValid
  io.read1Valid     := io.loadValid && crossesWord
  io.write0Valid    := io.storeValid && writeMask0.orR
  io.write0Data     := writeData0
  io.write0Mask     := writeMask0
  io.write1Valid    := io.storeValid && writeMask1.orR
  io.write1Data     := writeData1
  io.write1Mask     := writeMask1
  io.loadData       := loadData
  io.logicalMask    := logicalMask
}
