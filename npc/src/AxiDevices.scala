package npc

import chisel3._

/** AXI4-Lite slave directions, written out to make each handshake visible. */
class AxiLiteSlaveIO extends Bundle {
  // AW
  val awvalid = Input(Bool())
  val awready = Output(Bool())
  val awaddr  = Input(UInt(32.W))
  val awsize  = Input(UInt(3.W))
  // W
  val wvalid  = Input(Bool())
  val wready  = Output(Bool())
  val wdata   = Input(UInt(32.W))
  val wstrb   = Input(UInt(4.W))
  // B
  val bvalid  = Output(Bool())
  val bready  = Input(Bool())
  val bresp   = Output(UInt(2.W))
  // AR
  val arvalid = Input(Bool())
  val arready = Output(Bool())
  val araddr  = Input(UInt(32.W))
  val arsize  = Input(UInt(3.W))
  // R
  val rvalid  = Output(Bool())
  val rready  = Input(Bool())
  val rdata   = Output(UInt(32.W))
  val rresp   = Output(UInt(2.W))
}

/** UART device register at 0x10000000. */
class AxiLiteUart extends Module {
  val io = IO(new Bundle {
    val axi     = new AxiLiteSlaveIO
    val txValid = Output(Bool())
    val txData  = Output(UInt(8.W))
  })

  val awHold  = RegInit(false.B)
  val wHold   = RegInit(false.B)
  val awAddr  = RegInit(0.U(32.W))
  val wData   = RegInit(0.U(32.W))
  val wStrb   = RegInit(0.U(4.W))
  val bHold   = RegInit(false.B)
  val rHold   = RegInit(false.B)
  val rData   = RegInit(0.U(32.W))
  val txPulse = RegInit(false.B)
  val txByte  = RegInit(0.U(8.W))

