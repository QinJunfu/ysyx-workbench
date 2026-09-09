module NpcTop (
  input logic clock,
  input logic reset
);
`ifndef SYNTHESIS
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
`endif

  logic [31:0] io_imem_addr;
  logic [31:0] io_imem_data;
  logic [31:0] io_dmem_addr;
  logic [31:0] io_dmem_logical_addr;
  logic [2:0] io_dmem_access_size;
  logic [31:0] io_dmem_rdata;
  logic io_dmem_read_valid;
  logic [31:0] io_dmem_addr2;
  logic [31:0] io_dmem_rdata2;
  logic io_dmem_read_valid2;
  logic io_dmem_write0_valid;
  logic [31:0] io_dmem_write0_addr;
  logic [31:0] io_dmem_write0_data;
  logic [3:0] io_dmem_write0_mask;
  logic io_dmem_write1_valid;
  logic [31:0] io_dmem_write1_addr;
  logic [31:0] io_dmem_write1_data;
  logic [3:0] io_dmem_write1_mask;
  logic io_mem_trace_valid;
  logic io_mem_trace_write;
  logic [31:0] io_mem_trace_addr;
  logic [31:0] io_mem_trace_data;
  logic [3:0] io_mem_trace_mask;
  logic io_retire_valid;
  logic [31:0] io_retire_pc;
  logic [31:0] io_retire_inst;
  logic [31:0] io_retire_dnpc;
  logic [31:0] io_retire_gprs [0:15];
  logic io_halt;
  logic [31:0] io_halt_code;
  logic io_invalid;
  logic io_uart_tx_valid;
  logic [7:0] io_uart_tx_data;

  logic io_master_awvalid;
  logic io_master_awready;
  logic [31:0] io_master_awaddr;
  logic [2:0] io_master_awsize;
  logic io_master_wvalid;
  logic io_master_wready;
  logic [31:0] io_master_wdata;
  logic [3:0] io_master_wstrb;
  logic io_master_bvalid;
  logic io_master_bready;
  logic [1:0] io_master_bresp;
  logic io_master_arvalid;
  logic io_master_arready;
  logic [31:0] io_master_araddr;
  logic [2:0] io_master_arsize;
  logic io_master_rvalid;
  logic io_master_rready;
  logic [31:0] io_master_rdata;
  logic [1:0] io_master_rresp;

  NPC core (
    .clock(clock), .reset(reset),
    .io_imemAddr(io_imem_addr), .io_imemData(io_imem_data),
    .io_dmemAddr(io_dmem_addr), .io_dmemRdata(io_dmem_rdata),
    .io_dmemLogicalAddr(io_dmem_logical_addr), .io_dmemAccessSize(io_dmem_access_size),
    .io_dmemReadValid(io_dmem_read_valid), .io_dmemAddr2(io_dmem_addr2),
    .io_dmemRdata2(io_dmem_rdata2), .io_dmemReadValid2(io_dmem_read_valid2),
    .io_dmemWrite0Valid(io_dmem_write0_valid), .io_dmemWrite0Addr(io_dmem_write0_addr),
    .io_dmemWrite0Data(io_dmem_write0_data), .io_dmemWrite0Mask(io_dmem_write0_mask),
    .io_dmemWrite1Valid(io_dmem_write1_valid), .io_dmemWrite1Addr(io_dmem_write1_addr),
    .io_dmemWrite1Data(io_dmem_write1_data), .io_dmemWrite1Mask(io_dmem_write1_mask),
    .io_memTraceValid(io_mem_trace_valid), .io_memTraceWrite(io_mem_trace_write),
    .io_memTraceAddr(io_mem_trace_addr), .io_memTraceData(io_mem_trace_data),
    .io_memTraceMask(io_mem_trace_mask), .io_retireValid(io_retire_valid),
    .io_retirePc(io_retire_pc), .io_retireInst(io_retire_inst),
    .io_retireDnPc(io_retire_dnpc), .io_retireGprs_0(io_retire_gprs[0]),
    .io_retireGprs_1(io_retire_gprs[1]), .io_retireGprs_2(io_retire_gprs[2]),
    .io_retireGprs_3(io_retire_gprs[3]), .io_retireGprs_4(io_retire_gprs[4]),
    .io_retireGprs_5(io_retire_gprs[5]), .io_retireGprs_6(io_retire_gprs[6]),
    .io_retireGprs_7(io_retire_gprs[7]), .io_retireGprs_8(io_retire_gprs[8]),
    .io_retireGprs_9(io_retire_gprs[9]), .io_retireGprs_10(io_retire_gprs[10]),
    .io_retireGprs_11(io_retire_gprs[11]), .io_retireGprs_12(io_retire_gprs[12]),
    .io_retireGprs_13(io_retire_gprs[13]), .io_retireGprs_14(io_retire_gprs[14]),
    .io_retireGprs_15(io_retire_gprs[15]), .io_halt(io_halt),
    .io_haltCode(io_halt_code), .io_invalid(io_invalid),
    .io_uartTxValid(io_uart_tx_valid), .io_uartTxData(io_uart_tx_data),
    .io_master_awvalid(io_master_awvalid), .io_master_awready(io_master_awready),
    .io_master_awaddr(io_master_awaddr), .io_master_awsize(io_master_awsize),
    .io_master_wvalid(io_master_wvalid),
    .io_master_wready(io_master_wready), .io_master_wdata(io_master_wdata),
    .io_master_wstrb(io_master_wstrb), .io_master_bvalid(io_master_bvalid),
    .io_master_bready(io_master_bready), .io_master_bresp(io_master_bresp),
    .io_master_arvalid(io_master_arvalid), .io_master_arready(io_master_arready),
    .io_master_araddr(io_master_araddr), .io_master_arsize(io_master_arsize),
    .io_master_rvalid(io_master_rvalid),
    .io_master_rready(io_master_rready), .io_master_rdata(io_master_rdata),
    .io_master_rresp(io_master_rresp)
  );

  // The legacy direct-memory inputs are tied off.  All actual transactions
  // use the AXI master above.
  assign io_imem_data = 32'b0;
  assign io_dmem_rdata = 32'b0;
  assign io_dmem_rdata2 = 32'b0;

  logic [31:0] read_data_reg;
  logic read_valid_reg;
  logic [31:0] write_addr_reg;
  logic [31:0] write_data_reg;
  logic [3:0] write_strb_reg;
  logic write_addr_valid;
  logic write_data_valid;
  logic write_resp_valid;
  logic [1:0] write_resp_reg;
  logic [63:0] mtime;

  assign io_master_arready = !read_valid_reg;
  assign io_master_rvalid = read_valid_reg;
  assign io_master_rdata = read_data_reg;
  assign io_master_rresp = 2'b00;
  assign io_master_awready = !write_addr_valid && !write_resp_valid;
  assign io_master_wready = !write_data_valid && !write_resp_valid;
  assign io_master_bvalid = write_resp_valid;
  assign io_master_bresp = write_resp_reg;

  always_ff @(posedge clock) begin
    if (reset) begin
      read_data_reg <= 32'b0;
      read_valid_reg <= 1'b0;
      write_addr_reg <= 32'b0;
      write_data_reg <= 32'b0;
      write_strb_reg <= 4'b0;
      write_addr_valid <= 1'b0;
      write_data_valid <= 1'b0;
      write_resp_valid <= 1'b0;
      write_resp_reg <= 2'b00;
      mtime <= 64'b0;
    end else begin
      mtime <= mtime + 64'd1;

