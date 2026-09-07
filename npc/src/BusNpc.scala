package npc

import chisel3._
import chisel3.util._

/**
  * Flat AXI4-Lite master port.  Keeping the channels explicit makes the
  * handshake state machines visible in the generated RTL.
  */
class AxiLiteMasterIO extends Bundle {
  val awvalid = Output(Bool())
  val awready = Input(Bool())
  val awaddr  = Output(UInt(32.W))

  val wvalid = Output(Bool())
  val wready = Input(Bool())
  val wdata  = Output(UInt(32.W))
  val wstrb  = Output(UInt(4.W))

  val bvalid = Input(Bool())
  val bready = Output(Bool())
  val bresp  = Input(UInt(2.W))

  val arvalid = Output(Bool())
  val arready = Input(Bool())
  val araddr  = Output(UInt(32.W))

  val rvalid = Input(Bool())
  val rready = Output(Bool())
  val rdata  = Input(UInt(32.W))
  val rresp  = Input(UInt(2.W))
}

/** Steppable version used by the multi-cycle bus controller. */
class SteppableCoreIO extends Rv32eCoreIO {
  val step = Input(Bool())
}

/**
  * The same five explicit units used by Rv32eCore, with a clock-enable input.
  * The enable is deliberately implemented at the state-owning units only:
  * combinational decode and ALU results remain observable while a bus request
  * is outstanding, but register/PC/CSR state changes only on a commit cycle.
  */
class SteppableCore(val resetPc: BigInt = BigInt("80000000", 16)) extends Module {
  val io = IO(new SteppableCoreIO)

  val ifu = Module(new IFU(resetPc))
  val idu = Module(new IDU)
  val exu = Module(new EXU)
  val lsu = Module(new LSU)
  val wbu = Module(new WBU(resetPc))

  ifu.io.imemData := io.imemData
  ifu.io.run      := io.step
  ifu.io.nextPc   := wbu.io.nextPc
  ifu.io.halt     := wbu.io.halt

  idu.io.pc   := ifu.io.pc
  idu.io.inst := ifu.io.inst

  wbu.io.active  := ifu.io.active && io.step
  wbu.io.decoded := idu.io.decoded
  wbu.io.exu     := exu.io.result
  wbu.io.lsu     := lsu.io.result

  exu.io.decoded      := idu.io.decoded
  exu.io.rs1Data      := wbu.io.rs1Data
  exu.io.rs2Data      := wbu.io.rs2Data
  exu.io.csrReadData  := wbu.io.csrReadData
  exu.io.csrSupported := wbu.io.csrSupported
  exu.io.mtvec        := wbu.io.mtvec
  exu.io.mepc         := wbu.io.mepc

  lsu.io.decoded    := idu.io.decoded
  lsu.io.address    := exu.io.result.memoryAddr
  lsu.io.storeData  := wbu.io.rs2Data
  lsu.io.loadValid  := wbu.io.loadValid
  lsu.io.storeValid := wbu.io.storeValid
  lsu.io.readData0  := io.dmemRdata
  lsu.io.readData1  := io.dmemRdata2

  io.imemAddr        := ifu.io.imemAddr
  io.dmemAddr        := lsu.io.result.alignedAddress
  io.dmemAddr2       := lsu.io.result.address2
  io.dmemReadValid   := lsu.io.result.read0Valid
  io.dmemReadValid2  := lsu.io.result.read1Valid
  io.dmemWrite0Valid := lsu.io.result.write0Valid
  io.dmemWrite0Addr  := lsu.io.result.alignedAddress
  io.dmemWrite0Data  := lsu.io.result.write0Data
  io.dmemWrite0Mask  := lsu.io.result.write0Mask
  io.dmemWrite1Valid := lsu.io.result.write1Valid
  io.dmemWrite1Addr  := lsu.io.result.address2
  io.dmemWrite1Data  := lsu.io.result.write1Data
  io.dmemWrite1Mask  := lsu.io.result.write1Mask

  io.memTraceValid := wbu.io.retire.memTraceValid
  io.memTraceWrite := wbu.io.retire.memTraceWrite
  io.memTraceAddr  := wbu.io.retire.memTraceAddr
  io.memTraceData  := wbu.io.retire.memTraceData
  io.memTraceMask  := wbu.io.retire.memTraceMask

