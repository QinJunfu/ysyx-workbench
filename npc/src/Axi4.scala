package npc

import chisel3._
import chisel3.util._

/** Complete 32-bit AXI4 master interface required by ysyxSoC/spec/cpu-interface.md.
  *
  * The field order and names match that document exactly, because the flattened port names are part of the CPU contract
  * with the SoC.
  */
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

/** AXI4 slave interface required by ysyxSoC/spec/cpu-interface.md.
  *
  * It is reserved for the ChipLink DMA engine and is tied off inside the CPU until that DMA path is implemented.
  */
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

/** Simulation-only retirement callback.
  *
  * It carries no bus traffic: the CPU's only memory interface is the AXI4 master above.
  *
  * The CPU boundary is fixed by ysyxSoC/spec/cpu-interface.md, so instrumentation such as DiffTest, the instruction
  * trace, the memory trace and trap detection can no longer be top-level ports. This black box taps the internal
  * retirement and trap signals instead: under Verilator it calls the DPI-C function npc_commit, and under Icarus
  * Verilog (which has no DPI-C) it prints the trap result itself.
  */
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
      |`ifdef __ICARUS__
      |  // Icarus Verilog has no DPI-C, so the testbench cannot observe the trap
      |  // through npc_commit. The trap result is printed here instead, matching the
      |  // message and exit status the Verilator host produces.
      |  always @(posedge clock) begin
      |    if (!reset && valid && halt) begin
      |      if (invalid || haltCode != 32'd0) begin
      |        $display("HIT BAD TRAP at pc=0x%08x, inst=0x%08x, code=%0d",
      |                 pc, inst, haltCode);
      |        $fatal(1, "NPC stopped on a bad trap");
      |      end else begin
      |        $display("HIT GOOD TRAP at pc=0x%08x", pc);
      |        $finish;
      |      end
      |    end
      |  end
      |`else
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
      |`endif
      |endmodule
      |""".stripMargin
  )
}
