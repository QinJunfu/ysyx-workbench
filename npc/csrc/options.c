#include "options.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int npc_options_set_path(char *destination, const char *value, const char *name,
                                char *error, size_t error_size) {
  size_t length;

  length = strlen(value);
  if (length >= NPC_PATH_SIZE) {
    npc_set_error(error, error_size, "%s is too long", name);
    return 0;
  }
  memcpy(destination, value, length + 1);
  return 1;
}

static const char *npc_options_require_value(int argc, char **argv, int *index,
                                             const char *name, char *error,
                                             size_t error_size) {
  if (*index + 1 >= argc) {
    npc_set_error(error, error_size, "%s requires a value", name);
    return NULL;
  }
  *index += 1;
  return argv[*index];
}

static int npc_options_parse_u64(const char *text, const char *name, uint64_t *result,
                                 char *error, size_t error_size) {
  char *end;
  unsigned long long value;

  errno = 0;
  value = strtoull(text, &end, 0);
  if (text[0] == '\0' || *end != '\0' || errno == ERANGE) {
    npc_set_error(error, error_size, "%s requires a valid unsigned integer", name);
    return 0;
  }
  *result = (uint64_t)value;
  return 1;
}

NpcOptionsParseResult npc_options_parse(int argc, char **argv, NpcOptions *options,
                                        char *error, size_t error_size) {
  int index;

  memset(options, 0, sizeof(*options));
  options->max_cycles = UINT64_C(100000000);
  options->progress_interval = UINT64_C(1000000);
  for (index = 1; index < argc; ++index) {
    const char *argument;
    const char *value;

    argument = argv[index];
    if (strcmp(argument, "--image") == 0) {
      value = npc_options_require_value(argc, argv, &index, "--image", error, error_size);
      if (value == NULL || !npc_options_set_path(options->image, value, "--image", error,
                                                  error_size)) {
        return NPC_OPTIONS_ERROR;
      }
    } else if (strcmp(argument, "--elf") == 0) {
      value = npc_options_require_value(argc, argv, &index, "--elf", error, error_size);
      if (value == NULL || !npc_options_set_path(options->elf, value, "--elf", error,
                                                  error_size)) {
        return NPC_OPTIONS_ERROR;
      }
    } else if (strcmp(argument, "--mrom-image") == 0) {
      value = npc_options_require_value(argc, argv, &index, "--mrom-image", error,
                                        error_size);
      if (value == NULL || !npc_options_set_path(options->mrom_image, value,
                                                  "--mrom-image", error, error_size)) {
        return NPC_OPTIONS_ERROR;
      }
    } else if (strcmp(argument, "--flash-image") == 0) {
      value = npc_options_require_value(argc, argv, &index, "--flash-image", error,
                                        error_size);
      if (value == NULL || !npc_options_set_path(options->flash_image, value,
                                                  "--flash-image", error, error_size)) {
        return NPC_OPTIONS_ERROR;
      }
    } else if (strcmp(argument, "--diff") == 0 || strncmp(argument, "--diff=", 7) == 0) {
      npc_set_error(error, error_size,
                    "--diff is no longer supported; enable CONFIG_NPC_DIFFTEST with "
                    "'make menuconfig'");
      return NPC_OPTIONS_ERROR;
    } else if (strcmp(argument, "--batch") == 0) {
      options->batch = 1;
    } else if (strcmp(argument, "--nvboard") == 0) {
      options->nvboard = 1;
    } else if (strcmp(argument, "--max-cycles") == 0) {
      value = npc_options_require_value(argc, argv, &index, "--max-cycles", error,
                                        error_size);
      if (value == NULL || !npc_options_parse_u64(value, "--max-cycles",
                                                   &options->max_cycles, error,
                                                   error_size)) {
        return NPC_OPTIONS_ERROR;
      }
    } else if (strcmp(argument, "--progress-interval") == 0) {
      value = npc_options_require_value(argc, argv, &index, "--progress-interval", error,
                                        error_size);
      if (value == NULL || !npc_options_parse_u64(value, "--progress-interval",
                                                   &options->progress_interval, error,
                                                   error_size)) {
        return NPC_OPTIONS_ERROR;
      }
    } else if (strcmp(argument, "--waveform") == 0) {
      value = "build/waveform.vcd";
      if (index + 1 < argc && argv[index + 1][0] != '-') {
        index += 1;
        value = argv[index];
      }
      if (!npc_options_set_path(options->waveform, value, "--waveform", error,
                                error_size)) {
        return NPC_OPTIONS_ERROR;
      }
    } else if (strncmp(argument, "--waveform=", 11) == 0) {
      value = argument + 11;
      if (value[0] == '\0' || !npc_options_set_path(options->waveform, value,
                                                     "--waveform", error, error_size)) {
        return NPC_OPTIONS_ERROR;
      }
    } else if (strcmp(argument, "--itrace") == 0 || strncmp(argument, "--itrace=", 9) == 0) {
      npc_set_error(error, error_size,
                    "--itrace is no longer supported; enable CONFIG_NPC_ITRACE with "
                    "'make menuconfig'");
      return NPC_OPTIONS_ERROR;
    } else if (strcmp(argument, "--mtrace") == 0 || strncmp(argument, "--mtrace=", 9) == 0) {
      npc_set_error(error, error_size,
                    "--mtrace is no longer supported; enable CONFIG_NPC_MTRACE with "
                    "'make menuconfig'");
      return NPC_OPTIONS_ERROR;
    } else if (strcmp(argument, "--ftrace") == 0 || strncmp(argument, "--ftrace=", 9) == 0) {
      npc_set_error(error, error_size,
                    "--ftrace is no longer supported; enable CONFIG_NPC_FTRACE with "
                    "'make menuconfig'");
      return NPC_OPTIONS_ERROR;
    } else if (strcmp(argument, "--trace") == 0 || strncmp(argument, "--trace=", 8) == 0) {
      npc_set_error(error, error_size, "--trace was renamed to --waveform");
      return NPC_OPTIONS_ERROR;
    } else if (strcmp(argument, "--help") == 0) {
      printf("usage: npc --image IMAGE [--mrom-image IMAGE] [--flash-image IMAGE] "
             "[--elf ELF] [--batch] [--waveform [FILE]] "
             "[--max-cycles N] [--progress-interval N] [--nvboard]\n");
      return NPC_OPTIONS_HELP;
    } else {
      npc_set_error(error, error_size, "unknown option: %s", argument);
      return NPC_OPTIONS_ERROR;
    }
  }

  if (options->image[0] == '\0') {
    npc_set_error(error, error_size, "--image is required");
    return NPC_OPTIONS_ERROR;
  }
  return NPC_OPTIONS_OK;
}