  io.retireValid := wbu.io.retire.valid
  io.retirePc    := wbu.io.retire.pc
  io.retireInst  := wbu.io.retire.inst
  io.retireDnPc  := wbu.io.retire.nextPc
  io.retireGpr0  := wbu.io.retire.gprs(0)
  io.retireGpr1  := wbu.io.retire.gprs(1)
  io.retireGpr2  := wbu.io.retire.gprs(2)
  io.retireGpr3  := wbu.io.retire.gprs(3)
  io.retireGpr4  := wbu.io.retire.gprs(4)
  io.retireGpr5  := wbu.io.retire.gprs(5)
  io.retireGpr6  := wbu.io.retire.gprs(6)
  io.retireGpr7  := wbu.io.retire.gprs(7)
  io.retireGpr8  := wbu.io.retire.gprs(8)
  io.retireGpr9  := wbu.io.retire.gprs(9)
  io.retireGpr10 := wbu.io.retire.gprs(10)
  io.retireGpr11 := wbu.io.retire.gprs(11)
  io.retireGpr12 := wbu.io.retire.gprs(12)
  io.retireGpr13 := wbu.io.retire.gprs(13)
  io.retireGpr14 := wbu.io.retire.gprs(14)
  io.retireGpr15 := wbu.io.retire.gprs(15)
  io.halt        := wbu.io.retire.halt
  io.haltCode    := wbu.io.retire.haltCode
  io.invalid     := wbu.io.retire.invalid
}

/** Top-level port: the legacy debug/retire view plus one AXI4-Lite master. */
class NpcIO extends Rv32eCoreIO {
  val master = new AxiLiteMasterIO
  val uartTxValid = Output(Bool())
  val uartTxData = Output(UInt(8.W))
}

/**
  * Multi-cycle RV32E NPC.  IFU and LSU requests are serialized by this
  * explicit arbiter/state machine because the core can have at most one
  * outstanding memory transaction.  The external port is a conventional
  * AXI4-Lite master; read and write responses are accepted only when their
  * corresponding ready signal is asserted.
  */
class NPC(val resetPc: BigInt = BigInt("80000000", 16)) extends Module {
  val io = IO(new NpcIO)

  val core = Module(new SteppableCore(resetPc))
  val xbar = Module(new AxiLiteXbar)
  val imemData = RegInit(0.U(32.W))
  val dmemData0 = RegInit(0.U(32.W))
  val dmemData1 = RegInit(0.U(32.W))

  core.io.imemData := imemData
  core.io.dmemRdata := dmemData0
  core.io.dmemRdata2 := dmemData1

  // State values are named individually to keep the generated RTL readable.
  val sFetchReq  = 0.U(5.W)
  val sFetchResp = 1.U(5.W)
  val sExec      = 2.U(5.W)
  val sLoadReq0  = 3.U(5.W)
  val sLoadResp0 = 4.U(5.W)
  val sLoadReq1  = 5.U(5.W)
  val sLoadResp1 = 6.U(5.W)
  val sLoadExec  = 7.U(5.W)
  val sStoreReq0 = 8.U(5.W)
  val sStoreResp0 = 9.U(5.W)
  val sStoreReq1 = 10.U(5.W)
  val sStoreResp1 = 11.U(5.W)
  val sStoreExec = 12.U(5.W)
  val sHalted    = 13.U(5.W)
  val state = RegInit(sFetchReq)

  val awSent = RegInit(false.B)
  val wSent  = RegInit(false.B)

  val executeCycle = state === sExec && !core.io.dmemReadValid && !core.io.dmemWrite0Valid
  val loadCycle    = state === sLoadExec
  val storeCycle   = state === sStoreExec
  core.io.step := executeCycle || loadCycle || storeCycle

  xbar.io.cpu.awvalid := false.B
  xbar.io.cpu.awaddr  := 0.U
  xbar.io.cpu.wvalid  := false.B
  xbar.io.cpu.wdata   := 0.U
  xbar.io.cpu.wstrb   := 0.U
  xbar.io.cpu.bready  := false.B
  xbar.io.cpu.arvalid := false.B
  xbar.io.cpu.araddr  := 0.U
  xbar.io.cpu.rready  := false.B

