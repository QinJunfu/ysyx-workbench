// Simulation top for the Direct NPC platform.
//
// It instantiates the strict NPC, whose boundary is exactly
// ysyxSoC/spec/cpu-interface.md (io_interrupt plus a full AXI4 master and a
// full AXI4 slave), and implements a single-outstanding AXI4 memory slave so
// the CPU can fetch instructions and access data.
//
// The physical memory is provided through the DPI-C functions pmem_read() and
// pmem_write() under Verilator and through the VPI tasks $pmem_read/$pmem_write
// under Icarus Verilog; those are the only channels between the design and the
// C/C++ host. Retirement, DiffTest, the instruction/memory trace and trap
// reporting come from inside NPC through its simulation-only NpcCommitDpi tap,
// so this testbench never needs a non-standard CPU port.
//
// The gate-level netlist is synthesized from the same strict NPC, so it has no
// NpcCommitDpi tap: its simulation runs under a cycle limit and is checked with
// a waveform (X propagation), not with HIT GOOD/BAD TRAP.
module NpcTop (
  input logic clock,
  input logic reset
);
`ifdef __ICARUS__
  // Icarus Verilog does not support DPI-C.  Its simulation environment
  // supplies the physical memory through the VPI system function
  // $pmem_read and the VPI system task $pmem_write.
`else
`ifndef SYNTHESIS
  import "DPI-C" function int pmem_read(input int raddr);
  import "DPI-C" function void pmem_write(
    input int waddr, input int wdata, input byte wmask
  );
`endif
`endif

`ifdef __ICARUS__
  // Icarus Verilog warns that simulation system tasks and the VPI
  // $pmem_write task cannot be synthesized when they appear in an always_ff
  // process. NpcTop is simulation-only glue, so the Icarus build uses a plain
  // always block; Verilator keeps always_ff.
  `define NPC_SEQ always @(posedge clock)
`else
  `define NPC_SEQ always_ff @(posedge clock)
`endif

  // The interrupt line is not wired to a device in the Direct platform.
  logic io_interrupt;

  logic io_master_awvalid;
  logic io_master_awready;
  logic [31:0] io_master_awaddr;
  logic [3:0] io_master_awid;
  logic [7:0] io_master_awlen;
  logic [2:0] io_master_awsize;
  logic [1:0] io_master_awburst;
  logic io_master_wvalid;
  logic io_master_wready;
  logic [31:0] io_master_wdata;
  logic [3:0] io_master_wstrb;
  logic io_master_wlast;
  logic io_master_bvalid;
  logic io_master_bready;
  logic [1:0] io_master_bresp;
  logic [3:0] io_master_bid;
  logic io_master_arvalid;
  logic io_master_arready;
  logic [31:0] io_master_araddr;
  logic [3:0] io_master_arid;
  logic [7:0] io_master_arlen;
  logic [2:0] io_master_arsize;
  logic [1:0] io_master_arburst;
  logic io_master_rvalid;
  logic io_master_rready;
  logic [31:0] io_master_rdata;
  logic [1:0] io_master_rresp;
  logic io_master_rlast;
  logic [3:0] io_master_rid;

  logic io_slave_awready;
  logic io_slave_awvalid;
  logic [31:0] io_slave_awaddr;
  logic [3:0] io_slave_awid;
  logic [7:0] io_slave_awlen;
  logic [2:0] io_slave_awsize;
  logic [1:0] io_slave_awburst;
  logic io_slave_wready;
  logic io_slave_wvalid;
  logic [31:0] io_slave_wdata;
  logic [3:0] io_slave_wstrb;
  logic io_slave_wlast;
  logic io_slave_bvalid;
  logic io_slave_bready;
  logic [1:0] io_slave_bresp;
  logic [3:0] io_slave_bid;
  logic io_slave_arready;
  logic io_slave_arvalid;
  logic [31:0] io_slave_araddr;
  logic [3:0] io_slave_arid;
  logic [7:0] io_slave_arlen;
  logic [2:0] io_slave_arsize;
  logic [1:0] io_slave_arburst;
  logic io_slave_rvalid;
  logic io_slave_rready;
  logic [31:0] io_slave_rdata;
  logic [1:0] io_slave_rresp;
  logic io_slave_rlast;
  logic [3:0] io_slave_rid;

  // The CPU top module has the same name on every platform.
  ysyx_24100022 core (
    .clock(clock), .reset(reset),
    .io_interrupt(io_interrupt),
    .io_master_awvalid(io_master_awvalid), .io_master_awready(io_master_awready),
    .io_master_awaddr(io_master_awaddr), .io_master_awid(io_master_awid),
    .io_master_awlen(io_master_awlen), .io_master_awsize(io_master_awsize),
    .io_master_awburst(io_master_awburst),
    .io_master_wvalid(io_master_wvalid), .io_master_wready(io_master_wready),
    .io_master_wdata(io_master_wdata), .io_master_wstrb(io_master_wstrb),
    .io_master_wlast(io_master_wlast),
    .io_master_bvalid(io_master_bvalid), .io_master_bready(io_master_bready),
    .io_master_bresp(io_master_bresp), .io_master_bid(io_master_bid),
    .io_master_arvalid(io_master_arvalid), .io_master_arready(io_master_arready),
    .io_master_araddr(io_master_araddr), .io_master_arid(io_master_arid),
    .io_master_arlen(io_master_arlen), .io_master_arsize(io_master_arsize),
    .io_master_arburst(io_master_arburst),
    .io_master_rvalid(io_master_rvalid), .io_master_rready(io_master_rready),
    .io_master_rdata(io_master_rdata), .io_master_rresp(io_master_rresp),
    .io_master_rlast(io_master_rlast), .io_master_rid(io_master_rid),
    .io_slave_awready(io_slave_awready), .io_slave_awvalid(io_slave_awvalid),
    .io_slave_awaddr(io_slave_awaddr), .io_slave_awid(io_slave_awid),
    .io_slave_awlen(io_slave_awlen), .io_slave_awsize(io_slave_awsize),
    .io_slave_awburst(io_slave_awburst),
    .io_slave_wready(io_slave_wready), .io_slave_wvalid(io_slave_wvalid),
    .io_slave_wdata(io_slave_wdata), .io_slave_wstrb(io_slave_wstrb),
    .io_slave_wlast(io_slave_wlast),
    .io_slave_bvalid(io_slave_bvalid), .io_slave_bready(io_slave_bready),
    .io_slave_bresp(io_slave_bresp), .io_slave_bid(io_slave_bid),
    .io_slave_arready(io_slave_arready), .io_slave_arvalid(io_slave_arvalid),
    .io_slave_araddr(io_slave_araddr), .io_slave_arid(io_slave_arid),
    .io_slave_arlen(io_slave_arlen), .io_slave_arsize(io_slave_arsize),
    .io_slave_arburst(io_slave_arburst),
    .io_slave_rvalid(io_slave_rvalid), .io_slave_rready(io_slave_rready),
    .io_slave_rdata(io_slave_rdata), .io_slave_rresp(io_slave_rresp),
    .io_slave_rlast(io_slave_rlast), .io_slave_rid(io_slave_rid)
  );

  assign io_interrupt = 1'b0;

  // The AXI4 slave is reserved for the ChipLink DMA engine and is unused here,
  // so its inputs are tied off.
  assign io_slave_awvalid = 1'b0;
  assign io_slave_awaddr  = 32'b0;
  assign io_slave_awid    = 4'b0;
  assign io_slave_awlen   = 8'b0;
  assign io_slave_awsize  = 3'b0;
  assign io_slave_awburst = 2'b0;
  assign io_slave_wvalid  = 1'b0;
  assign io_slave_wdata   = 32'b0;
  assign io_slave_wstrb   = 4'b0;
  assign io_slave_wlast   = 1'b0;
  assign io_slave_bready  = 1'b0;
  assign io_slave_arvalid = 1'b0;
  assign io_slave_araddr  = 32'b0;
  assign io_slave_arid    = 4'b0;
  assign io_slave_arlen   = 8'b0;
  assign io_slave_arsize  = 3'b0;
  assign io_slave_arburst = 2'b0;
  assign io_slave_rready  = 1'b0;

  // Single-outstanding AXI4 memory slave.  Every transaction is one beat with
  // ID 0, so the burst attributes coming from the CPU are unused.
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
  assign io_master_rlast = 1'b1;
  assign io_master_rid   = 4'b0;
  assign io_master_awready = !write_addr_valid && !write_resp_valid;
  assign io_master_wready = !write_data_valid && !write_resp_valid;
  assign io_master_bvalid = write_resp_valid;
  assign io_master_bresp = write_resp_reg;
  assign io_master_bid   = 4'b0;

  `NPC_SEQ begin
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
`ifdef __ICARUS__
          read_data_reg <= $pmem_read(io_master_araddr);
`else
`ifndef SYNTHESIS
          read_data_reg <= pmem_read(io_master_araddr);
`else
          read_data_reg <= 32'b0;
`endif
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
`ifdef __ICARUS__
          $pmem_write(
            (write_addr_valid ? write_addr_reg : io_master_awaddr),
            (write_data_valid ? write_data_reg : io_master_wdata),
            (write_data_valid ? write_strb_reg : io_master_wstrb)
          );
`else
`ifndef SYNTHESIS
          pmem_write(
            (write_addr_valid ? write_addr_reg : io_master_awaddr),
            (write_data_valid ? write_data_reg : io_master_wdata),
            (write_data_valid ? write_strb_reg : io_master_wstrb)
          );
`endif
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

endmodule
