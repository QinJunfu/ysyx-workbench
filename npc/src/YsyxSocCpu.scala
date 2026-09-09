package npc

import chisel3._
import chisel3.util._

/** Complete 32-bit AXI4 master interface required by ysyxSoC. */
class Axi4MasterIO extends Bundle {
  val awready = Input(Bool())
  val awvalid = Output(Bool())
  val awaddr  = Output(UInt(32.W))
  val awid    = Output(UInt(4.W))
  val awlen   = Output(UInt(8.W))
  val awsize  = Output(UInt(3.W))
  val awburst = Output(UInt(2.W))

  val wready = Input(Bool())
  val wvalid = Output(Bool())
  val wdata  = Output(UInt(32.W))
  val wstrb  = Output(UInt(4.W))
  val wlast  = Output(Bool())

  val bready = Output(Bool())
  val bvalid = Input(Bool())
  val bresp  = Input(UInt(2.W))
  val bid    = Input(UInt(4.W))

  val arready = Input(Bool())
  val arvalid = Output(Bool())
  val araddr  = Output(UInt(32.W))
  val arid    = Output(UInt(4.W))
  val arlen   = Output(UInt(8.W))
  val arsize  = Output(UInt(3.W))
  val arburst = Output(UInt(2.W))

  val rready = Output(Bool())
  val rvalid = Input(Bool())
  val rresp  = Input(UInt(2.W))
  val rdata  = Input(UInt(32.W))
  val rlast  = Input(Bool())
  val rid    = Input(UInt(4.W))
}

/** AXI4 slave interface is reserved for ChipLink DMA and is unused for now. */
class Axi4SlaveIO extends Bundle {
  val awready = Output(Bool())
  val awvalid = Input(Bool())
  val awaddr  = Input(UInt(32.W))
  val awid    = Input(UInt(4.W))
  val awlen   = Input(UInt(8.W))
  val awsize  = Input(UInt(3.W))
  val awburst = Input(UInt(2.W))

  val wready = Output(Bool())
  val wvalid = Input(Bool())
  val wdata  = Input(UInt(32.W))
  val wstrb  = Input(UInt(4.W))
  val wlast  = Input(Bool())

  val bready = Input(Bool())
  val bvalid = Output(Bool())
  val bresp  = Output(UInt(2.W))
  val bid    = Output(UInt(4.W))

  val arready = Output(Bool())
  val arvalid = Input(Bool())
  val araddr  = Input(UInt(32.W))
  val arid    = Input(UInt(4.W))
  val arlen   = Input(UInt(8.W))
  val arsize  = Input(UInt(3.W))
  val arburst = Input(UInt(2.W))

  val rready = Input(Bool())
  val rvalid = Output(Bool())
  val rresp  = Output(UInt(2.W))
  val rdata  = Output(UInt(32.W))
  val rlast  = Output(Bool())
  val rid    = Output(UInt(4.W))
}

/** Simulation-only retirement callback without adding non-standard CPU ports. */
class NpcCommitDpi extends BlackBox with HasBlackBoxInline {
  val io = IO(new Bundle {
    val clock    = Input(Clock())
    val reset    = Input(Bool())
    val valid    = Input(Bool())
    val pc       = Input(UInt(32.W))
    val inst     = Input(UInt(32.W))
    val dnpc     = Input(UInt(32.W))
    val gprs     = Input(UInt(512.W))
    val memValid = Input(Bool())
    val memWrite = Input(Bool())
    val memAddr  = Input(UInt(32.W))
    val memData  = Input(UInt(32.W))
    val memMask  = Input(UInt(4.W))
    val halt     = Input(Bool())
    val haltCode = Input(UInt(32.W))
    val invalid  = Input(Bool())
  })

