module NpcTop (
  input logic clock,
  input logic reset
);
  import "DPI-C" function int pmem_read(input int raddr);
  import "DPI-C" function void pmem_write(
    input int waddr, input int wdata, input byte wmask
  );
  import "DPI-C" function void npc_commit(
    input int pc, input int inst, input int dnpc,
    input int gpr0, input int gpr1, input int gpr2, input int gpr3,
    input int gpr4, input int gpr5, input int gpr6, input int gpr7,
    input int gpr8, input int gpr9, input int gpr10, input int gpr11,
    input int gpr12, input int gpr13, input int gpr14, input int gpr15,
    input int mem_valid, input int mem_write, input int mem_addr,
    input int mem_data, input int mem_mask, input int halt,
    input int halt_code, input int invalid
  );

  logic [31:0] imem_addr;
  logic [31:0] imem_data;
  logic [31:0] dmem_addr;
  logic [31:0] dmem_rdata;
  logic dmem_read_valid;
  logic [31:0] dmem_addr2;
  logic [31:0] dmem_rdata2;
  logic dmem_read_valid2;

  logic dmem_write0_valid;
  logic [31:0] dmem_write0_addr;
  logic [31:0] dmem_write0_data;
  logic [3:0] dmem_write0_mask;
  logic dmem_write1_valid;
  logic [31:0] dmem_write1_addr;
  logic [31:0] dmem_write1_data;
  logic [3:0] dmem_write1_mask;

  logic mem_trace_valid;
  logic mem_trace_write;
  logic [31:0] mem_trace_addr;
  logic [31:0] mem_trace_data;
  logic [3:0] mem_trace_mask;
  logic retire_valid;
  logic [31:0] retire_pc;
  logic [31:0] retire_inst;
  logic [31:0] retire_dnpc;
  logic [31:0] retire_gpr0;
  logic [31:0] retire_gpr1;
  logic [31:0] retire_gpr2;
  logic [31:0] retire_gpr3;
  logic [31:0] retire_gpr4;
  logic [31:0] retire_gpr5;
  logic [31:0] retire_gpr6;
  logic [31:0] retire_gpr7;
  logic [31:0] retire_gpr8;
  logic [31:0] retire_gpr9;
  logic [31:0] retire_gpr10;
  logic [31:0] retire_gpr11;
  logic [31:0] retire_gpr12;
  logic [31:0] retire_gpr13;
  logic [31:0] retire_gpr14;
  logic [31:0] retire_gpr15;
  logic halt;
  logic [31:0] halt_code;
  logic invalid;

  NPC core (
    .clock(clock),
    .reset(reset),
    .io_imemAddr(imem_addr),
    .io_imemData(imem_data),
    .io_dmemAddr(dmem_addr),
    .io_dmemRdata(dmem_rdata),
    .io_dmemReadValid(dmem_read_valid),
    .io_dmemAddr2(dmem_addr2),
    .io_dmemRdata2(dmem_rdata2),
    .io_dmemReadValid2(dmem_read_valid2),
    .io_dmemWrite0Valid(dmem_write0_valid),
    .io_dmemWrite0Addr(dmem_write0_addr),
    .io_dmemWrite0Data(dmem_write0_data),
    .io_dmemWrite0Mask(dmem_write0_mask),
    .io_dmemWrite1Valid(dmem_write1_valid),
    .io_dmemWrite1Addr(dmem_write1_addr),
    .io_dmemWrite1Data(dmem_write1_data),
    .io_dmemWrite1Mask(dmem_write1_mask),
    .io_memTraceValid(mem_trace_valid),
    .io_memTraceWrite(mem_trace_write),
    .io_memTraceAddr(mem_trace_addr),
    .io_memTraceData(mem_trace_data),
    .io_memTraceMask(mem_trace_mask),
    .io_retireValid(retire_valid),
    .io_retirePc(retire_pc),
    .io_retireInst(retire_inst),
    .io_retireDnPc(retire_dnpc),
    .io_retireGpr0(retire_gpr0),
    .io_retireGpr1(retire_gpr1),
    .io_retireGpr2(retire_gpr2),
    .io_retireGpr3(retire_gpr3),
    .io_retireGpr4(retire_gpr4),
    .io_retireGpr5(retire_gpr5),
    .io_retireGpr6(retire_gpr6),
    .io_retireGpr7(retire_gpr7),
    .io_retireGpr8(retire_gpr8),
    .io_retireGpr9(retire_gpr9),
    .io_retireGpr10(retire_gpr10),
    .io_retireGpr11(retire_gpr11),
    .io_retireGpr12(retire_gpr12),
    .io_retireGpr13(retire_gpr13),
    .io_retireGpr14(retire_gpr14),
    .io_retireGpr15(retire_gpr15),
    .io_halt(halt),
    .io_haltCode(halt_code),
    .io_invalid(invalid)
  );

  always_comb begin
    imem_data = pmem_read(imem_addr);
  end

  always_comb begin
    dmem_rdata = dmem_read_valid ? pmem_read(dmem_addr) : 32'b0;
    dmem_rdata2 = dmem_read_valid2 ? pmem_read(dmem_addr2) : 32'b0;
  end

  always @(posedge clock) begin
    if (!reset) begin
      if (dmem_write0_valid) begin
        pmem_write(dmem_write0_addr, dmem_write0_data, {4'b0, dmem_write0_mask});
      end
      if (dmem_write1_valid) begin
        pmem_write(dmem_write1_addr, dmem_write1_data, {4'b0, dmem_write1_mask});
      end
      if (retire_valid) begin
        npc_commit(
          retire_pc, retire_inst, retire_dnpc,
          retire_gpr0, retire_gpr1, retire_gpr2, retire_gpr3,
          retire_gpr4, retire_gpr5, retire_gpr6, retire_gpr7,
          retire_gpr8, retire_gpr9, retire_gpr10, retire_gpr11,
          retire_gpr12, retire_gpr13, retire_gpr14, retire_gpr15,
          int'(mem_trace_valid), int'(mem_trace_write), mem_trace_addr, mem_trace_data,
          int'(mem_trace_mask), int'(halt), halt_code, int'(invalid)
        );
      end
    end
  end
endmodule
