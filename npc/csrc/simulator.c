#include "simulator.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <generated/autoconf.h>

#include "device.h"
#include "difftest.h"
#include "dpi.h"
#include "expr.h"
#include "memory.h"
#include "sdb.h"
#include "trace.h"
#include "verilator_host.h"

#ifndef NPC_CONFIG_ROOT
#define NPC_CONFIG_ROOT "."
#endif

typedef struct {
  int id;
  char *expression;
  uint32_t value;
} NpcWatchpoint;

struct NpcSimulator {
  NpcOptions options;
  NpcMemory *memory;
  NpcDevices devices;
  NpcTrace *trace;
  NpcDifftest *difftest;
  NpcVerilatorHost *host;
  NpcCommit state;
  NpcWatchpoint *watchpoints;
  size_t watchpoint_count;
  size_t watchpoint_capacity;
  int next_watchpoint_id;
  uint64_t cycles;
  uint64_t instructions;
  int halted;
  int fatal;
  int reset_active;
  int stop_requested;
  int quit;
  uint32_t halt_code;
};

static void npc_simulator_fatal(NpcSimulator *simulator, const char *message) {
  if (!simulator->fatal) {
    fprintf(stderr, "NPC error: %s\n", message);
  }
  simulator->fatal = 1;
  simulator->stop_requested = 1;
}

static void npc_simulator_evaluate(NpcSimulator *simulator) {
  npc_verilator_host_evaluate(simulator->host);
}

static void npc_simulator_single_cycle(NpcSimulator *simulator) {
  npc_verilator_host_update_devices(simulator->host);
  npc_verilator_host_set_clock(simulator->host, 0);
  npc_simulator_evaluate(simulator);
  npc_verilator_host_set_clock(simulator->host, 1);
  npc_simulator_evaluate(simulator);
  npc_verilator_host_set_clock(simulator->host, 0);
  npc_simulator_evaluate(simulator);
  simulator->cycles += 1;
  if (simulator->options.progress_interval != 0 &&
      simulator->cycles % simulator->options.progress_interval == 0) {
    fprintf(stderr, "[NPC] cycles=%llu instructions=%llu pc=0x%08x\n",
            (unsigned long long)simulator->cycles,
            (unsigned long long)simulator->instructions, simulator->state.dnpc);
  }
  if (simulator->options.max_cycles != 0 &&
      simulator->cycles >= simulator->options.max_cycles &&
      !simulator->halted && !simulator->fatal && !simulator->quit) {
    char message[NPC_ERROR_SIZE];

    (void)snprintf(message, sizeof(message),
                   "cycle limit reached after %llu cycles and %llu instructions",
                   (unsigned long long)simulator->cycles,
                   (unsigned long long)simulator->instructions);
    npc_simulator_fatal(simulator, message);
  }
}

static void npc_simulator_check_watchpoints(NpcSimulator *simulator) {
  size_t index;

  for (index = 0; index < simulator->watchpoint_count; ++index) {
    NpcWatchpoint *watchpoint;
    uint32_t value;
    char error[NPC_ERROR_SIZE];

    watchpoint = &simulator->watchpoints[index];
    if (!npc_expr_evaluate(watchpoint->expression, &simulator->state, simulator->memory,
                           &value, error, sizeof(error))) {
      char message[NPC_ERROR_SIZE];

      npc_set_error(message, sizeof(message), "watchpoint evaluation failed: %.450s", error);
      npc_simulator_fatal(simulator, message);
      return;
    }
    if (value != watchpoint->value) {
      printf("Watchpoint %d: %s changed 0x%x -> 0x%x\n", watchpoint->id,
             watchpoint->expression, watchpoint->value, value);
      watchpoint->value = value;
      simulator->stop_requested = 1;
    }
  }
}

static void npc_simulator_execute(NpcSimulator *simulator) {
  simulator->stop_requested = 0;
  while (!simulator->halted && !simulator->fatal && !simulator->quit &&
         !simulator->stop_requested && !npc_verilator_host_got_finish(simulator->host)) {
    npc_simulator_single_cycle(simulator);
  }
}

