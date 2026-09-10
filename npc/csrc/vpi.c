// VPI module for the Icarus Verilog build of NPC.
//
// Icarus Verilog does not support DPI-C, so the physical memory cannot be
// reached through the C functions used by the Verilator host.  Instead this
// module registers two VPI system tasks:
//
//   $pmem_read(address)            -> 32-bit word read from physical memory
//   $pmem_write(address, data, mask) -> masked word write to physical memory
//
// The image named by the +image=PATH plusarg is loaded into the memory array
// right before the simulation starts, reusing the same NpcMemory implementation
// as the Verilator build (csrc/memory.c).
//
// This file is built into npc_vpi.vpi and is intentionally excluded from the
// Verilator executable's object list by the Makefile.

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <vpi_user.h>

#include "common.h"
#include "memory.h"

static NpcMemory *npc_vpi_memory = NULL;
static int npc_vpi_range_reported = 0;

static PLI_INT32 npc_vpi_sizetf(PLI_BYTE8 *user_data);

static const char *npc_vpi_find_image(int *argc, char ***argv) {
  int index;

  for (index = 1; index < *argc; ++index) {
    const char *argument = (*argv)[index];
    if (strncmp(argument, "+image=", 7) == 0 && argument[7] != '\0') {
      return argument + 7;
    }
  }
  return NULL;
}

// Runs just before the first simulation time step: load the program image so
// that the very first instruction fetch already sees valid memory contents.
static PLI_INT32 npc_vpi_start_of_simulation(s_cb_data *callback_data) {
  s_vpi_vlog_info info;
  const char *image;
  char error[NPC_ERROR_SIZE];
  size_t image_size;

  (void)callback_data;

  memset(&info, 0, sizeof(info));
  if (!vpi_get_vlog_info(&info)) {
    vpi_printf("NPC VPI: cannot read the simulation command line\n");
    vpi_control(vpiFinish, 1);
    return 0;
  }

  image = npc_vpi_find_image(&info.argc, &info.argv);
  if (image == NULL) {
    vpi_printf("NPC VPI: missing +image=PATH plusarg\n");
    vpi_control(vpiFinish, 1);
    return 0;
  }

  npc_vpi_memory = npc_memory_create(error, sizeof(error));
  if (npc_vpi_memory == NULL) {
    vpi_printf("NPC VPI: %s\n", error);
    vpi_control(vpiFinish, 1);
    return 0;
  }
  if (!npc_memory_load_image(npc_vpi_memory, image, error, sizeof(error))) {
    vpi_printf("NPC VPI: %s\n", error);
    vpi_control(vpiFinish, 1);
    return 0;
  }

  image_size = npc_memory_image_size(npc_vpi_memory);
  vpi_printf("NPC VPI: loaded %zu bytes from %s\n", image_size, image);
  return 0;
}

// $pmem_read(address) returns the physical memory word at address.  Icarus
// passes the address as a 32-bit value; X bits are read as zero, which is the
// only sensible two-value view of a four-value address.
static PLI_INT32 npc_vpi_pmem_read(PLI_BYTE8 *user_data) {
  vpiHandle call;
  vpiHandle iterator;
  vpiHandle argument;
  s_vpi_value value;
  uint32_t address;
  uint32_t data;
  char error[NPC_ERROR_SIZE];

  (void)user_data;

  call = vpi_handle(vpiSysTfCall, NULL);
  iterator = vpi_iterate(vpiArgument, call);
  argument = vpi_scan(iterator);
  if (argument == NULL) {
    vpi_printf("NPC VPI: $pmem_read called without an address\n");
    data = 0;
  } else {
    value.format = vpiIntVal;
    vpi_get_value(argument, &value);
    address = (uint32_t)value.value.integer;
    data = 0;
    if (npc_vpi_memory == NULL ||
        !npc_memory_read_word(npc_vpi_memory, address, &data, error, sizeof(error))) {
      if (!npc_vpi_range_reported) {
        vpi_printf("NPC VPI: $pmem_read ignored out-of-range address 0x%08x\n", address);
        npc_vpi_range_reported = 1;
      }
      data = 0;
    }
  }
  if (iterator != NULL) {
    vpi_free_object(iterator);
  }

  // Writing to the system function call handle is how a VPI function returns
  // its value to the expression that called it.
  value.format = vpiIntVal;
  value.value.integer = (PLI_INT32)data;
  vpi_put_value(call, &value, NULL, vpiNoDelay);
  return 0;
}

