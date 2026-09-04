#include "expr.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  const char *input;
  size_t length;
  size_t position;
  const NpcCommit *state;
  const NpcMemory *memory;
  char *error;
  size_t error_size;
} NpcExpressionParser;

static void npc_expr_skip_space(NpcExpressionParser *parser) {
  while (parser->position < parser->length &&
         isspace((unsigned char)parser->input[parser->position])) {
    parser->position += 1;
  }
}

static int npc_expr_consume(NpcExpressionParser *parser, char character) {
  npc_expr_skip_space(parser);
  if (parser->position < parser->length && parser->input[parser->position] == character) {
    parser->position += 1;
    return 1;
  }
  return 0;
}

static int npc_expr_expression(NpcExpressionParser *parser, uint32_t *value);

static int npc_expr_register_value(NpcExpressionParser *parser, size_t begin, size_t length,
                                   uint32_t *value) {
  static const char *const names[NPC_GPR_COUNT] = {
      "zero", "ra", "sp", "gp", "tp", "t0", "t1", "t2",
      "s0", "s1", "a0", "a1", "a2", "a3", "a4", "a5"};
  unsigned int index;
  char number[16];

  if (length == 2 && strncmp(parser->input + begin, "pc", 2) == 0) {
    *value = parser->state->dnpc;
    return 1;
  }
  for (index = 0; index < NPC_GPR_COUNT; ++index) {
    int written;

    if (strlen(names[index]) == length &&
        strncmp(parser->input + begin, names[index], length) == 0) {
      *value = parser->state->gpr[index];
      return 1;
    }
    written = snprintf(number, sizeof(number), "%u", index);
    if (written >= 0 && (size_t)written == length &&
        strncmp(parser->input + begin, number, length) == 0) {
      *value = parser->state->gpr[index];
      return 1;
    }
    written = snprintf(number, sizeof(number), "x%u", index);
    if (written >= 0 && (size_t)written == length &&
        strncmp(parser->input + begin, number, length) == 0) {
      *value = parser->state->gpr[index];
      return 1;
    }
  }
  npc_set_error(parser->error, parser->error_size, "unknown register '$%.*s'", (int)length,
                parser->input + begin);
  return 0;
}

static int npc_expr_identifier(NpcExpressionParser *parser, uint32_t *value) {
  size_t begin;

  npc_expr_skip_space(parser);
  begin = parser->position;
  while (parser->position < parser->length &&
         (isalnum((unsigned char)parser->input[parser->position]) ||
          parser->input[parser->position] == '_')) {
    parser->position += 1;
  }
  if (begin == parser->position) {
    npc_set_error(parser->error, parser->error_size,
                  "expected a register name after '$'");
    return 0;
  }
  return npc_expr_register_value(parser, begin, parser->position - begin, value);
}

static int npc_expr_number(NpcExpressionParser *parser, uint32_t *value) {
  size_t begin;
  size_t digits;
  int base;
  char *end;
  unsigned long parsed;

  npc_expr_skip_space(parser);
  begin = parser->position;
  base = 10;
  if (parser->position + 2 <= parser->length && parser->input[parser->position] == '0' &&
      (parser->input[parser->position + 1] == 'x' ||
       parser->input[parser->position + 1] == 'X')) {
    parser->position += 2;
    base = 16;
  }
  digits = parser->position;
  while (parser->position < parser->length &&
         isxdigit((unsigned char)parser->input[parser->position])) {
    parser->position += 1;
  }
  if (digits == parser->position) {
    npc_set_error(parser->error, parser->error_size, "expected a number near '%s'",
                  parser->input + begin);
    return 0;
  }
  errno = 0;
  parsed = strtoul(parser->input + begin, &end, base);
  if (errno == ERANGE || end == parser->input + begin) {
    npc_set_error(parser->error, parser->error_size, "invalid number in expression");
    return 0;
  }
  *value = (uint32_t)parsed;
  return 1;
}

static int npc_expr_factor(NpcExpressionParser *parser, uint32_t *value) {
  uint32_t operand;

  npc_expr_skip_space(parser);
  if (npc_expr_consume(parser, '(')) {
    if (!npc_expr_expression(parser, value)) {
      return 0;
    }
    if (!npc_expr_consume(parser, ')')) {
      npc_set_error(parser->error, parser->error_size, "missing ')' in expression");
      return 0;
    }
    return 1;
  }
  if (npc_expr_consume(parser, '-')) {
    if (!npc_expr_factor(parser, &operand)) {
      return 0;
    }
    *value = 0u - operand;
    return 1;
  }
  if (npc_expr_consume(parser, '*')) {
    if (!npc_expr_factor(parser, &operand)) {
      return 0;
    }
    return npc_memory_read(parser->memory, operand, 4, value, parser->error,
                           parser->error_size);
  }
  if (npc_expr_consume(parser, '$')) {
    return npc_expr_identifier(parser, value);
  }
  return npc_expr_number(parser, value);
}

static int npc_expr_term(NpcExpressionParser *parser, uint32_t *value) {
  uint32_t right;

  if (!npc_expr_factor(parser, value)) {
    return 0;
  }
  for (;;) {
    if (npc_expr_consume(parser, '*')) {
      if (!npc_expr_factor(parser, &right)) {
        return 0;
      }
      *value *= right;
    } else if (npc_expr_consume(parser, '/')) {
      if (!npc_expr_factor(parser, &right)) {
        return 0;
      }
      if (right == 0) {
        npc_set_error(parser->error, parser->error_size, "division by zero");
        return 0;
      }
      *value /= right;
    } else {
      return 1;
    }
  }
}

static int npc_expr_expression(NpcExpressionParser *parser, uint32_t *value) {
  uint32_t right;

  if (!npc_expr_term(parser, value)) {
    return 0;
  }
  for (;;) {
    if (npc_expr_consume(parser, '+')) {
      if (!npc_expr_term(parser, &right)) {
        return 0;
      }
      *value += right;
    } else if (npc_expr_consume(parser, '-')) {
      if (!npc_expr_term(parser, &right)) {
        return 0;
      }
      *value -= right;
    } else {
      return 1;
    }
  }
}

int npc_expr_evaluate(const char *input, const NpcCommit *state, const NpcMemory *memory,
                      uint32_t *value, char *error, size_t error_size) {
  NpcExpressionParser parser;

  memset(&parser, 0, sizeof(parser));
  parser.input = input;
  parser.length = strlen(input);
  parser.state = state;
  parser.memory = memory;
  parser.error = error;
  parser.error_size = error_size;
  if (!npc_expr_expression(&parser, value)) {
    return 0;
  }
  npc_expr_skip_space(&parser);
  if (parser.position != parser.length) {
    npc_set_error(error, error_size, "unexpected token near '%s'", input + parser.position);
    return 0;
  }
  return 1;
}
