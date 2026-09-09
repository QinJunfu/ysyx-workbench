#include "memory.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

struct NpcMemory {
  uint8_t *bytes;
  size_t image_size;
  uint8_t mrom[NPC_MROM_SIZE];
  size_t mrom_size;
  uint8_t *flash;
  size_t flash_size;
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
  free(memory->flash);
  free(memory);
}

static int npc_memory_load_fixed(uint8_t *destination, size_t capacity, size_t *loaded_size,
                                 const char *path, const char *region_name, char *error,
                                 size_t error_size) {
  FILE *input;
  long raw_size;
  size_t size;

  input = fopen(path, "rb");
  if (input == NULL) {
    npc_set_error(error, error_size, "cannot open %s image '%s'", region_name, path);
    return 0;
  }
  if (fseek(input, 0, SEEK_END) != 0 || (raw_size = ftell(input)) < 0) {
    fclose(input);
    npc_set_error(error, error_size, "cannot read %s image '%s'", region_name, path);
    return 0;
  }
  size = (size_t)raw_size;
  if (size > capacity) {
    fclose(input);
    npc_set_error(error, error_size, "%s image is larger than %zu bytes", region_name,
                  capacity);
    return 0;
  }
  memset(destination, 0, capacity);
  if (fseek(input, 0, SEEK_SET) != 0 ||
      (size != 0 && fread(destination, 1, size, input) != size)) {
    fclose(input);
    npc_set_error(error, error_size, "cannot read %s image '%s'", region_name, path);
    return 0;
  }
  fclose(input);
  *loaded_size = size;
  return 1;
}

int npc_memory_load_mrom(NpcMemory *memory, const char *path, char *error,
                         size_t error_size) {
  return npc_memory_load_fixed(memory->mrom, sizeof(memory->mrom), &memory->mrom_size,
                               path, "MROM", error, error_size);
}

int npc_memory_load_flash(NpcMemory *memory, const char *path, char *error,
                          size_t error_size) {
  FILE *input;
  long raw_size;
  size_t size;
  uint8_t *contents;

  input = fopen(path, "rb");
  if (input == NULL) {
    npc_set_error(error, error_size, "cannot open flash image '%s'", path);
    return 0;
  }
  if (fseek(input, 0, SEEK_END) != 0 || (raw_size = ftell(input)) < 0) {
    fclose(input);
    npc_set_error(error, error_size, "cannot read flash image '%s'", path);
    return 0;
  }
  size = (size_t)raw_size;
  if (size > NPC_FLASH_SIZE) {
    fclose(input);
    npc_set_error(error, error_size, "flash image is larger than %zu bytes", NPC_FLASH_SIZE);
    return 0;
  }
  contents = (uint8_t *)calloc(size == 0 ? 1 : size, sizeof(*contents));
  if (contents == NULL) {
    fclose(input);
    npc_set_error(error, error_size, "cannot allocate flash image");
    return 0;
  }
  if (fseek(input, 0, SEEK_SET) != 0 ||
      (size != 0 && fread(contents, 1, size, input) != size)) {
    fclose(input);
    free(contents);
    npc_set_error(error, error_size, "cannot read flash image '%s'", path);
    return 0;
  }
  fclose(input);
  free(memory->flash);
  memory->flash = contents;
  memory->flash_size = size;
  return 1;
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

static uint32_t npc_memory_read_le_word(const uint8_t *bytes, size_t offset) {
  return (uint32_t)bytes[offset] |
         ((uint32_t)bytes[offset + 1] << 8) |
         ((uint32_t)bytes[offset + 2] << 16) |
         ((uint32_t)bytes[offset + 3] << 24);
}

int npc_memory_read_mrom_word(const NpcMemory *memory, uint32_t address, uint32_t *value,
                              char *error, size_t error_size) {
  uint32_t aligned_address;
  uint64_t offset;

  if (address < NPC_MROM_BASE) {
    npc_set_error(error, error_size, "MROM read out of range: addr=0x%x", address);
    return 0;
  }
  aligned_address = address & UINT32_C(0xfffffffc);
  offset = (uint64_t)aligned_address - NPC_MROM_BASE;
  if (offset + 4 > NPC_MROM_SIZE) {
    npc_set_error(error, error_size, "MROM read out of range: addr=0x%x", address);
    return 0;
  }
  *value = npc_memory_read_le_word(memory->mrom, (size_t)offset);
  return 1;
}

int npc_memory_read_flash_word(const NpcMemory *memory, uint32_t address, uint32_t *value,
                               char *error, size_t error_size) {
  uint64_t offset;

  offset = address >= NPC_FLASH_BASE ? (uint64_t)address - NPC_FLASH_BASE : address;
  if (memory->flash == NULL || offset + 4 > memory->flash_size) {
    npc_set_error(error, error_size, "flash read out of image: addr=0x%x", address);
    return 0;
  }
  *value = npc_memory_read_le_word(memory->flash, (size_t)offset);
  return 1;
}

const uint8_t *npc_memory_data(const NpcMemory *memory) {
  return memory->bytes;
}

size_t npc_memory_image_size(const NpcMemory *memory) {
  return memory->image_size;
}

const uint8_t *npc_memory_mrom_data(const NpcMemory *memory) {
  return memory->mrom;
}

size_t npc_memory_mrom_size(const NpcMemory *memory) {
  return memory->mrom_size;
}

const uint8_t *npc_memory_flash_data(const NpcMemory *memory) {
  return memory->flash;
}

size_t npc_memory_flash_size(const NpcMemory *memory) {
  return memory->flash_size;
}