// $pmem_write(address, data, mask) performs a byte-masked word write.
static PLI_INT32 npc_vpi_pmem_write(PLI_BYTE8 *user_data) {
  vpiHandle call;
  vpiHandle iterator;
  vpiHandle argument;
  s_vpi_value value;
  uint32_t address;
  uint32_t data;
  uint32_t mask;
  char error[NPC_ERROR_SIZE];

  (void)user_data;

  call = vpi_handle(vpiSysTfCall, NULL);
  iterator = vpi_iterate(vpiArgument, call);

  argument = vpi_scan(iterator);
  value.format = vpiIntVal;
  if (argument == NULL) {
    vpi_printf("NPC VPI: $pmem_write called without an address\n");
    return 0;
  }
  vpi_get_value(argument, &value);
  address = (uint32_t)value.value.integer;

  argument = vpi_scan(iterator);
  if (argument == NULL) {
    vpi_printf("NPC VPI: $pmem_write called without data\n");
    return 0;
  }
  vpi_get_value(argument, &value);
  data = (uint32_t)value.value.integer;

  argument = vpi_scan(iterator);
  if (argument == NULL) {
    vpi_printf("NPC VPI: $pmem_write called without a write mask\n");
    return 0;
  }
  vpi_get_value(argument, &value);
  mask = (uint32_t)value.value.integer;

  if (npc_vpi_memory == NULL ||
      !npc_memory_write_word_masked(npc_vpi_memory, address, data, (uint8_t)mask, error,
                                    sizeof(error))) {
    if (!npc_vpi_range_reported) {
      vpi_printf("NPC VPI: $pmem_write ignored out-of-range address 0x%08x\n", address);
      npc_vpi_range_reported = 1;
    }
  }
  return 0;
}

static PLI_INT32 npc_vpi_pmem_read_compiletf(PLI_BYTE8 *user_data) {
  vpiHandle call;
  vpiHandle iterator;

  (void)user_data;

  call = vpi_handle(vpiSysTfCall, NULL);
  iterator = vpi_iterate(vpiArgument, call);
  if (iterator == NULL || vpi_scan(iterator) == NULL) {
    vpi_printf("NPC VPI: $pmem_read requires one address argument\n");
    vpi_control(vpiFinish, 1);
    return 0;
  }
  vpi_free_object(iterator);
  return 0;
}

static PLI_INT32 npc_vpi_pmem_write_compiletf(PLI_BYTE8 *user_data) {
  vpiHandle call;
  vpiHandle iterator;
  int count = 0;

  (void)user_data;

  call = vpi_handle(vpiSysTfCall, NULL);
  iterator = vpi_iterate(vpiArgument, call);
  if (iterator != NULL) {
    while (vpi_scan(iterator) != NULL) {
      count += 1;
    }
  }
  if (count != 3) {
    vpi_printf("NPC VPI: $pmem_write requires address, data and write mask\n");
    vpi_control(vpiFinish, 1);
    return 0;
  }
  return 0;
}

static void npc_vpi_register(void) {
  s_vpi_systf_data tf;
  s_cb_data callback;
  vpiHandle callback_handle;

  memset(&tf, 0, sizeof(tf));
  // vpiSysFuncInt collides with vpiSysTask in vpi_user.h, so a system function
  // is registered as vpiSysFunc plus an explicit return type and width.
  tf.type = vpiSysFunc;
  tf.sysfunctype = vpiSysFuncSized;
  tf.tfname = "$pmem_read";
  tf.calltf = npc_vpi_pmem_read;
  tf.compiletf = npc_vpi_pmem_read_compiletf;
  tf.sizetf = npc_vpi_sizetf;
  tf.user_data = NULL;
  vpi_register_systf(&tf);

  memset(&tf, 0, sizeof(tf));
  tf.type = vpiSysTask;
  tf.tfname = "$pmem_write";
  tf.calltf = npc_vpi_pmem_write;
  tf.compiletf = npc_vpi_pmem_write_compiletf;
  tf.user_data = NULL;
  vpi_register_systf(&tf);

  memset(&callback, 0, sizeof(callback));
  callback.reason = cbStartOfSimulation;
  callback.cb_rtn = npc_vpi_start_of_simulation;
  callback.obj = NULL;
  callback.time = NULL;
  callback.value = NULL;
  callback.user_data = NULL;
  callback_handle = vpi_register_cb(&callback);
  if (callback_handle != NULL) {
    vpi_free_object(callback_handle);
  }
}

static PLI_INT32 npc_vpi_sizetf(PLI_BYTE8 *user_data) {
  (void)user_data;
  return 32;
}

void (*vlog_startup_routines[])(void) = {
  npc_vpi_register,
  0
};