  io.axi.awready := !awHold && !bHold
  io.axi.wready  := !wHold && !bHold
  io.axi.bvalid  := bHold
  io.axi.bresp   := 0.U
  io.axi.arready := !rHold
  io.axi.rvalid  := rHold
  io.axi.rdata   := rData
  io.axi.rresp   := 0.U
  io.txValid     := txPulse
  io.txData      := txByte

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
  when(
    !bHold && (awHold || (io.axi.awvalid && io.axi.awready)) &&
      (wHold || (io.axi.wvalid && io.axi.wready))
  ) {
    bHold  := true.B
    when(
      Mux(awHold, awAddr, io.axi.awaddr) === "h10000000".U &&
        Mux(wHold, wStrb, io.axi.wstrb)(0)
    ) {
      txPulse := true.B
      txByte  := Mux(wHold, wData, io.axi.wdata)(7, 0)
    }
    awHold := false.B
    wHold  := false.B
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
class AxiLiteClint(val baseAddress: BigInt) extends Module {
  val io     = IO(new AxiLiteSlaveIO)
  val mtime  = RegInit(0.U(64.W))
  val rHold  = RegInit(false.B)
  val rData  = RegInit(0.U(32.W))
  val awHold = RegInit(false.B)
  val wHold  = RegInit(false.B)
  val bHold  = RegInit(false.B)

  io.awready := !awHold && !bHold
  io.wready  := !wHold && !bHold
  io.bvalid  := bHold
  io.bresp   := 0.U
  io.arready := !rHold
  io.rvalid  := rHold
  io.rdata   := rData
  io.rresp   := 0.U

  mtime := mtime + 1.U
  when(io.awvalid && io.awready) {
    awHold := true.B
  }
  when(io.wvalid && io.wready) {
    wHold := true.B
  }
  when(
    !bHold && (awHold || (io.awvalid && io.awready)) &&
      (wHold || (io.wvalid && io.wready))
  ) {
    awHold := false.B
    wHold  := false.B
    bHold  := true.B
  }
  when(io.arvalid && io.arready) {
    rHold := true.B
    when(io.araddr === baseAddress.U) {
      rData := mtime(31, 0)
    }.elsewhen(io.araddr === (baseAddress + 4).U) {
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

/** Address decoder for one CPU master and one SRAM slave. UART and CLINT are instantiated here, so adding a device does
  * not change the CPU protocol. The current NPC issues one request at a time and presents AW/W together; this keeps the
  * decoder deliberately small while still honoring every AXI4-Lite handshake.
  */
class AxiLiteXbar(val includeUart: Boolean, val clintBase: BigInt) extends Module {
  val io = IO(new Bundle {
    val cpu         = Flipped(new AxiLiteMasterIO)
    val memory      = new AxiLiteMasterIO
    val uartTxValid = Output(Bool())
    val uartTxData  = Output(UInt(8.W))
  })

  val uart         = if (includeUart) Some(Module(new AxiLiteUart)) else None
  val clint        = Module(new AxiLiteClint(clintBase))
  val isUartWrite  = if (includeUart) io.cpu.awaddr === "h10000000".U else false.B
  val isClintWrite = io.cpu.awaddr === clintBase.U || io.cpu.awaddr === (clintBase + 4).U
  val isUartRead   = if (includeUart) {
    io.cpu.araddr >= "h10000000".U && io.cpu.araddr < "h10001000".U
  } else {
    false.B
  }
  val isClintRead  = io.cpu.araddr === clintBase.U || io.cpu.araddr === (clintBase + 4).U

  io.memory.awvalid := io.cpu.awvalid && !isUartWrite && !isClintWrite
  io.memory.awaddr  := io.cpu.awaddr
  io.memory.awsize  := io.cpu.awsize
  io.memory.wvalid  := io.cpu.wvalid && !isUartWrite && !isClintWrite
  io.memory.wdata   := io.cpu.wdata
  io.memory.wstrb   := io.cpu.wstrb
  io.memory.bready  := io.cpu.bready
  io.memory.arvalid := io.cpu.arvalid && !isUartRead && !isClintRead
  io.memory.araddr  := io.cpu.araddr
  io.memory.arsize  := io.cpu.arsize
  io.memory.rready  := io.cpu.rready

  uart.foreach { device =>
    device.io.axi.awvalid := io.cpu.awvalid && isUartWrite
    device.io.axi.awaddr  := io.cpu.awaddr
    device.io.axi.awsize  := io.cpu.awsize
    device.io.axi.wvalid  := io.cpu.wvalid && isUartWrite
    device.io.axi.wdata   := io.cpu.wdata
    device.io.axi.wstrb   := io.cpu.wstrb
    device.io.axi.bready  := io.cpu.bready
    device.io.axi.arvalid := io.cpu.arvalid && isUartRead
    device.io.axi.araddr  := io.cpu.araddr
    device.io.axi.arsize  := io.cpu.arsize
    device.io.axi.rready  := io.cpu.rready
  }

  clint.io.awvalid := io.cpu.awvalid && isClintWrite
  clint.io.awaddr  := io.cpu.awaddr
  clint.io.awsize  := io.cpu.awsize
  clint.io.wvalid  := io.cpu.wvalid && isClintWrite
  clint.io.wdata   := io.cpu.wdata
  clint.io.wstrb   := io.cpu.wstrb
  clint.io.bready  := io.cpu.bready
  clint.io.arvalid := io.cpu.arvalid && isClintRead
  clint.io.araddr  := io.cpu.araddr
  clint.io.arsize  := io.cpu.arsize
  clint.io.rready  := io.cpu.rready

  val uartAwReady = uart.map(_.io.axi.awready).getOrElse(false.B)
  val uartWReady  = uart.map(_.io.axi.wready).getOrElse(false.B)
  val uartBValid  = uart.map(_.io.axi.bvalid).getOrElse(false.B)
  val uartBResp   = uart.map(_.io.axi.bresp).getOrElse(0.U)
  val uartArReady = uart.map(_.io.axi.arready).getOrElse(false.B)
  val uartRValid  = uart.map(_.io.axi.rvalid).getOrElse(false.B)
  val uartRData   = uart.map(_.io.axi.rdata).getOrElse(0.U)
  val uartRResp   = uart.map(_.io.axi.rresp).getOrElse(0.U)

  io.cpu.awready := Mux(isUartWrite, uartAwReady, Mux(isClintWrite, clint.io.awready, io.memory.awready))
  io.cpu.wready  := Mux(isUartWrite, uartWReady, Mux(isClintWrite, clint.io.wready, io.memory.wready))
  io.cpu.bvalid  := Mux(isUartWrite, uartBValid, Mux(isClintWrite, clint.io.bvalid, io.memory.bvalid))
  io.cpu.bresp   := Mux(isUartWrite, uartBResp, Mux(isClintWrite, clint.io.bresp, io.memory.bresp))
  io.cpu.arready := Mux(isUartRead, uartArReady, Mux(isClintRead, clint.io.arready, io.memory.arready))
  io.cpu.rvalid  := Mux(isUartRead, uartRValid, Mux(isClintRead, clint.io.rvalid, io.memory.rvalid))
  io.cpu.rdata   := Mux(isUartRead, uartRData, Mux(isClintRead, clint.io.rdata, io.memory.rdata))
  io.cpu.rresp   := Mux(isUartRead, uartRResp, Mux(isClintRead, clint.io.rresp, io.memory.rresp))
  io.uartTxValid := uart.map(_.io.txValid).getOrElse(false.B)
  io.uartTxData  := uart.map(_.io.txData).getOrElse(0.U)
}
