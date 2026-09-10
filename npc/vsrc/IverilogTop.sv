// Self-driving testbench used only by the Icarus Verilog simulator.
//
// Icarus Verilog does not support DPI-C, so the C host that normally drives
// NpcTop from the Verilator build cannot be reused.  This bench generates the
// clock and reset itself, provides optional VCD dumping and a cycle limit, and
// lets NpcTop terminate the simulation when the program traps.  The physical
// memory is reached through the VPI system tasks registered by the npc_vpi
// module, so no C code runs in this process other than that module.
`ifdef __ICARUS__
`timescale 1ns/1ps

module iverilog_top;
  reg clock;
  reg reset;

  integer max_cycles;
  integer cycles;
  reg [8*1024-1:0] wave_file;

  NpcTop top (
    .clock(clock),
    .reset(reset)
  );

  initial begin
    clock = 1'b0;
    reset = 1'b1;
    cycles = 0;
    if (!$value$plusargs("max-cycles=%d", max_cycles)) begin
      max_cycles = 100000000;
    end
    // Hold reset for a few cycles so every register with a reset reaches its
    // defined value before the program starts.  A register without a reset
    // keeps its X value here, which is exactly what this build is meant to
    // expose.
    #10 reset = 1'b0;
  end

  always #1 clock = ~clock;

  initial begin
    if ($value$plusargs("wave=%s", wave_file)) begin
      $dumpfile(wave_file);
      $dumpvars(0, iverilog_top);
    end
  end

  always @(posedge clock) begin
    if (!reset) begin
      cycles = cycles + 1;
      if (max_cycles > 0 && cycles >= max_cycles) begin
        $display("NPC TIMEOUT after %0d cycles", cycles);
        $fatal(1, "NPC simulation exceeded the cycle limit");
      end
    end
  end
endmodule
`endif
