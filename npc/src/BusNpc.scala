package npc

import chisel3._
import chisel3.util._

/** Steppable version used by the multi-cycle bus controller. */
class SteppableCoreIO extends Rv32eCoreIO {
  val step = Input(Bool())
}

/** The same five explicit units used by Rv32eCore, with a clock-enable input. The enable is deliberately implemented at
  * the state-owning units only: combinational decode and ALU results remain observable while a bus request is
  * outstanding, but register/PC/CSR state changes only on a commit cycle.
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

  wbu.io.active := ifu.io.active && io.step
  wbu.io.decoded <> idu.io.decoded
  wbu.io.exu <> exu.io.result
  wbu.io.lsu <> lsu.io.result

  exu.io.decoded <> idu.io.decoded
  exu.io.rs1Data      := wbu.io.rs1Data
  exu.io.rs2Data      := wbu.io.rs2Data
  exu.io.csrReadData  := wbu.io.csrReadData
  exu.io.csrSupported := wbu.io.csrSupported
  exu.io.mtvec        := wbu.io.mtvec
  exu.io.mepc         := wbu.io.mepc

  lsu.io.decoded <> idu.io.decoded
  lsu.io.address    := exu.io.result.memoryAddr
  lsu.io.storeData  := wbu.io.rs2Data
  lsu.io.loadValid  := wbu.io.loadValid
  lsu.io.storeValid := wbu.io.storeValid
  lsu.io.readData0  := io.dmemRdata
  lsu.io.readData1  := io.dmemRdata2

  io.imemAddr        := ifu.io.imemAddr
  io.dmemAddr        := lsu.io.result.alignedAddress
  io.dmemLogicalAddr := lsu.io.result.logicalAddress
  io.dmemAccessSize  := lsu.io.result.accessSize
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
  io.retireGprs <> wbu.io.retire.gprs
  io.halt        := wbu.io.retire.halt
  io.haltCode    := wbu.io.retire.haltCode
  io.invalid     := wbu.io.retire.invalid
}

/** Top-level port required by ysyxSoC/spec/cpu-interface.md: an interrupt input, one AXI4 master and one AXI4 slave.
  *
  * The fields are named exactly like that document, so the flattened port names (io_interrupt, io_master_*, io_slave_*)
  * match it. Retirement, tracing and trap detection are deliberately not part of this boundary: the simulation-only
  * NpcCommitDpi tap inside the module exports them through DPI-C, and gate-level synthesis removes that tap, so nothing
  * outside the document's port list is ever exposed.
  */
class NpcIO extends Bundle {
  val interrupt = Input(Bool())
  val master    = new Axi4MasterIO
  val slave     = new Axi4SlaveIO
}

/** Multi-cycle RV32E NPC. IFU and LSU requests are serialized by this explicit arbiter/state machine because the core
  * can have at most one outstanding memory transaction. The module boundary is exactly ysyxSoC/spec/cpu-interface.md;
  * everything else (retirement, DiffTest, the instruction/memory trace and trap detection) leaves the CPU only through
  * the simulation-only NpcCommitDpi tap, which gate-level synthesis removes.
  */
