#ifndef NPC_TRACE_H
#define NPC_TRACE_H

#include <stddef.h>

#include "common.h"
#include "options.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NpcTrace NpcTrace;

NpcTrace *npc_trace_create(const NpcOptions *options, char *error, size_t error_size);
void npc_trace_destroy(NpcTrace *trace);
void npc_trace_record(NpcTrace *trace, const NpcCommit *commit);
void npc_trace_dump_recent(const NpcTrace *trace);

#ifdef __cplusplus
}
#endif

#endif
