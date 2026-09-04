#include "sdb.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

static char *npc_sdb_trim_left(char *text) {
  while (*text == ' ' || *text == '\t') {
    text += 1;
  }
  return text;
}

static char *npc_sdb_read_line(void) {
  char *line;
  size_t length;
  size_t capacity;
  int character;

  capacity = 128;
  line = (char *)malloc(capacity);
  if (line == NULL) {
    return NULL;
  }
  length = 0;
  while ((character = fgetc(stdin)) != EOF && character != '\n') {
    char *grown;

    if (length + 1 >= capacity) {
      capacity *= 2;
      grown = (char *)realloc(line, capacity);
      if (grown == NULL) {
        free(line);
        return NULL;
      }
      line = grown;
    }
    line[length] = (char)character;
    length += 1;
  }
  if (character == EOF && length == 0) {
    free(line);
    return NULL;
  }
  line[length] = '\0';
  return line;
}

static int npc_sdb_parse_u64(const char *text, uint64_t *value) {
  char *end;
  unsigned long long parsed;

  errno = 0;
  parsed = strtoull(text, &end, 0);
  if (end == text || errno == ERANGE) {
    return 0;
  }
  *value = (uint64_t)parsed;
  return 1;
}

static int npc_sdb_parse_int(const char *text, int *value) {
  char *end;
  long parsed;

  errno = 0;
  parsed = strtol(text, &end, 0);
  if (end == text || errno == ERANGE || parsed < INT_MIN || parsed > INT_MAX) {
    return 0;
  }
  *value = (int)parsed;
  return 1;
}

static void npc_sdb_print_error(const char *error) {
  printf("error: %s\n", error);
}

static void npc_sdb_command_x(NpcSimulator *simulator, char *rest) {
  char *end;
  char *expression;
  unsigned long count;
  uint32_t address;
  char error[NPC_ERROR_SIZE];

  errno = 0;
  count = strtoul(rest, &end, 10);
  expression = npc_sdb_trim_left(end);
  if (end == rest || errno == ERANGE || count > UINT_MAX || count == 0 ||
      expression[0] == '\0') {
    printf("usage: x N EXPR\n");
    return;
  }
  if (!npc_simulator_evaluate_expression(simulator, expression, &address, error,
                                         sizeof(error))) {
    npc_sdb_print_error(error);
    return;
  }
  npc_simulator_print_memory(simulator, (unsigned int)count, address);
}

void npc_sdb_run(NpcSimulator *simulator) {
  while (npc_simulator_is_active(simulator)) {
    char *line;
    char *command;
    char *rest;
    char *cursor;

    printf("(npc) ");
    fflush(stdout);
    line = npc_sdb_read_line();
    if (line == NULL) {
      return;
    }
    cursor = npc_sdb_trim_left(line);
    command = cursor;
    while (*cursor != '\0' && !isspace((unsigned char)*cursor)) {
      cursor += 1;
    }
    if (*cursor == '\0') {
      rest = cursor;
    } else {
      *cursor = '\0';
      rest = npc_sdb_trim_left(cursor + 1);
    }
    if (command[0] == '\0') {
      free(line);
      continue;
    }
    if (strcmp(command, "c") == 0) {
      npc_simulator_continue(simulator);
    } else if (strcmp(command, "si") == 0) {
      uint64_t count;

      count = 1;
      if (rest[0] != '\0' && !npc_sdb_parse_u64(rest, &count)) {
        npc_sdb_print_error("invalid step count");
      } else {
        npc_simulator_step(simulator, count);
      }
    } else if (strcmp(command, "info") == 0 && strcmp(rest, "r") == 0) {
      npc_simulator_print_registers(simulator);
    } else if (strcmp(command, "info") == 0 && strcmp(rest, "w") == 0) {
      npc_simulator_show_watchpoints(simulator);
    } else if (strcmp(command, "x") == 0) {
      npc_sdb_command_x(simulator, rest);
    } else if (strcmp(command, "p") == 0) {
      uint32_t value;
      char error[NPC_ERROR_SIZE];

      if (rest[0] == '\0') {
        printf("usage: p EXPR\n");
      } else if (npc_simulator_evaluate_expression(simulator, rest, &value, error,
                                                   sizeof(error))) {
        printf("0x%x\n", value);
      } else {
        npc_sdb_print_error(error);
      }
    } else if (strcmp(command, "w") == 0) {
      char error[NPC_ERROR_SIZE];

      if (rest[0] == '\0') {
        printf("usage: w EXPR\n");
      } else if (!npc_simulator_add_watchpoint(simulator, rest, error, sizeof(error))) {
        npc_sdb_print_error(error);
      }
    } else if (strcmp(command, "d") == 0) {
      int id;

      if (!npc_sdb_parse_int(rest, &id)) {
        npc_sdb_print_error("invalid watchpoint number");
      } else {
        npc_simulator_delete_watchpoint(simulator, id);
      }
    } else if (strcmp(command, "q") == 0) {
      npc_simulator_request_quit(simulator);
    } else if (strcmp(command, "help") == 0) {
      printf("c, si [N], info r, info w, x N EXPR, p EXPR, w EXPR, d N, q\n");
    } else {
      printf("Unknown command: %s\n", command);
    }
    free(line);
  }
}