  // Keep the write address stable through the B response.  The Xbar uses the
  // address to select the response channel, and AXI requires request fields
  // to remain stable until their handshake has completed.
  when(state === sStoreReq0 || state === sStoreResp0) {
    xbar.io.cpu.awaddr := core.io.dmemWrite0Addr
  }.elsewhen(state === sStoreReq1 || state === sStoreResp1) {
    xbar.io.cpu.awaddr := core.io.dmemWrite1Addr
  }

  when(state === sFetchReq) {
    xbar.io.cpu.arvalid := true.B
    xbar.io.cpu.araddr  := core.io.imemAddr
  }.elsewhen(state === sFetchResp) {
    xbar.io.cpu.rready := true.B
  }.elsewhen(state === sLoadReq0) {
    xbar.io.cpu.arvalid := true.B
    xbar.io.cpu.araddr  := core.io.dmemAddr
  }.elsewhen(state === sLoadResp0) {
    xbar.io.cpu.rready := true.B
  }.elsewhen(state === sLoadReq1) {
    xbar.io.cpu.arvalid := true.B
    xbar.io.cpu.araddr  := core.io.dmemAddr2
  }.elsewhen(state === sLoadResp1) {
    xbar.io.cpu.rready := true.B
  }.elsewhen(state === sStoreReq0) {
    xbar.io.cpu.awvalid := !awSent
    xbar.io.cpu.awaddr  := core.io.dmemWrite0Addr
    xbar.io.cpu.wvalid  := !wSent
    xbar.io.cpu.wdata   := core.io.dmemWrite0Data
    xbar.io.cpu.wstrb   := core.io.dmemWrite0Mask
  }.elsewhen(state === sStoreResp0) {
    xbar.io.cpu.bready := true.B
  }.elsewhen(state === sStoreReq1) {
    xbar.io.cpu.awvalid := !awSent
    xbar.io.cpu.awaddr  := core.io.dmemWrite1Addr
    xbar.io.cpu.wvalid  := !wSent
    xbar.io.cpu.wdata   := core.io.dmemWrite1Data
    xbar.io.cpu.wstrb   := core.io.dmemWrite1Mask
  }.elsewhen(state === sStoreResp1) {
    xbar.io.cpu.bready := true.B
  }

  when(state === sFetchReq) {
    when(xbar.io.cpu.arvalid && xbar.io.cpu.arready) {
      state := sFetchResp
    }
  }.elsewhen(state === sFetchResp) {
    when(xbar.io.cpu.rvalid && xbar.io.cpu.rready) {
      imemData := xbar.io.cpu.rdata
      state := sExec
    }
  }.elsewhen(state === sExec) {
    when(core.io.dmemReadValid) {
      state := sLoadReq0
    }.elsewhen(core.io.dmemWrite0Valid) {
      awSent := false.B
      wSent := false.B
      state := sStoreReq0
    }.otherwise {
      when(core.io.halt) {
        state := sHalted
      }.otherwise {
        state := sFetchReq
      }
    }
  }.elsewhen(state === sLoadReq0) {
    when(xbar.io.cpu.arvalid && xbar.io.cpu.arready) {
      state := sLoadResp0
    }
  }.elsewhen(state === sLoadResp0) {
    when(xbar.io.cpu.rvalid && xbar.io.cpu.rready) {
      dmemData0 := xbar.io.cpu.rdata
      when(core.io.dmemReadValid2) {
        state := sLoadReq1
      }.otherwise {
        state := sLoadExec
      }
    }
  }.elsewhen(state === sLoadReq1) {
    when(xbar.io.cpu.arvalid && xbar.io.cpu.arready) {
      state := sLoadResp1
    }
  }.elsewhen(state === sLoadResp1) {
    when(xbar.io.cpu.rvalid && xbar.io.cpu.rready) {
      dmemData1 := xbar.io.cpu.rdata
      state := sLoadExec
    }
  }.elsewhen(state === sLoadExec) {
    state := sFetchReq
  }.elsewhen(state === sStoreReq0) {
    when(xbar.io.cpu.awvalid && xbar.io.cpu.awready) {
      awSent := true.B
    }
    when(xbar.io.cpu.wvalid && xbar.io.cpu.wready) {
      wSent := true.B
    }
    when((awSent || xbar.io.cpu.awready) && (wSent || xbar.io.cpu.wready)) {
      state := sStoreResp0
    }
  }.elsewhen(state === sStoreResp0) {
    when(xbar.io.cpu.bvalid && xbar.io.cpu.bready) {
      when(core.io.dmemWrite1Valid) {
        awSent := false.B
        wSent := false.B
        state := sStoreReq1
      }.otherwise {
        state := sStoreExec
      }
    }
  }.elsewhen(state === sStoreReq1) {
    when(xbar.io.cpu.awvalid && xbar.io.cpu.awready) {
      awSent := true.B
    }
    when(xbar.io.cpu.wvalid && xbar.io.cpu.wready) {
      wSent := true.B
    }
    when((awSent || xbar.io.cpu.awready) && (wSent || xbar.io.cpu.wready)) {
      state := sStoreResp1
    }
  }.elsewhen(state === sStoreResp1) {
    when(xbar.io.cpu.bvalid && xbar.io.cpu.bready) {
      state := sStoreExec
    }
  }.elsewhen(state === sStoreExec) {
    state := sFetchReq
  }

