#ifndef NPC_EXPR_H
#define NPC_EXPR_H

#include <stddef.h>
#include <stdint.h>

#include "common.h"
#include "memory.h"

#ifdef __cplusplus
extern "C" {
#endif

int npc_expr_evaluate(const char *input, const NpcCommit *state, const NpcMemory *memory,
                      uint32_t *value, char *error, size_t error_size);

#ifdef __cplusplus
}
#endif

#endif
