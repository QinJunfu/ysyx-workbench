#ifndef NPC_VERILATOR_HOST_H
#define NPC_VERILATOR_HOST_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NpcVerilatorHost NpcVerilatorHost;

NpcVerilatorHost *npc_verilator_host_create(int argc, char **argv, char *error,
                                             size_t error_size);
void npc_verilator_host_destroy(NpcVerilatorHost *host);
void npc_verilator_host_set_clock(NpcVerilatorHost *host, int value);
void npc_verilator_host_set_reset(NpcVerilatorHost *host, int value);
void npc_verilator_host_evaluate(NpcVerilatorHost *host);
int npc_verilator_host_got_finish(const NpcVerilatorHost *host);

#ifdef __cplusplus
}
#endif

#endif
