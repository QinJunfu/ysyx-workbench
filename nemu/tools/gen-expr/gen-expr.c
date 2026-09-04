/***************************************************************************************
* Copyright (c) 2014-2024 Zihao Yu, Nanjing University
*
* NEMU is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*          http://license.coscl.org.cn/MulanPSL2
*
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
*
* See the Mulan PSL v2 for more details.
***************************************************************************************/

#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>

enum {
  MAX_EXPR_LEN = 4095,
  MAX_ORACLE_EXPR_LEN = MAX_EXPR_LEN * 2,
  MAX_EXPR_DEPTH = 10,
  MAX_FAILED_ATTEMPTS = 100,
};

static char buf[MAX_EXPR_LEN + 1];
static char oracle_buf[MAX_ORACLE_EXPR_LEN + 1];
static char code_buf[MAX_ORACLE_EXPR_LEN + 512];
static size_t buf_len;
static size_t oracle_len;

static const char code_format[] =
"#include <inttypes.h>\n"
"#include <limits.h>\n"
"#include <stdint.h>\n"
"#include <stdio.h>\n"
"_Static_assert(UINT_MAX == UINT32_MAX, \"unsigned must be 32 bits\");\n"
"int main(void) {\n"
"  uint32_t result = (uint32_t)(%s);\n"
"  printf(\"%%\" PRIu32, result);\n"
"  return 0;\n"
"}\n";

static uint32_t choose(uint32_t n) {
  return (uint32_t)rand() % n;
}

static uint32_t rand_u32(void) {
  return ((uint32_t)rand() << 16) ^ (uint32_t)rand();
}

static bool append_text(char *dst, size_t *len, size_t max_len,
                        const char *text) {
  size_t text_len = strlen(text);
  if (text_len > max_len - *len) {
    return false;
  }

  memcpy(dst + *len, text, text_len + 1);
  *len += text_len;
  return true;
}

static bool append_pair(const char *expr_text, const char *oracle_text) {
  size_t saved_buf_len = buf_len;
  size_t saved_oracle_len = oracle_len;

  if (append_text(buf, &buf_len, MAX_EXPR_LEN, expr_text) &&
      append_text(oracle_buf, &oracle_len, MAX_ORACLE_EXPR_LEN, oracle_text)) {
    return true;
  }

  buf_len = saved_buf_len;
  oracle_len = saved_oracle_len;
  buf[buf_len] = '\0';
  oracle_buf[oracle_len] = '\0';
  return false;
}

static bool append_pair_char(char c) {
  char text[2] = {c, '\0'};
  return append_pair(text, text);
}

static bool gen_spaces(void) {
  uint32_t count = choose(3);
  while (count-- > 0) {
    if (!append_pair_char(' ')) {
      return false;
    }
  }
  return true;
}

static bool gen_num(bool nonzero) {
  uint32_t value;
  do {
    value = rand_u32();
  } while (nonzero && value == 0);

  char number[sizeof("4294967295")];
  char oracle_number[sizeof("4294967295u")];
  int number_len = snprintf(number, sizeof(number), "%" PRIu32, value);
  int oracle_number_len = snprintf(oracle_number, sizeof(oracle_number),
                                    "%" PRIu32 "u", value);
  if (number_len < 0 || oracle_number_len < 0 ||
      (size_t)number_len >= sizeof(number) ||
      (size_t)oracle_number_len >= sizeof(oracle_number)) {
    return false;
  }

  return append_pair(number, oracle_number);
}

static bool gen_rand_expr(unsigned depth);

static bool gen_parenthesized_expr(unsigned depth) {
  size_t saved_buf_len = buf_len;
  size_t saved_oracle_len = oracle_len;

  if (gen_spaces() && append_pair_char('(') && gen_spaces() &&
      gen_rand_expr(depth + 1) && gen_spaces() && append_pair_char(')') &&
      gen_spaces()) {
    return true;
  }

  buf_len = saved_buf_len;
  oracle_len = saved_oracle_len;
  buf[buf_len] = '\0';
  oracle_buf[oracle_len] = '\0';
  return false;
}

