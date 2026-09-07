package npc

import chisel3._

/** AXI4-Lite slave directions, written out to make each handshake visible. */
class AxiLiteSlaveIO extends Bundle {
  val awvalid = Input(Bool())
  val awready = Output(Bool())
  val awaddr = Input(UInt(32.W))
  val wvalid = Input(Bool())
  val wready = Output(Bool())
  val wdata = Input(UInt(32.W))
  val wstrb = Input(UInt(4.W))
  val bvalid = Output(Bool())
  val bready = Input(Bool())
  val bresp = Output(UInt(2.W))
  val arvalid = Input(Bool())
  val arready = Output(Bool())
  val araddr = Input(UInt(32.W))
  val rvalid = Output(Bool())
  val rready = Input(Bool())
  val rdata = Output(UInt(32.W))
  val rresp = Output(UInt(2.W))
}

/** UART device register at 0x10000000. */
class AxiLiteUart extends Module {
  val io = IO(new Bundle {
    val axi = new AxiLiteSlaveIO
    val txValid = Output(Bool())
    val txData = Output(UInt(8.W))
  })

  val awHold = RegInit(false.B)
  val wHold = RegInit(false.B)
  val awAddr = RegInit(0.U(32.W))
  val wData = RegInit(0.U(32.W))
  val wStrb = RegInit(0.U(4.W))
  val bHold = RegInit(false.B)
  val rHold = RegInit(false.B)
  val rData = RegInit(0.U(32.W))
  val txPulse = RegInit(false.B)
  val txByte = RegInit(0.U(8.W))

  io.axi.awready := !awHold && !bHold
  io.axi.wready := !wHold && !bHold
  io.axi.bvalid := bHold
  io.axi.bresp := 0.U
  io.axi.arready := !rHold
  io.axi.rvalid := rHold
  io.axi.rdata := rData
  io.axi.rresp := 0.U
  io.txValid := txPulse
  io.txData := txByte

  when(txPulse) {
    txPulse := false.B
  }
  when(io.axi.awvalid && io.axi.awready) {
    awAddr := io.axi.awaddr
    awHold := true.B
  }
  when(io.axi.wvalid && io.axi.wready) {
    wData := io.axi.wdata
    wStrb := io.axi.wstrb
    wHold := true.B
  }
  when(!bHold && (awHold || (io.axi.awvalid && io.axi.awready)) &&
      (wHold || (io.axi.wvalid && io.axi.wready))) {
    bHold := true.B
    when(Mux(awHold, awAddr, io.axi.awaddr) === "h10000000".U &&
         Mux(wHold, wStrb, io.axi.wstrb)(0)) {
      txPulse := true.B
      txByte := Mux(wHold, wData, io.axi.wdata)(7, 0)
    }
    awHold := false.B
    wHold := false.B
  }
  when(bHold && io.axi.bready) {
    bHold := false.B
  }
  when(io.axi.arvalid && io.axi.arready) {
    rHold := true.B
    rData := 0.U
  }
  when(rHold && io.axi.rready) {
    rHold := false.B
  }
}

/** CLINT mtime register at 0x20000000/0x20000004. */
class AxiLiteClint extends Module {
  val io = IO(new AxiLiteSlaveIO)
  val mtime = RegInit(0.U(64.W))
  val rHold = RegInit(false.B)
  val rData = RegInit(0.U(32.W))
  val bHold = RegInit(false.B)

  io.awready := false.B
  io.wready := false.B
  io.bvalid := bHold
  io.bresp := 0.U
  io.arready := !rHold
  io.rvalid := rHold
  io.rdata := rData
  io.rresp := 0.U

  mtime := mtime + 1.U
  when(io.arvalid && io.arready) {
    rHold := true.B
    when(io.araddr === "h20000000".U) {
      rData := mtime(31, 0)
    }.elsewhen(io.araddr === "h20000004".U) {
      rData := mtime(63, 32)
    }.otherwise {
      rData := 0.U
    }
  }
  when(rHold && io.rready) {
    rHold := false.B
  }
  when(bHold && io.bready) {
    bHold := false.B
  }
}