  // The Xbar owns the UART/CLINT address decode.  Its memory side is the
  // external AXI port exported by NPC.
  io.master.awvalid := xbar.io.memory.awvalid
  io.master.awaddr := xbar.io.memory.awaddr
  io.master.wvalid := xbar.io.memory.wvalid
  io.master.wdata := xbar.io.memory.wdata
  io.master.wstrb := xbar.io.memory.wstrb
  io.master.bready := xbar.io.memory.bready
  io.master.arvalid := xbar.io.memory.arvalid
  io.master.araddr := xbar.io.memory.araddr
  io.master.rready := xbar.io.memory.rready
  xbar.io.memory.awready := io.master.awready
  xbar.io.memory.wready := io.master.wready
  xbar.io.memory.bvalid := io.master.bvalid
  xbar.io.memory.bresp := io.master.bresp
  xbar.io.memory.arready := io.master.arready
  xbar.io.memory.rvalid := io.master.rvalid
  xbar.io.memory.rdata := io.master.rdata
  xbar.io.memory.rresp := io.master.rresp
  io.uartTxValid := xbar.io.uartTxValid
  io.uartTxData := xbar.io.uartTxData

  // Preserve the existing retire/DPI view for the simulator and unit users.
  io.imemAddr := core.io.imemAddr
  io.dmemAddr := core.io.dmemAddr
  io.dmemReadValid := core.io.dmemReadValid
  io.dmemAddr2 := core.io.dmemAddr2
  io.dmemReadValid2 := core.io.dmemReadValid2
  io.dmemWrite0Valid := core.io.dmemWrite0Valid
  io.dmemWrite0Addr := core.io.dmemWrite0Addr
  io.dmemWrite0Data := core.io.dmemWrite0Data
  io.dmemWrite0Mask := core.io.dmemWrite0Mask
  io.dmemWrite1Valid := core.io.dmemWrite1Valid
  io.dmemWrite1Addr := core.io.dmemWrite1Addr
  io.dmemWrite1Data := core.io.dmemWrite1Data
  io.dmemWrite1Mask := core.io.dmemWrite1Mask
  io.memTraceValid := core.io.memTraceValid
  io.memTraceWrite := core.io.memTraceWrite
  io.memTraceAddr := core.io.memTraceAddr
  io.memTraceData := core.io.memTraceData
  io.memTraceMask := core.io.memTraceMask
  io.retireValid := core.io.retireValid
  io.retirePc := core.io.retirePc
  io.retireInst := core.io.retireInst
  io.retireDnPc := core.io.retireDnPc
  io.retireGpr0 := core.io.retireGpr0
  io.retireGpr1 := core.io.retireGpr1
  io.retireGpr2 := core.io.retireGpr2
  io.retireGpr3 := core.io.retireGpr3
  io.retireGpr4 := core.io.retireGpr4
  io.retireGpr5 := core.io.retireGpr5
  io.retireGpr6 := core.io.retireGpr6
  io.retireGpr7 := core.io.retireGpr7
  io.retireGpr8 := core.io.retireGpr8
  io.retireGpr9 := core.io.retireGpr9
  io.retireGpr10 := core.io.retireGpr10
  io.retireGpr11 := core.io.retireGpr11
  io.retireGpr12 := core.io.retireGpr12
  io.retireGpr13 := core.io.retireGpr13
  io.retireGpr14 := core.io.retireGpr14
  io.retireGpr15 := core.io.retireGpr15
  io.halt := core.io.halt
  io.haltCode := core.io.haltCode
  io.invalid := core.io.invalid
}

