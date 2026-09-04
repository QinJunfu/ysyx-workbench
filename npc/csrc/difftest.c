#include "difftest.h"

#include <dlfcn.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  uint32_t gpr[32];
  uint32_t pc;
} NpcNemuState;

typedef void (*NpcDifftestMemcpy)(uint32_t address, void *buffer, size_t size, bool direction);
typedef void (*NpcDifftestRegcpy)(void *state, bool direction);
typedef void (*NpcDifftestExec)(uint64_t count);
typedef void (*NpcDifftestInit)(int port);

struct NpcDifftest {
  void *handle;
  NpcDifftestMemcpy memcpy_fn;
  NpcDifftestRegcpy regcpy_fn;
  NpcDifftestExec exec_fn;
  NpcDifftestInit init_fn;
  int enabled;
};

static int npc_difftest_load_symbol(NpcDifftest *difftest, const char *name,
                                    void *destination, size_t destination_size,
                                    char *error, size_t error_size) {
  void *symbol;
  const char *message;

  dlerror();
  symbol = dlsym(difftest->handle, name);
  message = dlerror();
  if (message != NULL) {
    npc_set_error(error, error_size, "DiffTest reference is missing '%s': %s", name, message);
    return 0;
  }
  if (destination_size != sizeof(symbol)) {
    npc_set_error(error, error_size, "DiffTest symbol '%s' has an incompatible pointer size", name);
    return 0;
  }
  memcpy(destination, &symbol, destination_size);
  return 1;
}

NpcDifftest *npc_difftest_create(char *error, size_t error_size) {
  NpcDifftest *difftest;

  difftest = (NpcDifftest *)calloc(1, sizeof(*difftest));
  if (difftest == NULL) {
    npc_set_error(error, error_size, "cannot allocate DiffTest state");
  }
  return difftest;
}

void npc_difftest_destroy(NpcDifftest *difftest) {
  if (difftest == NULL) {
    return;
  }
  if (difftest->handle != NULL) {
    dlclose(difftest->handle);
  }
  free(difftest);
}

int npc_difftest_initialize(NpcDifftest *difftest, const char *path,
                            const NpcMemory *memory, uint32_t reset_pc,
                            char *error, size_t error_size) {
  NpcNemuState initial;
  const char *message;

  difftest->handle = dlopen(path, RTLD_LAZY | RTLD_LOCAL);
  if (difftest->handle == NULL) {
    message = dlerror();
    npc_set_error(error, error_size, "cannot load DiffTest reference '%s': %s", path,
                  message == NULL ? "unknown error" : message);
    return 0;
  }
  if (!npc_difftest_load_symbol(difftest, "difftest_memcpy", &difftest->memcpy_fn,
                                sizeof(difftest->memcpy_fn), error, error_size) ||
      !npc_difftest_load_symbol(difftest, "difftest_regcpy", &difftest->regcpy_fn,
                                sizeof(difftest->regcpy_fn), error, error_size) ||
      !npc_difftest_load_symbol(difftest, "difftest_exec", &difftest->exec_fn,
                                sizeof(difftest->exec_fn), error, error_size) ||
      !npc_difftest_load_symbol(difftest, "difftest_init", &difftest->init_fn,
                                sizeof(difftest->init_fn), error, error_size)) {
    return 0;
  }

  difftest->init_fn(0);
  difftest->memcpy_fn(NPC_PMEM_BASE, (void *)npc_memory_data(memory),
                      npc_memory_image_size(memory), true);
  memset(&initial, 0, sizeof(initial));
  initial.pc = reset_pc;
  difftest->regcpy_fn(&initial, true);
  difftest->enabled = 1;
  return 1;
}

int npc_difftest_enabled(const NpcDifftest *difftest) {
  return difftest != NULL && difftest->enabled;
}

int npc_difftest_step(NpcDifftest *difftest, const NpcCommit *dut,
                      char *error, size_t error_size) {
  NpcNemuState reference;
  unsigned int index;

  difftest->exec_fn(1);
  memset(&reference, 0, sizeof(reference));
  difftest->regcpy_fn(&reference, false);
  for (index = 0; index < NPC_GPR_COUNT; ++index) {
    if (reference.gpr[index] != dut->gpr[index]) {
      npc_set_error(error, error_size,
                    "DiffTest mismatch at pc=0x%x: x%u DUT=0x%x REF=0x%x", dut->pc,
                    index, dut->gpr[index], reference.gpr[index]);
      return 0;
    }
  }
  if (reference.pc != dut->dnpc) {
    npc_set_error(error, error_size,
                  "DiffTest mismatch at pc=0x%x: next pc DUT=0x%x REF=0x%x", dut->pc,
                  dut->dnpc, reference.pc);
    return 0;
  }
  return 1;
}