static int npc_simulator_exit_status(const NpcSimulator *simulator) {
  if (simulator->fatal) {
    return 1;
  }
  if (simulator->halted) {
    if (simulator->halt_code == 0) {
      printf("HIT GOOD TRAP at pc=0x%x after %llu instructions\n", simulator->state.pc,
             (unsigned long long)simulator->instructions);
      return 0;
    }
    fprintf(stderr, "HIT BAD TRAP(code=%u) at pc=0x%x\n", simulator->halt_code,
            simulator->state.pc);
    return 1;
  }
  return simulator->quit ? 0 : 1;
}

#ifdef CONFIG_NPC_DIFFTEST
static int npc_simulator_normalize_path(const char *path, char *normalized,
                                        size_t normalized_size) {
  char work[NPC_PATH_SIZE * 2u];
  char *components[NPC_PATH_SIZE * 2u];
  size_t component_count;
  size_t path_length;
  size_t used;
  int absolute;
  char *cursor;

  path_length = strlen(path);
  if (path_length >= sizeof(work) || normalized_size == 0) {
    return 0;
  }
  memcpy(work, path, path_length + 1);
  absolute = work[0] == '/';
  component_count = 0;
  cursor = work;
  while (*cursor != '\0') {
    char *component;

    while (*cursor == '/') {
      cursor += 1;
    }
    if (*cursor == '\0') {
      break;
    }
    component = cursor;
    while (*cursor != '\0' && *cursor != '/') {
      cursor += 1;
    }
    if (*cursor == '/') {
      *cursor = '\0';
      cursor += 1;
    }
    if (strcmp(component, ".") == 0) {
      continue;
    }
    if (strcmp(component, "..") == 0) {
      if (component_count > 0 && strcmp(components[component_count - 1], "..") != 0) {
        component_count -= 1;
      } else if (!absolute) {
        components[component_count] = component;
        component_count += 1;
      }
      continue;
    }
    components[component_count] = component;
    component_count += 1;
  }

  used = 0;
  if (absolute) {
    if (used + 1 >= normalized_size) {
      return 0;
    }
    normalized[used] = '/';
    used += 1;
  }
  for (path_length = 0; path_length < component_count; ++path_length) {
    size_t component_length;

    component_length = strlen(components[path_length]);
    if (used != 0 && normalized[used - 1] != '/') {
      if (used + 1 >= normalized_size) {
        return 0;
      }
      normalized[used] = '/';
      used += 1;
    }
    if (component_length >= normalized_size - used) {
      return 0;
    }
    memcpy(normalized + used, components[path_length], component_length);
    used += component_length;
  }
  if (used == 0) {
    if (normalized_size < 2) {
      return 0;
    }
    normalized[0] = absolute ? '/' : '.';
    used = 1;
  }
  normalized[used] = '\0';
  return 1;
}
#endif

NpcSimulator *npc_simulator_create(const NpcOptions *options, char *error,
                                   size_t error_size) {
  NpcSimulator *simulator;

  simulator = (NpcSimulator *)calloc(1, sizeof(*simulator));
  if (simulator == NULL) {
    npc_set_error(error, error_size, "cannot allocate simulator state");
    return NULL;
  }
  simulator->options = *options;
  simulator->next_watchpoint_id = 1;
  simulator->memory = npc_memory_create(error, error_size);
  if (simulator->memory == NULL) {
    npc_simulator_destroy(simulator);
    return NULL;
  }
  simulator->trace = npc_trace_create(&simulator->options, error, error_size);
  if (simulator->trace == NULL) {
    npc_simulator_destroy(simulator);
    return NULL;
  }
  simulator->difftest = npc_difftest_create(error, error_size);
  if (simulator->difftest == NULL) {
    npc_simulator_destroy(simulator);
    return NULL;
  }
  return simulator;
}

void npc_simulator_destroy(NpcSimulator *simulator) {
  size_t index;

  if (simulator == NULL) {
    return;
  }
  npc_verilator_host_destroy(simulator->host);
  npc_dpi_set_simulator(NULL);
  npc_difftest_destroy(simulator->difftest);
  npc_trace_destroy(simulator->trace);
  npc_memory_destroy(simulator->memory);
  for (index = 0; index < simulator->watchpoint_count; ++index) {
    free(simulator->watchpoints[index].expression);
  }
  free(simulator->watchpoints);
  free(simulator);
}

