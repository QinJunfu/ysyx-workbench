package npc

import chisel3._
import chisel3.util._

/** One request channel and one response channel of the teaching SimpleBus. */
class SimpleBusMasterIO extends Bundle {
  val reqValid = Output(Bool())
  val reqReady = Input(Bool())
  val addr = Output(UInt(32.W))
  val wen = Output(Bool())
  val wdata = Output(UInt(32.W))
  val wmask = Output(UInt(4.W))
  val respValid = Input(Bool())
  val respReady = Output(Bool())
  val rdata = Input(UInt(32.W))
  val error = Input(Bool())
}

class SimpleBusSlaveIO extends Bundle {
  val reqValid = Input(Bool())
  val reqReady = Output(Bool())
  val addr = Input(UInt(32.W))
  val wen = Input(Bool())
  val wdata = Input(UInt(32.W))
  val wmask = Input(UInt(4.W))
  val respValid = Output(Bool())
  val respReady = Input(Bool())
  val rdata = Output(UInt(32.W))
  val error = Output(Bool())
}

/**
  * Small synchronous SimpleBus SRAM.  It has one response register, so a
  * request is accepted only when the previous response has been consumed.
  * Reads and writes both complete with a response one cycle after acceptance.
  */
class SimpleBusMemory(val wordCount: Int = 256, val responseDelay: Int = 1) extends Module {
  require(wordCount > 0 && wordCount <= 1024)
  require(responseDelay >= 1)

  val io = IO(new SimpleBusSlaveIO)
  val memory = RegInit(VecInit(Seq.fill(wordCount)(0.U(32.W))))
  val responseValid = RegInit(false.B)
  val responseData = RegInit(0.U(32.W))
  val responseError = RegInit(false.B)
  val pending = RegInit(false.B)
  val delayWidth = if (responseDelay < 2) 1 else log2Ceil(responseDelay + 1)
  val delay = RegInit(0.U(delayWidth.W))

  io.reqReady := !responseValid && !pending
  io.respValid := responseValid
  io.rdata := responseData
  io.error := responseError

  when(responseValid && io.respReady) {
    responseValid := false.B
  }
  when(pending) {
    when(delay === 0.U) {
      responseValid := true.B
      pending := false.B
    }.otherwise {
      delay := delay - 1.U
    }
  }
  when(io.reqValid && io.reqReady) {
    val index = io.addr(log2Ceil(wordCount) + 1, 2)
    pending := true.B
    delay := (responseDelay - 1).U
    responseError := index >= wordCount.U
    responseData := 0.U
    when(index < wordCount.U) {
      when(io.wen) {
        when(io.wmask(0)) { memory(index)(7, 0) := io.wdata(7, 0) }
        when(io.wmask(1)) { memory(index)(15, 8) := io.wdata(15, 8) }
        when(io.wmask(2)) { memory(index)(23, 16) := io.wdata(23, 16) }
        when(io.wmask(3)) { memory(index)(31, 24) := io.wdata(31, 24) }
      }.otherwise {
        responseData := memory(index)
      }
    }
  }
}
