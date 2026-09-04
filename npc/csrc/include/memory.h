#ifndef NPC_MEMORY_H
#define NPC_MEMORY_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NpcMemory NpcMemory;

NpcMemory *npc_memory_create(char *error, size_t error_size);
void npc_memory_destroy(NpcMemory *memory);
int npc_memory_load_image(NpcMemory *memory, const char *path, char *error, size_t error_size);
int npc_memory_read_word(const NpcMemory *memory, uint32_t address, uint32_t *value,
                         char *error, size_t error_size);
int npc_memory_write_word_masked(NpcMemory *memory, uint32_t address, uint32_t value,
                                 uint8_t mask, char *error, size_t error_size);
int npc_memory_read(const NpcMemory *memory, uint32_t address, unsigned int length,
                    uint32_t *value, char *error, size_t error_size);
const uint8_t *npc_memory_data(const NpcMemory *memory);
size_t npc_memory_image_size(const NpcMemory *memory);

#ifdef __cplusplus
}
#endif

#endif