int npc_simulator_load_image(NpcSimulator *simulator, char *error, size_t error_size) {
#ifdef CONFIG_NPC_PLATFORM_YSYXSOC
  const char *mrom_path;

  mrom_path = simulator->options.mrom_image;
  if (mrom_path[0] == '\0' && simulator->options.flash_image[0] == '\0') {
    mrom_path = simulator->options.image;
  }
  if (mrom_path[0] != '\0' &&
      !npc_memory_load_mrom(simulator->memory, mrom_path, error, error_size)) {
    return 0;
  }
  if (simulator->options.flash_image[0] != '\0' &&
      !npc_memory_load_flash(simulator->memory, simulator->options.flash_image, error,
                             error_size)) {
    return 0;
  }
  return 1;
#else
  return npc_memory_load_image(simulator->memory, simulator->options.image, error,
                               error_size);
#endif
}

int npc_simulator_initialize_difftest(NpcSimulator *simulator, char *error,
                                      size_t error_size) {
#ifdef CONFIG_NPC_DIFFTEST
  const char *configured_path;
  char joined_path[NPC_PATH_SIZE * 2u];
  char reference_path[NPC_PATH_SIZE * 2u];
  int written;

  configured_path = CONFIG_NPC_DIFFTEST_REF_PATH;
  if (configured_path[0] == '\0') {
    npc_set_error(error, error_size,
                  "CONFIG_NPC_DIFFTEST_REF_PATH is empty; set it with 'make menuconfig'");
    return 0;
  }
  if (configured_path[0] == '/') {
    written = snprintf(joined_path, sizeof(joined_path), "%s", configured_path);
  } else {
    written = snprintf(joined_path, sizeof(joined_path), "%s/%s", NPC_CONFIG_ROOT,
                       configured_path);
  }
  if (written < 0 || (size_t)written >= sizeof(joined_path) ||
      !npc_simulator_normalize_path(joined_path, reference_path, sizeof(reference_path))) {
    npc_set_error(error, error_size, "CONFIG_NPC_DIFFTEST_REF_PATH is too long");
    return 0;
  }
  return npc_difftest_initialize(simulator->difftest, reference_path, simulator->memory,
                                  NPC_RESET_PC, error, error_size);
#else
  (void)simulator;
  (void)error;
  (void)error_size;
  return 1;
#endif
}

int npc_simulator_initialize(NpcSimulator *simulator, int argc, char **argv,
                             char *error, size_t error_size) {
  unsigned int reset_assert_cycles;
  unsigned int reset_drain_cycles;
  unsigned int index;

  simulator->host = npc_verilator_host_create(argc, argv, &simulator->options, error,
                                               error_size);
  if (simulator->host == NULL) {
    return 0;
  }

  npc_devices_init(&simulator->devices);
  memset(&simulator->state, 0, sizeof(simulator->state));
  simulator->state.pc = NPC_RESET_PC;
  simulator->state.dnpc = NPC_RESET_PC;
  npc_dpi_set_simulator(simulator);
  simulator->reset_active = 1;
  npc_verilator_host_set_clock(simulator->host, 0);
  npc_verilator_host_set_reset(simulator->host, 1);
  npc_simulator_evaluate(simulator);
#ifdef CONFIG_NPC_PLATFORM_YSYXSOC
  reset_assert_cycles = 16;
  reset_drain_cycles = 10;
#else
  reset_assert_cycles = 1;
  reset_drain_cycles = 0;
#endif
  for (index = 0; index < reset_assert_cycles; ++index) {
    npc_verilator_host_set_clock(simulator->host, 1);
    npc_simulator_evaluate(simulator);
    npc_verilator_host_set_clock(simulator->host, 0);
    npc_simulator_evaluate(simulator);
  }
  npc_verilator_host_set_reset(simulator->host, 0);
  npc_simulator_evaluate(simulator);
  // ysyxSoC inserts a ten-stage reset synchronizer in front of the CPU.
  // Drain it before execution so reset cannot arrive after instructions retire.
  for (index = 0; index < reset_drain_cycles; ++index) {
    npc_verilator_host_set_clock(simulator->host, 1);
    npc_simulator_evaluate(simulator);
    npc_verilator_host_set_clock(simulator->host, 0);
    npc_simulator_evaluate(simulator);
  }
  simulator->reset_active = 0;
  return 1;
}

int npc_simulator_run(NpcSimulator *simulator) {
  if (simulator->options.batch) {
    npc_simulator_execute(simulator);
  } else {
    printf("NPC sdb. Type 'help' for commands.\n");
    npc_sdb_run(simulator);
  }
  return npc_simulator_exit_status(simulator);
}