  setInline(
    "NpcCommitDpi.sv",
    """module NpcCommitDpi(
      |  input clock, input reset, input valid,
      |  input [31:0] pc, input [31:0] inst, input [31:0] dnpc,
      |  input [511:0] gprs,
      |  input memValid, input memWrite, input [31:0] memAddr,
      |  input [31:0] memData, input [3:0] memMask,
      |  input halt, input [31:0] haltCode, input invalid
      |);
      |`ifndef SYNTHESIS
      |  import "DPI-C" function void npc_commit(
      |    input int pc, input int inst, input int dnpc,
      |    input int gpr0, input int gpr1, input int gpr2, input int gpr3,
      |    input int gpr4, input int gpr5, input int gpr6, input int gpr7,
      |    input int gpr8, input int gpr9, input int gpr10, input int gpr11,
      |    input int gpr12, input int gpr13, input int gpr14, input int gpr15,
      |    input int mem_valid, input int mem_write, input int mem_addr,
      |    input int mem_data, input int mem_mask, input int halt,
      |    input int halt_code, input int invalid
      |  );
      |  always @(posedge clock) begin
      |    if (!reset && valid) begin
      |      npc_commit(
      |        pc, inst, dnpc,
      |        gprs[31:0], gprs[63:32], gprs[95:64], gprs[127:96],
      |        gprs[159:128], gprs[191:160], gprs[223:192], gprs[255:224],
      |        gprs[287:256], gprs[319:288], gprs[351:320], gprs[383:352],
      |        gprs[415:384], gprs[447:416], gprs[479:448], gprs[511:480],
      |        int'(memValid), int'(memWrite), memAddr, memData, int'(memMask),
      |        int'(halt), haltCode, int'(invalid)
      |      );
      |    end
      |  end
      |`endif
      |endmodule
      |""".stripMargin
  )
}

/** ysyxSoC CPU boundary for student ID 24100022. */
class ysyx_24100022(resetPc: BigInt = BigInt("20000000", 16)) extends Module {
  val io_interrupt = IO(Input(Bool()))
  val io_master    = IO(new Axi4MasterIO)
  val io_slave     = IO(new Axi4SlaveIO)

  val core = Module(
    new NPC(
      resetPc = resetPc,
      internalUart = false,
      clintBase = BigInt("0200bff8", 16),
      useNarrowAddresses = true
    )
  )

  core.io.imemData   := 0.U
  core.io.dmemRdata  := 0.U
  core.io.dmemRdata2 := 0.U

  val busActive = !reset.asBool

  io_master.awvalid      := core.io.master.awvalid && busActive
  io_master.awaddr       := core.io.master.awaddr
  io_master.awid         := 0.U
  io_master.awlen        := 0.U
  io_master.awsize       := core.io.master.awsize
  io_master.awburst      := 0.U
  core.io.master.awready := io_master.awready

  io_master.wvalid      := core.io.master.wvalid && busActive
  io_master.wdata       := core.io.master.wdata
  io_master.wstrb       := core.io.master.wstrb
  io_master.wlast       := true.B
  core.io.master.wready := io_master.wready

  io_master.bready      := core.io.master.bready && busActive
  core.io.master.bvalid := io_master.bvalid
  core.io.master.bresp  := io_master.bresp

  io_master.arvalid      := core.io.master.arvalid && busActive
  io_master.araddr       := core.io.master.araddr
  io_master.arid         := 0.U
  io_master.arlen        := 0.U
  io_master.arsize       := core.io.master.arsize
  io_master.arburst      := 0.U
  core.io.master.arready := io_master.arready

  io_master.rready      := core.io.master.rready && busActive
  core.io.master.rvalid := io_master.rvalid
  core.io.master.rdata  := io_master.rdata
  core.io.master.rresp  := io_master.rresp

  io_slave.awready := false.B
  io_slave.wready  := false.B
  io_slave.bvalid  := false.B
  io_slave.bresp   := 0.U
  io_slave.bid     := 0.U
  io_slave.arready := false.B
  io_slave.rvalid  := false.B
  io_slave.rresp   := 0.U
  io_slave.rdata   := 0.U
  io_slave.rlast   := false.B
  io_slave.rid     := 0.U

  dontTouch(io_interrupt)
  dontTouch(io_master.bid)
  dontTouch(io_master.rid)
  dontTouch(io_master.rlast)

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
