#ifndef NPC_COMMON_H
#define NPC_COMMON_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NPC_PMEM_BASE 0x80000000u
#define NPC_PMEM_SIZE ((size_t)128 * 1024u * 1024u)
#define NPC_GPR_COUNT 16u
#define NPC_ERROR_SIZE 512u
#define NPC_PATH_SIZE 4096u
#define NPC_ITRACE_DEPTH 16u

typedef struct {
  uint32_t pc;
  uint32_t inst;
  uint32_t dnpc;
  uint32_t gpr[NPC_GPR_COUNT];
  int mem_valid;
  int mem_write;
  uint32_t mem_addr;
  uint32_t mem_data;
  uint8_t mem_mask;
  int halt;
  uint32_t halt_code;
  int invalid;
} NpcCommit;

void npc_set_error(char *buffer, size_t buffer_size, const char *format, ...);
char *npc_copy_string(const char *text);

#ifdef __cplusplus
}
#endif

#endif