uint32_t npc_simulator_dpi_read(NpcSimulator *simulator, uint32_t address) {
  uint32_t value;
  char error[NPC_ERROR_SIZE];

  value = 0;
  if (npc_devices_read(&simulator->devices, address, &value)) {
    return value;
  }
  if (!npc_memory_read_word(simulator->memory, address, &value, error, sizeof(error))) {
    if (simulator->reset_active) {
      return 0;
    }
    npc_simulator_fatal(simulator, error);
  }
  return value;
}

void npc_simulator_dpi_write(NpcSimulator *simulator, uint32_t address, uint32_t value,
                             uint8_t mask) {
  char error[NPC_ERROR_SIZE];

  if (npc_devices_write(&simulator->devices, address, value, mask)) {
    return;
  }
  if (!npc_memory_write_word_masked(simulator->memory, address, value, mask, error,
                                    sizeof(error))) {
    npc_simulator_fatal(simulator, error);
  }
}

uint32_t npc_simulator_mrom_read(NpcSimulator *simulator, uint32_t address) {
  uint32_t value;
  char error[NPC_ERROR_SIZE];

  value = 0;
  if (!npc_memory_read_mrom_word(simulator->memory, address, &value, error,
                                 sizeof(error)) && !simulator->reset_active) {
    npc_simulator_fatal(simulator, error);
  }
  return value;
}

uint32_t npc_simulator_flash_read(NpcSimulator *simulator, uint32_t address) {
  uint32_t value;
  char error[NPC_ERROR_SIZE];

  value = 0;
  if (!npc_memory_read_flash_word(simulator->memory, address, &value, error,
                                  sizeof(error)) && !simulator->reset_active) {
    npc_simulator_fatal(simulator, error);
  }
  return value;
}

static int npc_simulator_accesses_cycle_csr(uint32_t instruction) {
  uint32_t csr;
  uint32_t funct3;

  if ((instruction & UINT32_C(0x7f)) != UINT32_C(0x73)) {
    return 0;
  }
  funct3 = (instruction >> 12) & UINT32_C(0x7);
  if (funct3 == 0) {
    return 0;
  }
  csr = instruction >> 20;
  return csr == UINT32_C(0xb00) || csr == UINT32_C(0xb80);
}

void npc_simulator_dpi_commit(NpcSimulator *simulator, const NpcCommit *commit) {
  char error[NPC_ERROR_SIZE];

  if (simulator->fatal) {
    return;
  }
  simulator->state = *commit;
  simulator->state.gpr[0] = 0;
  simulator->instructions += 1;
  npc_trace_record(simulator->trace, &simulator->state);
  npc_simulator_check_watchpoints(simulator);
  if (!simulator->state.invalid && npc_difftest_enabled(simulator->difftest)) {
    if ((simulator->state.mem_valid &&
         npc_devices_accesses_mmio(simulator->state.mem_addr, simulator->state.mem_mask)) ||
        npc_simulator_accesses_cycle_csr(simulator->state.inst)) {
      npc_difftest_sync_to_dut(simulator->difftest, &simulator->state);
    } else if (!npc_difftest_step(simulator->difftest, &simulator->state, error,
                                  sizeof(error))) {
      npc_simulator_fatal(simulator, error);
      npc_trace_dump_recent(simulator->trace);
    }
  }
  if (simulator->state.invalid) {
    char message[NPC_ERROR_SIZE];

    (void)snprintf(message, sizeof(message), "invalid instruction 0x%x at pc=0x%x",
                   simulator->state.inst, simulator->state.pc);
    npc_simulator_fatal(simulator, message);
    npc_trace_dump_recent(simulator->trace);
    return;
  }
  if (simulator->state.halt) {
    simulator->halted = 1;
    simulator->halt_code = simulator->state.halt_code;
    simulator->stop_requested = 1;
  }
}

int npc_simulator_is_active(const NpcSimulator *simulator) {
  return !simulator->halted && !simulator->fatal && !simulator->quit;
}

void npc_simulator_continue(NpcSimulator *simulator) {
  npc_simulator_execute(simulator);
}

void npc_simulator_step(NpcSimulator *simulator, uint64_t count) {
  uint64_t executed;

  simulator->stop_requested = 0;
  for (executed = 0; executed < count && !simulator->halted && !simulator->fatal &&
                     !simulator->quit && !simulator->stop_requested &&
                     !npc_verilator_host_got_finish(simulator->host);
       ++executed) {
    npc_simulator_single_cycle(simulator);
  }
}

