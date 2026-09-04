#include <exception>
#include <new>
#include <stddef.h>

#include <VNpcTop.h>
#include <verilated.h>
#include <verilated_vcd_c.h>

extern "C" {
#include "common.h"
#include "verilator_host.h"
}

struct NpcVerilatorHost {
  VerilatedContext *context;
  VNpcTop *top;
  VerilatedVcdC *trace_vcd;
};

extern "C" NpcVerilatorHost *npc_verilator_host_create(int argc, char **argv,
                                                          char *error,
                                                          size_t error_size) {
  NpcVerilatorHost *host = NULL;

  try {
    host = new (std::nothrow) NpcVerilatorHost;
    if (host == NULL) {
      npc_set_error(error, error_size, "cannot allocate Verilator host");
      return NULL;
    }
    host->context = NULL;
    host->top = NULL;
    host->trace_vcd = NULL;
    host->context = new (std::nothrow) VerilatedContext;
    if (host->context == NULL) {
      npc_set_error(error, error_size, "cannot allocate Verilator context");
      npc_verilator_host_destroy(host);
      return NULL;
    }
    host->context->commandArgs(argc, argv);
    host->context->traceEverOn(true);
    host->top = new (std::nothrow) VNpcTop(host->context);
    if (host->top == NULL) {
      npc_set_error(error, error_size, "cannot allocate Verilator top module");
      npc_verilator_host_destroy(host);
      return NULL;
    }
    host->trace_vcd = new (std::nothrow) VerilatedVcdC;
    if (host->trace_vcd == NULL) {
      npc_set_error(error, error_size, "cannot allocate VCD trace writer");
      npc_verilator_host_destroy(host);
      return NULL;
    }
    host->top->trace(host->trace_vcd, 99);
    host->trace_vcd->open("build/waveform.vcd");
    return host;
  } catch (const std::exception &exception) {
    npc_set_error(error, error_size, "%s", exception.what());
    npc_verilator_host_destroy(host);
    return NULL;
  } catch (...) {
    npc_set_error(error, error_size, "unknown exception while initializing Verilator host");
    npc_verilator_host_destroy(host);
    return NULL;
  }
}

extern "C" void npc_verilator_host_destroy(NpcVerilatorHost *host) {
  if (host == NULL) {
    return;
  }
  if (host->top != NULL) {
    host->top->final();
  }
  if (host->trace_vcd != NULL) {
    host->trace_vcd->close();
  }
  delete host->trace_vcd;
  delete host->top;
  delete host->context;
  delete host;
}

extern "C" void npc_verilator_host_set_clock(NpcVerilatorHost *host, int value) {
  host->top->clock = value != 0;
}

extern "C" void npc_verilator_host_set_reset(NpcVerilatorHost *host, int value) {
  host->top->reset = value != 0;
}

extern "C" void npc_verilator_host_evaluate(NpcVerilatorHost *host) {
  host->top->eval();
  host->trace_vcd->dump(host->context->time());
  host->context->timeInc(1);
}

extern "C" int npc_verilator_host_got_finish(const NpcVerilatorHost *host) {
  return host->context->gotFinish() ? 1 : 0;
}
