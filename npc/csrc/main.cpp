#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <verilated.h>
#include <verilated_vcd_c.h>
#include <Vtop.h>

int main(int argc, char **argv) {
  VerilatedContext *contextp = new VerilatedContext;
  contextp->commandArgs(argc, argv);
  contextp->traceEverOn(true);
  Vtop *top = new Vtop{contextp};

  VerilatedVcdC *trace = new VerilatedVcdC;
  top->trace(trace, 99);
  trace->open("waveform.vcd");

  constexpr vluint64_t kMaxCycles = 100;
  while (!contextp->gotFinish() && contextp->time() < kMaxCycles) {
    int a = rand() & 1;
    int b = rand() & 1;
    top->a = a;
    top->b = b;
    top->eval();
    trace->dump(contextp->time());
    printf("a = %d, b = %d, f = %d\n", a, b, top->f);
    assert(top->f == (a ^ b));
    contextp->timeInc(1);
  }

  top->final();
  trace->close();
  delete trace;
  delete top;
  delete contextp;
}