class NPC(
  val resetPc:            BigInt = BigInt("80000000", 16),
  val useNarrowAddresses: Boolean = false)
    extends Module {
  val io = IO(new NpcIO)

  val core      = Module(new SteppableCore(resetPc))
  val imemData  = RegInit(0.U(32.W))
  val dmemData0 = RegInit(0.U(32.W))
  val dmemData1 = RegInit(0.U(32.W))

  core.io.imemData   := imemData
  core.io.dmemRdata  := dmemData0
  core.io.dmemRdata2 := dmemData1

  // State values are named individually to keep the generated RTL readable.
  val sFetchReq   = 0.U(5.W)
  val sFetchResp  = 1.U(5.W)
  val sExec       = 2.U(5.W)
  val sLoadReq0   = 3.U(5.W)
  val sLoadResp0  = 4.U(5.W)
  val sLoadReq1   = 5.U(5.W)
  val sLoadResp1  = 6.U(5.W)
  val sLoadExec   = 7.U(5.W)
  val sStoreReq0  = 8.U(5.W)
  val sStoreResp0 = 9.U(5.W)
  val sStoreReq1  = 10.U(5.W)
  val sStoreResp1 = 11.U(5.W)
  val sStoreExec  = 12.U(5.W)
  val sHalted     = 13.U(5.W)
  val state       = RegInit(sFetchReq)

  val awSent = RegInit(false.B)
  val wSent  = RegInit(false.B)

  val executeCycle = state === sExec && !core.io.dmemReadValid && !core.io.dmemWrite0Valid
  val loadCycle    = state === sLoadExec
  val storeCycle   = state === sStoreExec
  core.io.step := executeCycle || loadCycle || storeCycle

  // Every transaction is a single beat with ID 0, so the AXI4 length, burst and
  // last attributes are constant. The internal controller only drives the
  // address/data/valid/ready subset below.
  io.master.awvalid := false.B
  io.master.awaddr  := 0.U
  io.master.awid    := 0.U
  io.master.awlen   := 0.U
  io.master.awsize  := 2.U
  io.master.awburst := 0.U
  io.master.wvalid  := false.B
  io.master.wdata   := 0.U
  io.master.wstrb   := 0.U
  io.master.wlast   := true.B
  io.master.bready  := false.B
  io.master.arvalid := false.B
  io.master.araddr  := 0.U
  io.master.arid    := 0.U
  io.master.arlen   := 0.U
  io.master.arsize  := 2.U
  io.master.arburst := 0.U
  io.master.rready  := false.B

  val loadAddress0  = if (useNarrowAddresses) {
    Mux(core.io.dmemReadValid2, core.io.dmemAddr, core.io.dmemLogicalAddr)
  } else {
    core.io.dmemAddr
  }
  val storeAddress0 = if (useNarrowAddresses) {
    Mux(core.io.dmemWrite1Valid, core.io.dmemWrite0Addr, core.io.dmemLogicalAddr)
  } else {
    core.io.dmemWrite0Addr
  }

  // Keep the write address stable until the write response is accepted.
  when(state === sStoreReq0 || state === sStoreResp0) {
    io.master.awaddr := storeAddress0
    io.master.awsize := Mux(core.io.dmemWrite1Valid, 2.U, core.io.dmemAccessSize)
  }.elsewhen(state === sStoreReq1 || state === sStoreResp1) {
    io.master.awaddr := core.io.dmemWrite1Addr
    io.master.awsize := 2.U
  }

  when(state === sFetchReq) {
    io.master.arvalid := true.B
    io.master.araddr  := core.io.imemAddr
    io.master.arsize  := 2.U
  }.elsewhen(state === sFetchResp) {
    io.master.araddr := core.io.imemAddr
    io.master.arsize := 2.U
    io.master.rready := true.B
  }.elsewhen(state === sLoadReq0) {
    io.master.arvalid := true.B
    io.master.araddr  := loadAddress0
    io.master.arsize  := Mux(core.io.dmemReadValid2, 2.U, core.io.dmemAccessSize)
  }.elsewhen(state === sLoadResp0) {
    io.master.araddr := loadAddress0
    io.master.arsize := Mux(core.io.dmemReadValid2, 2.U, core.io.dmemAccessSize)
    io.master.rready := true.B
  }.elsewhen(state === sLoadReq1) {
    io.master.arvalid := true.B
    io.master.araddr  := core.io.dmemAddr2
    io.master.arsize  := 2.U
  }.elsewhen(state === sLoadResp1) {
    io.master.araddr := core.io.dmemAddr2
    io.master.arsize := 2.U
    io.master.rready := true.B
  }.elsewhen(state === sStoreReq0) {
    io.master.awvalid := !awSent
    io.master.awaddr  := storeAddress0
    io.master.wvalid  := !wSent
    io.master.wdata   := core.io.dmemWrite0Data
    io.master.wstrb   := core.io.dmemWrite0Mask
  }.elsewhen(state === sStoreResp0) {
    io.master.bready := true.B
  }.elsewhen(state === sStoreReq1) {
    io.master.awvalid := !awSent
    io.master.awaddr  := core.io.dmemWrite1Addr
    io.master.wvalid  := !wSent
    io.master.wdata   := core.io.dmemWrite1Data
    io.master.wstrb   := core.io.dmemWrite1Mask
  }.elsewhen(state === sStoreResp1) {
    io.master.bready := true.B
  }

  when(state === sFetchReq) {
    when(io.master.arvalid && io.master.arready) {
      state := sFetchResp
    }
  }.elsewhen(state === sFetchResp) {
    when(io.master.rvalid && io.master.rready) {
      imemData := io.master.rdata
      state    := sExec
    }
  }.elsewhen(state === sExec) {
    when(core.io.dmemReadValid) {
      state := sLoadReq0
    }.elsewhen(core.io.dmemWrite0Valid) {
      awSent := false.B
      wSent  := false.B
      state  := sStoreReq0
    }.otherwise {
      when(core.io.halt) {
        state := sHalted
      }.otherwise {
        state := sFetchReq
      }
    }
  }.elsewhen(state === sLoadReq0) {
    when(io.master.arvalid && io.master.arready) {
      state := sLoadResp0
    }
  }.elsewhen(state === sLoadResp0) {
    when(io.master.rvalid && io.master.rready) {
      dmemData0 := io.master.rdata
      when(core.io.dmemReadValid2) {
        state := sLoadReq1
      }.otherwise {
        state := sLoadExec
      }
    }
  }.elsewhen(state === sLoadReq1) {
    when(io.master.arvalid && io.master.arready) {
      state := sLoadResp1
    }
  }.elsewhen(state === sLoadResp1) {
    when(io.master.rvalid && io.master.rready) {
      dmemData1 := io.master.rdata
      state     := sLoadExec
    }
  }.elsewhen(state === sLoadExec) {
    state := sFetchReq
  }.elsewhen(state === sStoreReq0) {
    when(io.master.awvalid && io.master.awready) {
      awSent := true.B
    }
    when(io.master.wvalid && io.master.wready) {
      wSent := true.B
    }
    when((awSent || io.master.awready) && (wSent || io.master.wready)) {
      state := sStoreResp0
    }
  }.elsewhen(state === sStoreResp0) {
    when(io.master.bvalid && io.master.bready) {
      when(core.io.dmemWrite1Valid) {
        awSent := false.B
        wSent  := false.B
        state  := sStoreReq1
      }.otherwise {
        state := sStoreExec
      }
    }
  }.elsewhen(state === sStoreReq1) {
    when(io.master.awvalid && io.master.awready) {
      awSent := true.B
    }
    when(io.master.wvalid && io.master.wready) {
      wSent := true.B
    }
    when((awSent || io.master.awready) && (wSent || io.master.wready)) {
      state := sStoreResp1
    }
  }.elsewhen(state === sStoreResp1) {
    when(io.master.bvalid && io.master.bready) {
      state := sStoreExec
    }
  }.elsewhen(state === sStoreExec) {
    state := sFetchReq
  }

  // The AXI4 slave port is reserved for the ChipLink DMA engine; until that
  // path exists the CPU never accepts a slave transaction.
  io.slave.awready := false.B
  io.slave.wready  := false.B
  io.slave.bvalid  := false.B
  io.slave.bresp   := 0.U
  io.slave.bid     := 0.U
  io.slave.arready := false.B
  io.slave.rvalid  := false.B
  io.slave.rresp   := 0.U
  io.slave.rdata   := 0.U
  io.slave.rlast   := false.B
  io.slave.rid     := 0.U

  // Interrupt delivery is not implemented yet; keep the required port alive.
  dontTouch(io.interrupt)

  // Retirement, trace and trap data leave the CPU only through this
  // simulation-only tap, so they never appear in the standard port list.
  // Gate-level synthesis drops the output-less black box, leaving a pure
  // standard-cell netlist with exactly the cpu-interface.md boundary.
  val commit = Module(new NpcCommitDpi)
  commit.io.clock    := clock
  commit.io.reset    := reset.asBool
  commit.io.valid    := core.io.retireValid
  commit.io.pc       := core.io.retirePc
  commit.io.inst     := core.io.retireInst
  commit.io.dnpc     := core.io.retireDnPc
  commit.io.gprs     := Cat(core.io.retireGprs.reverse)
  commit.io.memValid := core.io.memTraceValid
  commit.io.memWrite := core.io.memTraceWrite
  commit.io.memAddr  := core.io.memTraceAddr
  commit.io.memData  := core.io.memTraceData
  commit.io.memMask  := core.io.memTraceMask
  commit.io.halt     := core.io.halt
  commit.io.haltCode := core.io.haltCode
  commit.io.invalid  := core.io.invalid
}