void npc_simulator_request_quit(NpcSimulator *simulator) {
  simulator->quit = 1;
}

int npc_simulator_evaluate_expression(const NpcSimulator *simulator, const char *expression,
                                      uint32_t *value, char *error, size_t error_size) {
  return npc_expr_evaluate(expression, &simulator->state, simulator->memory, value, error,
                           error_size);
}

void npc_simulator_print_registers(const NpcSimulator *simulator) {
  static const char *const names[NPC_GPR_COUNT] = {
      "zero", "ra", "sp", "gp", "tp", "t0", "t1", "t2",
      "s0", "s1", "a0", "a1", "a2", "a3", "a4", "a5"};
  unsigned int index;

  for (index = 0; index < NPC_GPR_COUNT; ++index) {
    printf("%4s (x%2u) = 0x%08x\n", names[index], index, simulator->state.gpr[index]);
  }
  printf("  pc       = 0x%08x\n", simulator->state.dnpc);
}

void npc_simulator_print_memory(const NpcSimulator *simulator, unsigned int count,
                                uint32_t address) {
  unsigned int index;

  for (index = 0; index < count; ++index) {
    uint32_t value;
    uint32_t current;
    char error[NPC_ERROR_SIZE];

    current = address + index * 4u;
    value = 0;
    if (!npc_memory_read(simulator->memory, current, 4, &value, error, sizeof(error))) {
      printf("%s\n", error);
      return;
    }
    printf("0x%08x: 0x%08x\n", current, value);
  }
}

int npc_simulator_add_watchpoint(NpcSimulator *simulator, const char *expression,
                                 char *error, size_t error_size) {
  uint32_t value;
  NpcWatchpoint *watchpoints;
  size_t new_capacity;
  char *copy;
  int id;

  if (!npc_simulator_evaluate_expression(simulator, expression, &value, error, error_size)) {
    return 0;
  }
  if (simulator->watchpoint_count == simulator->watchpoint_capacity) {
    new_capacity = simulator->watchpoint_capacity == 0 ? 8 :
                                                  simulator->watchpoint_capacity * 2;
    watchpoints = (NpcWatchpoint *)realloc(simulator->watchpoints,
                                           new_capacity * sizeof(*simulator->watchpoints));
    if (watchpoints == NULL) {
      npc_set_error(error, error_size, "cannot allocate watchpoint");
      return 0;
    }
    simulator->watchpoints = watchpoints;
    simulator->watchpoint_capacity = new_capacity;
  }
  copy = npc_copy_string(expression);
  if (copy == NULL) {
    npc_set_error(error, error_size, "cannot allocate watchpoint expression");
    return 0;
  }
  id = simulator->next_watchpoint_id;
  simulator->next_watchpoint_id += 1;
  simulator->watchpoints[simulator->watchpoint_count].id = id;
  simulator->watchpoints[simulator->watchpoint_count].expression = copy;
  simulator->watchpoints[simulator->watchpoint_count].value = value;
  simulator->watchpoint_count += 1;
  printf("Watchpoint %d: %s = 0x%x\n", id, expression, value);
  return 1;
}

void npc_simulator_delete_watchpoint(NpcSimulator *simulator, int id) {
  size_t index;

  for (index = 0; index < simulator->watchpoint_count; ++index) {
    if (simulator->watchpoints[index].id == id) {
      free(simulator->watchpoints[index].expression);
      if (index + 1 < simulator->watchpoint_count) {
        memmove(&simulator->watchpoints[index], &simulator->watchpoints[index + 1],
                (simulator->watchpoint_count - index - 1) *
                    sizeof(*simulator->watchpoints));
      }
      simulator->watchpoint_count -= 1;
      printf("Deleted watchpoint %d\n", id);
      return;
    }
  }
  printf("No watchpoint %d\n", id);
}

void npc_simulator_show_watchpoints(const NpcSimulator *simulator) {
  size_t index;

  if (simulator->watchpoint_count == 0) {
    printf("No watchpoints.\n");
    return;
  }
  for (index = 0; index < simulator->watchpoint_count; ++index) {
    const NpcWatchpoint *watchpoint;

    watchpoint = &simulator->watchpoints[index];
    printf("%d: %s = 0x%x\n", watchpoint->id, watchpoint->expression, watchpoint->value);
  }
}
