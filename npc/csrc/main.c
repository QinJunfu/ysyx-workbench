#include <stdio.h>

#include "common.h"
#include "options.h"
#include "simulator.h"

int main(int argc, char **argv) {
  NpcOptions options;
  NpcOptionsParseResult options_result;
  NpcSimulator *simulator;
  char error[NPC_ERROR_SIZE];
  int status;

  options_result = npc_options_parse(argc, argv, &options, error, sizeof(error));
  if (options_result == NPC_OPTIONS_HELP) {
    return 0;
  }
  if (options_result != NPC_OPTIONS_OK) {
    fprintf(stderr, "NPC error: %s\n", error);
    return 1;
  }
  simulator = npc_simulator_create(&options, error, sizeof(error));
  if (simulator == NULL) {
    fprintf(stderr, "NPC error: %s\n", error);
    return 1;
  }
  if (!npc_simulator_load_image(simulator, error, sizeof(error)) ||
      !npc_simulator_initialize_difftest(simulator, error, sizeof(error)) ||
      !npc_simulator_initialize(simulator, argc, argv, error, sizeof(error))) {
    fprintf(stderr, "NPC error: %s\n", error);
    npc_simulator_destroy(simulator);
    return 1;
  }
  status = npc_simulator_run(simulator);
  npc_simulator_destroy(simulator);
  return status;
}
