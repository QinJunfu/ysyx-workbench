#include <nvboard.h>
#include <verilated.h>
#include <verilated_vcd_c.h>
#include <Vtop.h>

void nvboard_bind_all_pins(TOP_NAME *top);

int main(int argc, char **argv) {
  VerilatedContext *contextp = new VerilatedContext;
  contextp->commandArgs(argc, argv);
  contextp->traceEverOn(true);
  TOP_NAME *top = new TOP_NAME{contextp};

  nvboard_bind_all_pins(top);
  nvboard_init();

  VerilatedVcdC *trace = new VerilatedVcdC;
  top->trace(trace, 99);
  trace->open("waveform.vcd");

  while (!contextp->gotFinish()) {
    nvboard_update();
    top->eval();
    trace->dump(contextp->time());
    if ((contextp->time() & 0x3ff) == 0) trace->flush();
    contextp->timeInc(1);
  }

  top->final();
  trace->close();
  nvboard_quit();
  delete trace;
  delete top;
  delete contextp;
}
