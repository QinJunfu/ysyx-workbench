#include <exception>
#include <new>
#include <stddef.h>

#include <generated/autoconf.h>

#ifdef CONFIG_NPC_PLATFORM_YSYXSOC
#include <VysyxSoCFull.h>
#include <nvboard.h>
using NpcVerilatedTop = VysyxSoCFull;

void nvboard_bind_all_pins(VysyxSoCFull *top);
#else
#include <VNpcTop.h>
using NpcVerilatedTop = VNpcTop;
#endif
#include <verilated.h>
#include <verilated_vcd_c.h>

extern "C" {
#include "common.h"
#include "verilator_host.h"
}

struct NpcVerilatorHost {
  VerilatedContext *context;
  NpcVerilatedTop *top;
  VerilatedVcdC *trace_vcd;
  bool nvboard_active;
};

extern "C" NpcVerilatorHost *npc_verilator_host_create(int argc, char **argv,
                                                          const NpcOptions *options,
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
    host->nvboard_active = false;
    host->context = new (std::nothrow) VerilatedContext;
    if (host->context == NULL) {
      npc_set_error(error, error_size, "cannot allocate Verilator context");
      npc_verilator_host_destroy(host);
      return NULL;
    }
    host->context->commandArgs(argc, argv);
    host->context->traceEverOn(options->waveform[0] != '\0');
    host->top = new (std::nothrow) NpcVerilatedTop(host->context);
    if (host->top == NULL) {
      npc_set_error(error, error_size, "cannot allocate Verilator top module");
      npc_verilator_host_destroy(host);
      return NULL;
    }
    if (options->waveform[0] != '\0') {
      host->trace_vcd = new (std::nothrow) VerilatedVcdC;
      if (host->trace_vcd == NULL) {
        npc_set_error(error, error_size, "cannot allocate VCD trace writer");
        npc_verilator_host_destroy(host);
        return NULL;
      }
      host->top->trace(host->trace_vcd, 99);
      host->trace_vcd->open(options->waveform);
    }
#ifdef CONFIG_NPC_PLATFORM_YSYXSOC
    host->top->externalPins_gpio_in = 0;
    host->top->externalPins_ps2_clk = 1;
    host->top->externalPins_ps2_data = 1;
    host->top->externalPins_uart_rx = 1;
    if (options->nvboard) {
      nvboard_bind_all_pins(host->top);
      nvboard_init();
      host->nvboard_active = true;
    }
#else
    if (options->nvboard) {
      npc_set_error(error, error_size, "--nvboard requires the ysyxSoC platform");
      npc_verilator_host_destroy(host);
      return NULL;
    }
#endif
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
#ifdef CONFIG_NPC_PLATFORM_YSYXSOC
  if (host->nvboard_active) {
    nvboard_quit();
  }
#endif
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
  if (host->trace_vcd != NULL) {
    host->trace_vcd->dump(host->context->time());
  }
  host->context->timeInc(1);
}

extern "C" void npc_verilator_host_update_devices(NpcVerilatorHost *host) {
#ifdef CONFIG_NPC_PLATFORM_YSYXSOC
  if (host->nvboard_active) {
    nvboard_update();
  }
#else
  (void)host;
#endif
}

extern "C" int npc_verilator_host_got_finish(const NpcVerilatorHost *host) {
  return host->context->gotFinish() ? 1 : 0;
}