/** A compact explicit arbiter useful for later SoC integration. */
class AxiLiteArbiter extends Module {
  val io = IO(new Bundle {
    val ifu = Flipped(new AxiLiteMasterIO)
    val lsu = Flipped(new AxiLiteMasterIO)
    val out = new AxiLiteMasterIO
  })

  val ownerIfu = 0.U(1.W)
  val ownerLsu = 1.U(1.W)
  val owner = RegInit(ownerIfu)
  val busy = RegInit(false.B)

  io.out.awvalid := false.B
  io.out.awaddr := 0.U
  io.out.wvalid := false.B
  io.out.wdata := 0.U
  io.out.wstrb := 0.U
  io.out.bready := false.B
  io.out.arvalid := false.B
  io.out.araddr := 0.U
  io.out.rready := false.B
  io.ifu.awready := false.B
  io.ifu.wready := false.B
  io.ifu.bvalid := false.B
  io.ifu.bresp := 0.U
  io.ifu.arready := false.B
  io.ifu.rvalid := false.B
  io.ifu.rdata := 0.U
  io.ifu.rresp := 0.U
  io.lsu.awready := false.B
  io.lsu.wready := false.B
  io.lsu.bvalid := false.B
  io.lsu.bresp := 0.U
  io.lsu.arready := false.B
  io.lsu.rvalid := false.B
  io.lsu.rdata := 0.U
  io.lsu.rresp := 0.U

  when(!busy) {
    when(io.ifu.arvalid || io.ifu.awvalid || io.ifu.wvalid) {
      owner := ownerIfu
      busy := true.B
    }.elsewhen(io.lsu.arvalid || io.lsu.awvalid || io.lsu.wvalid) {
      owner := ownerLsu
      busy := true.B
    }
  }.otherwise {
    when(owner === ownerIfu &&
      ((io.out.rvalid && io.out.rready) || (io.out.bvalid && io.out.bready))) {
      busy := false.B
    }
    when(owner === ownerLsu &&
      ((io.out.rvalid && io.out.rready) || (io.out.bvalid && io.out.bready))) {
      busy := false.B
    }
  }

  when(owner === ownerIfu) {
    io.out.awvalid := io.ifu.awvalid && busy
    io.out.awaddr := io.ifu.awaddr
    io.out.wvalid := io.ifu.wvalid && busy
    io.out.wdata := io.ifu.wdata
    io.out.wstrb := io.ifu.wstrb
    io.out.bready := io.ifu.bready
    io.out.arvalid := io.ifu.arvalid && busy
    io.out.araddr := io.ifu.araddr
    io.out.rready := io.ifu.rready
    io.ifu.awready := io.out.awready && busy
    io.ifu.wready := io.out.wready && busy
    io.ifu.bvalid := io.out.bvalid
    io.ifu.bresp := io.out.bresp
    io.ifu.arready := io.out.arready && busy
    io.ifu.rvalid := io.out.rvalid
    io.ifu.rdata := io.out.rdata
    io.ifu.rresp := io.out.rresp
  }.otherwise {
    io.out.awvalid := io.lsu.awvalid && busy
    io.out.awaddr := io.lsu.awaddr
    io.out.wvalid := io.lsu.wvalid && busy
    io.out.wdata := io.lsu.wdata
    io.out.wstrb := io.lsu.wstrb
    io.out.bready := io.lsu.bready
    io.out.arvalid := io.lsu.arvalid && busy
    io.out.araddr := io.lsu.araddr
    io.out.rready := io.lsu.rready
    io.lsu.awready := io.out.awready && busy
    io.lsu.wready := io.out.wready && busy
    io.lsu.bvalid := io.out.bvalid
    io.lsu.bresp := io.out.bresp
    io.lsu.arready := io.out.arready && busy
    io.lsu.rvalid := io.out.rvalid
    io.lsu.rdata := io.out.rdata
    io.lsu.rresp := io.out.rresp
  }
}