`ifndef SYNTHESIS
      if (io_uart_tx_valid) begin
        $write("%c", io_uart_tx_data);
        $fflush();
      end
`endif

      if (read_valid_reg && io_master_rready) begin
        read_valid_reg <= 1'b0;
      end
      if (io_master_arvalid && io_master_arready) begin
        read_valid_reg <= 1'b1;
        if (io_master_araddr == 32'h20000000) begin
          read_data_reg <= mtime[31:0];
        end else if (io_master_araddr == 32'h20000004) begin
          read_data_reg <= mtime[63:32];
        end else begin
`ifndef SYNTHESIS
          read_data_reg <= pmem_read(io_master_araddr);
`else
          read_data_reg <= 32'b0;
`endif
        end
      end

      if (io_master_awvalid && io_master_awready) begin
        write_addr_reg <= io_master_awaddr;
        write_addr_valid <= 1'b1;
      end
      if (io_master_wvalid && io_master_wready) begin
        write_data_reg <= io_master_wdata;
        write_strb_reg <= io_master_wstrb;
        write_data_valid <= 1'b1;
      end
      if (!write_resp_valid &&
          (write_addr_valid || (io_master_awvalid && io_master_awready)) &&
          (write_data_valid || (io_master_wvalid && io_master_wready))) begin
        write_resp_valid <= 1'b1;
        write_resp_reg <= 2'b00;
        if ((write_addr_valid ? write_addr_reg : io_master_awaddr) == 32'h10000000) begin
`ifndef SYNTHESIS
          if (((write_data_valid ? write_strb_reg : io_master_wstrb) & 4'b0001) != 0) begin
            $write("%c", (write_data_valid ? write_data_reg : io_master_wdata) & 32'hff);
            $fflush();
          end
`endif
        end else if ((write_addr_valid ? write_addr_reg : io_master_awaddr) != 32'h20000000 &&
                     (write_addr_valid ? write_addr_reg : io_master_awaddr) != 32'h20000004) begin
`ifndef SYNTHESIS
          pmem_write(
            (write_addr_valid ? write_addr_reg : io_master_awaddr),
            (write_data_valid ? write_data_reg : io_master_wdata),
            (write_data_valid ? write_strb_reg : io_master_wstrb)
          );
`endif
        end
        write_addr_valid <= 1'b0;
        write_data_valid <= 1'b0;
      end
      if (write_resp_valid && io_master_bready) begin
        write_resp_valid <= 1'b0;
      end
    end
  end

  always_ff @(posedge clock) begin
    if (!reset && io_retire_valid) begin
`ifndef SYNTHESIS
      npc_commit(
        io_retire_pc, io_retire_inst, io_retire_dnpc,
        io_retire_gprs[0], io_retire_gprs[1], io_retire_gprs[2], io_retire_gprs[3],
        io_retire_gprs[4], io_retire_gprs[5], io_retire_gprs[6], io_retire_gprs[7],
        io_retire_gprs[8], io_retire_gprs[9], io_retire_gprs[10], io_retire_gprs[11],
        io_retire_gprs[12], io_retire_gprs[13], io_retire_gprs[14], io_retire_gprs[15],
        int'(io_mem_trace_valid), int'(io_mem_trace_write), io_mem_trace_addr,
        io_mem_trace_data, int'(io_mem_trace_mask), int'(io_halt), io_halt_code,
        int'(io_invalid)
      );
`endif
    end
  end

endmodule
