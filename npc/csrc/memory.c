#include "memory.h"

#include <stdio.h>
#include <stdlib.h>

#include "common.h"

struct NpcMemory {
  uint8_t *bytes;
  size_t image_size;
};

static int npc_memory_in_range(uint32_t address, size_t length) {
  uint64_t first;
  uint64_t last;

  first = address;
  last = first + length;
  return first >= NPC_PMEM_BASE && last <= (uint64_t)NPC_PMEM_BASE + NPC_PMEM_SIZE;
}

static void npc_memory_range_error(uint32_t address, size_t length, char *error,
                                   size_t error_size) {
  npc_set_error(error, error_size, "physical memory access out of range: addr=0x%x len=%zu",
                address, length);
}

NpcMemory *npc_memory_create(char *error, size_t error_size) {
  NpcMemory *memory;

  memory = (NpcMemory *)calloc(1, sizeof(*memory));
  if (memory == NULL) {
    npc_set_error(error, error_size, "cannot allocate physical memory state");
    return NULL;
  }
  memory->bytes = (uint8_t *)calloc(NPC_PMEM_SIZE, sizeof(*memory->bytes));
  if (memory->bytes == NULL) {
    free(memory);
    npc_set_error(error, error_size, "cannot allocate physical memory");
    return NULL;
  }
  return memory;
}

void npc_memory_destroy(NpcMemory *memory) {
  if (memory == NULL) {
    return;
  }
  free(memory->bytes);
  free(memory);
}

int npc_memory_load_image(NpcMemory *memory, const char *path, char *error,
                          size_t error_size) {
  FILE *input;
  long raw_size;
  size_t size;

  input = fopen(path, "rb");
  if (input == NULL) {
    npc_set_error(error, error_size, "cannot open image '%s'", path);
    return 0;
  }
  if (fseek(input, 0, SEEK_END) != 0 || (raw_size = ftell(input)) < 0) {
    fclose(input);
    npc_set_error(error, error_size, "cannot read image '%s'", path);
    return 0;
  }
  size = (size_t)raw_size;
  if (size > NPC_PMEM_SIZE) {
    fclose(input);
    npc_set_error(error, error_size, "image is larger than physical memory");
    return 0;
  }
  if (fseek(input, 0, SEEK_SET) != 0 ||
      (size != 0 && fread(memory->bytes, 1, size, input) != size)) {
    fclose(input);
    npc_set_error(error, error_size, "cannot read image '%s'", path);
    return 0;
  }
  fclose(input);
  memory->image_size = size;
  return 1;
}

int npc_memory_read_word(const NpcMemory *memory, uint32_t address, uint32_t *value,
                         char *error, size_t error_size) {
  size_t offset;

  if ((address & 0x3u) != 0 || !npc_memory_in_range(address, 4)) {
    npc_memory_range_error(address, 4, error, error_size);
    return 0;
  }
  offset = (size_t)(address - NPC_PMEM_BASE);
  *value = (uint32_t)memory->bytes[offset] |
           ((uint32_t)memory->bytes[offset + 1] << 8) |
           ((uint32_t)memory->bytes[offset + 2] << 16) |
           ((uint32_t)memory->bytes[offset + 3] << 24);
  return 1;
}

int npc_memory_write_word_masked(NpcMemory *memory, uint32_t address, uint32_t value,
                                 uint8_t mask, char *error, size_t error_size) {
  size_t offset;
  unsigned int index;

  if ((address & 0x3u) != 0 || !npc_memory_in_range(address, 4)) {
    npc_memory_range_error(address, 4, error, error_size);
    return 0;
  }
  offset = (size_t)(address - NPC_PMEM_BASE);
  for (index = 0; index < 4; ++index) {
    if ((mask & (uint8_t)(1u << index)) != 0) {
      memory->bytes[offset + index] = (uint8_t)(value >> (index * 8));
    }
  }
  return 1;
}

int npc_memory_read(const NpcMemory *memory, uint32_t address, unsigned int length,
                    uint32_t *value, char *error, size_t error_size) {
  size_t offset;
  uint32_t result;
  unsigned int index;

  if (length == 0 || length > 4 || !npc_memory_in_range(address, length)) {
    npc_memory_range_error(address, length, error, error_size);
    return 0;
  }
  offset = (size_t)(address - NPC_PMEM_BASE);
  result = 0;
  for (index = 0; index < length; ++index) {
    result |= (uint32_t)memory->bytes[offset + index] << (index * 8);
  }
  *value = result;
  return 1;
}

const uint8_t *npc_memory_data(const NpcMemory *memory) {
  return memory->bytes;
}

size_t npc_memory_image_size(const NpcMemory *memory) {
  return memory->image_size;
}