static bool gen_unary_expr(unsigned depth) {
  size_t saved_buf_len = buf_len;
  size_t saved_oracle_len = oracle_len;

  /* The space prevents consecutive unary minuses from becoming C's -- token. */
  if (append_pair("- ", "- ") && gen_rand_expr(depth + 1)) {
    return true;
  }

  buf_len = saved_buf_len;
  oracle_len = saved_oracle_len;
  buf[buf_len] = '\0';
  oracle_buf[oracle_len] = '\0';
  return false;
}

static bool gen_binary_expr(unsigned depth) {
  static const char operators[] = "+-*/";
  size_t saved_buf_len = buf_len;
  size_t saved_oracle_len = oracle_len;
  char op = operators[choose(sizeof(operators) - 1)];

  if (gen_rand_expr(depth + 1) && gen_spaces() && append_pair_char(op) &&
      gen_spaces() &&
      ((op == '/') ? gen_num(true) : gen_rand_expr(depth + 1))) {
    return true;
  }

  buf_len = saved_buf_len;
  oracle_len = saved_oracle_len;
  buf[buf_len] = '\0';
  oracle_buf[oracle_len] = '\0';
  return false;
}

static bool gen_rand_expr(unsigned depth) {
  if (depth >= MAX_EXPR_DEPTH) {
    return gen_num(false);
  }

  switch (choose(4)) {
    case 0:
      return gen_num(false);
    case 1:
      if (gen_parenthesized_expr(depth)) {
        return true;
      }
      break;
    case 2:
      if (gen_unary_expr(depth)) {
        return true;
      }
      break;
    default:
      if (gen_binary_expr(depth)) {
        return true;
      }
      break;
  }

  return gen_num(false);
}

static bool command_succeeded(int status) {
  return status != -1 && WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

static bool get_oracle_result(uint32_t *result) {
  int code_len = snprintf(code_buf, sizeof(code_buf), code_format, oracle_buf);
  if (code_len < 0 || (size_t)code_len >= sizeof(code_buf)) {
    return false;
  }

  FILE *fp = fopen("/tmp/.code.c", "w");
  if (fp == NULL) {
    return false;
  }

  int write_status = fputs(code_buf, fp);
  int close_status = fclose(fp);
  if (write_status == EOF || close_status != 0) {
    return false;
  }

  int status = system("gcc -std=c11 -Wall -Werror /tmp/.code.c -o /tmp/.expr "
                      ">/dev/null 2>&1");
  if (!command_succeeded(status)) {
    return false;
  }

  fp = popen("/tmp/.expr", "r");
  if (fp == NULL) {
    return false;
  }

  uint32_t value;
  int scanned = fscanf(fp, "%" SCNu32, &value);
  status = pclose(fp);
  if (scanned != 1 || !command_succeeded(status)) {
    return false;
  }

  *result = value;
  return true;
}

static bool parse_loop_count(const char *arg, size_t *loop) {
  char *end = NULL;
  errno = 0;
  unsigned long long value = strtoull(arg, &end, 10);
  if (errno != 0 || end == arg || *end != '\0' || value > SIZE_MAX) {
    return false;
  }

  *loop = (size_t)value;
  return true;
}

int main(int argc, char *argv[]) {
  size_t loop = 1;
  if (argc > 2 || (argc == 2 && !parse_loop_count(argv[1], &loop))) {
    fprintf(stderr, "Usage: %s [number-of-expressions]\n", argv[0]);
    return EXIT_FAILURE;
  }

  srand((unsigned int)time(NULL));

  size_t generated = 0;
  unsigned int failed_attempts = 0;
  while (generated < loop) {
    buf_len = 0;
    oracle_len = 0;
    buf[0] = '\0';
    oracle_buf[0] = '\0';

    if (!gen_rand_expr(0)) {
      failed_attempts++;
    } else {
      uint32_t result;
      if (get_oracle_result(&result)) {
        printf("%" PRIu32 " %s\n", result, buf);
        generated++;
        failed_attempts = 0;
        continue;
      }
      failed_attempts++;
    }

    if (failed_attempts >= MAX_FAILED_ATTEMPTS) {
      fprintf(stderr, "Failed to generate a valid expression after %u attempts\n",
              failed_attempts);
      return EXIT_FAILURE;
    }
  }

  return EXIT_SUCCESS;
}
