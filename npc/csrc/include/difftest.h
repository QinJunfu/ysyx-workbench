#ifndef NPC_DIFFTEST_H
#define NPC_DIFFTEST_H

#include <stddef.h>
#include <stdint.h>

#include "common.h"
#include "memory.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NpcDifftest NpcDifftest;

NpcDifftest *npc_difftest_create(char *error, size_t error_size);
void npc_difftest_destroy(NpcDifftest *difftest);
int npc_difftest_initialize(NpcDifftest *difftest, const char *path,
                            const NpcMemory *memory, uint32_t reset_pc,
                            char *error, size_t error_size);
int npc_difftest_enabled(const NpcDifftest *difftest);
int npc_difftest_step(NpcDifftest *difftest, const NpcCommit *dut,
                      char *error, size_t error_size);
void npc_difftest_sync_mmio(NpcDifftest *difftest, const NpcCommit *dut);

#ifdef __cplusplus
}
#endif

#endif
