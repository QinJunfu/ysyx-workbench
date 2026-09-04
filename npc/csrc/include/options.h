#ifndef NPC_OPTIONS_H
#define NPC_OPTIONS_H

#include <stddef.h>

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  char image[NPC_PATH_SIZE];
  char elf[NPC_PATH_SIZE];
  int batch;
} NpcOptions;

typedef enum {
  NPC_OPTIONS_ERROR = -1,
  NPC_OPTIONS_OK = 0,
  NPC_OPTIONS_HELP = 1
} NpcOptionsParseResult;

NpcOptionsParseResult npc_options_parse(int argc, char **argv, NpcOptions *options,
                                        char *error, size_t error_size);

#ifdef __cplusplus
}
#endif

#endif