/**
  * Address decoder for one CPU master and one SRAM slave.  UART and CLINT are
  * instantiated here, so adding a device does not change the CPU protocol.
  * The current NPC issues one request at a time and presents AW/W together;
  * this keeps the decoder deliberately small while still honoring every
  * AXI4-Lite handshake.
  */
class AxiLiteXbar extends Module {
  val io = IO(new Bundle {
    val cpu = Flipped(new AxiLiteMasterIO)
    val memory = new AxiLiteMasterIO
    val uartTxValid = Output(Bool())
    val uartTxData = Output(UInt(8.W))
  })

  val uart = Module(new AxiLiteUart)
  val clint = Module(new AxiLiteClint)
  val isUartWrite = io.cpu.awaddr === "h10000000".U
  val isClintWrite = io.cpu.awaddr === "h20000000".U || io.cpu.awaddr === "h20000004".U
  val isUartRead = io.cpu.araddr >= "h10000000".U && io.cpu.araddr < "h10001000".U
  val isClintRead = io.cpu.araddr === "h20000000".U || io.cpu.araddr === "h20000004".U

  io.memory.awvalid := io.cpu.awvalid && !isUartWrite && !isClintWrite
  io.memory.awaddr := io.cpu.awaddr
  io.memory.wvalid := io.cpu.wvalid && !isUartWrite && !isClintWrite
  io.memory.wdata := io.cpu.wdata
  io.memory.wstrb := io.cpu.wstrb
  io.memory.bready := io.cpu.bready
  io.memory.arvalid := io.cpu.arvalid && !isUartRead && !isClintRead
  io.memory.araddr := io.cpu.araddr
  io.memory.rready := io.cpu.rready

  uart.io.axi.awvalid := io.cpu.awvalid && isUartWrite
  uart.io.axi.awaddr := io.cpu.awaddr
  uart.io.axi.wvalid := io.cpu.wvalid && isUartWrite
  uart.io.axi.wdata := io.cpu.wdata
  uart.io.axi.wstrb := io.cpu.wstrb
  uart.io.axi.bready := io.cpu.bready
  uart.io.axi.arvalid := io.cpu.arvalid && isUartRead
  uart.io.axi.araddr := io.cpu.araddr
  uart.io.axi.rready := io.cpu.rready

  clint.io.awvalid := io.cpu.awvalid && isClintWrite
  clint.io.awaddr := io.cpu.awaddr
  clint.io.wvalid := io.cpu.wvalid && isClintWrite
  clint.io.wdata := io.cpu.wdata
  clint.io.wstrb := io.cpu.wstrb
  clint.io.bready := io.cpu.bready
  clint.io.arvalid := io.cpu.arvalid && isClintRead
  clint.io.araddr := io.cpu.araddr
  clint.io.rready := io.cpu.rready

  io.cpu.awready := Mux(isUartWrite, uart.io.axi.awready,
    Mux(isClintWrite, clint.io.awready, io.memory.awready))
  io.cpu.wready := Mux(isUartWrite, uart.io.axi.wready,
    Mux(isClintWrite, clint.io.wready, io.memory.wready))
  io.cpu.bvalid := Mux(isUartWrite, uart.io.axi.bvalid,
    Mux(isClintWrite, clint.io.bvalid, io.memory.bvalid))
  io.cpu.bresp := Mux(isUartWrite, uart.io.axi.bresp,
    Mux(isClintWrite, clint.io.bresp, io.memory.bresp))
  io.cpu.arready := Mux(isUartRead, uart.io.axi.arready,
    Mux(isClintRead, clint.io.arready, io.memory.arready))
  io.cpu.rvalid := Mux(isUartRead, uart.io.axi.rvalid,
    Mux(isClintRead, clint.io.rvalid, io.memory.rvalid))
  io.cpu.rdata := Mux(isUartRead, uart.io.axi.rdata,
    Mux(isClintRead, clint.io.rdata, io.memory.rdata))
  io.cpu.rresp := Mux(isUartRead, uart.io.axi.rresp,
    Mux(isClintRead, clint.io.rresp, io.memory.rresp))
  io.uartTxValid := uart.io.txValid
  io.uartTxData := uart.io.txData
}
