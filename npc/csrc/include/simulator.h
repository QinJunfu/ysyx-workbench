#ifndef NPC_SIMULATOR_H
#define NPC_SIMULATOR_H

#include <stddef.h>
#include <stdint.h>

#include "common.h"
#include "options.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NpcSimulator NpcSimulator;

NpcSimulator *npc_simulator_create(const NpcOptions *options, char *error, size_t error_size);
void npc_simulator_destroy(NpcSimulator *simulator);
int npc_simulator_load_image(NpcSimulator *simulator, char *error, size_t error_size);
int npc_simulator_initialize_difftest(NpcSimulator *simulator, char *error,
                                      size_t error_size);
int npc_simulator_initialize(NpcSimulator *simulator, int argc, char **argv,
                             char *error, size_t error_size);
int npc_simulator_run(NpcSimulator *simulator);

uint32_t npc_simulator_dpi_read(NpcSimulator *simulator, uint32_t address);
void npc_simulator_dpi_write(NpcSimulator *simulator, uint32_t address, uint32_t value,
                             uint8_t mask);
void npc_simulator_dpi_commit(NpcSimulator *simulator, const NpcCommit *commit);

int npc_simulator_is_active(const NpcSimulator *simulator);
void npc_simulator_continue(NpcSimulator *simulator);
void npc_simulator_step(NpcSimulator *simulator, uint64_t count);
void npc_simulator_request_quit(NpcSimulator *simulator);
int npc_simulator_evaluate_expression(const NpcSimulator *simulator, const char *expression,
                                      uint32_t *value, char *error, size_t error_size);
void npc_simulator_print_registers(const NpcSimulator *simulator);
void npc_simulator_print_memory(const NpcSimulator *simulator, unsigned int count,
                                uint32_t address);
int npc_simulator_add_watchpoint(NpcSimulator *simulator, const char *expression,
                                 char *error, size_t error_size);
void npc_simulator_delete_watchpoint(NpcSimulator *simulator, int id);
void npc_simulator_show_watchpoints(const NpcSimulator *simulator);

#ifdef __cplusplus
}
#endif

#endif
