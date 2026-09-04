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

#include <isa.h>
#include <errno.h>
#include <limits.h>

/* We use the POSIX regex functions to process regular expressions.
 * Type 'man regex' for more information about POSIX regex functions.
 */
#include <regex.h>

enum {
  TK_NOTYPE = 256, TK_NUM, TK_EQ, TK_NEG,
};

static struct rule {
  const char *regex;
  int token_type;
} rules[] = {
  {"[ \\t]+", TK_NOTYPE},
  {"[0-9]+", TK_NUM},
  {"\\+", '+'},
  {"-", '-'},
  {"\\*", '*'},
  {"/", '/'},
  {"\\(", '('},
  {"\\)", ')'},
  {"==", TK_EQ},
};

#define NR_REGEX ARRLEN(rules)

static regex_t re[NR_REGEX] = {};

/* Rules are used for many times.
 * Therefore we compile them only once before any usage.
 */
void init_regex() {
  int i;
  char error_msg[128];
  int ret;

  for (i = 0; i < NR_REGEX; i ++) {
    ret = regcomp(&re[i], rules[i].regex, REG_EXTENDED);
    if (ret != 0) {
      regerror(ret, &re[i], error_msg, 128);
      panic("regex compilation failed: %s\n%s", error_msg, rules[i].regex);
    }
  }
}

typedef struct token {
  int type;
  char str[32];
} Token;

#define NR_TOKEN_MAX 4096

static Token tokens[NR_TOKEN_MAX] = {};
static int nr_token = 0;

static bool is_expr_end(int type) {
  return type == TK_NUM || type == ')';
}

static void recognize_unary_minus(void) {
  for (int i = 0; i < nr_token; i ++) {
    if (tokens[i].type == '-' && (i == 0 || !is_expr_end(tokens[i - 1].type))) {
      tokens[i].type = TK_NEG;
    }
  }
}

static bool append_token(int type, const char *str, int len) {
  if (type == TK_NOTYPE) {
    return true;
  }

  if (nr_token >= NR_TOKEN_MAX) {
    printf("Expression has too many tokens\n");
    return false;
  }

  Token *token = &tokens[nr_token];
  token->type = type;
  token->str[0] = '\0';
  if (type == TK_NUM) {
    if (len >= (int)sizeof(token->str)) {
      printf("Token is too long\n");
      return false;
    }
    memcpy(token->str, str, len);
    token->str[len] = '\0';
  }
  nr_token ++;
  return true;
}

static bool make_token(char *e) {
  int position = 0;
  int i;
  regmatch_t pmatch;

  nr_token = 0;

  while (e[position] != '\0') {
    /* Try all rules one by one. */
    for (i = 0; i < NR_REGEX; i ++) {
      if (regexec(&re[i], e + position, 1, &pmatch, 0) == 0 && pmatch.rm_so == 0) {
        char *substr_start = e + position;
        int substr_len = pmatch.rm_eo;

        log_write("match rules[%d] = \"%s\" at position %d with len %d: %.*s\n",
            i, rules[i].regex, position, substr_len, substr_len, substr_start);

        position += substr_len;

        if (!append_token(rules[i].token_type, substr_start, substr_len)) {
          return false;
        }

        break;
      }
    }

    if (i == NR_REGEX) {
      printf("no match at position %d\n%s\n%*.s^\n", position, e, position, "");
      return false;
    }
  }

  recognize_unary_minus();
  return true;
}

static bool check_parentheses(int p, int q, bool *valid) {
  int depth = 0;
  *valid = true;

  for (int i = p; i <= q; i ++) {
    if (tokens[i].type == '(') {
      depth ++;
    }
    else if (tokens[i].type == ')') {
      depth --;
      if (depth < 0) {
        *valid = false;
        return false;
      }
      if (depth == 0 && i < q) {
        return false;
      }
    }
  }

  if (depth != 0) {
    *valid = false;
    return false;
  }
  return tokens[p].type == '(' && tokens[q].type == ')';
}

static int precedence(int type) {
  switch (type) {
    case '+': case '-': return 1;
    case '*': case '/': return 2;
    default: return -1;
  }
}

static int find_main_op(int p, int q, bool *valid) {
  int depth = 0;
  int main_op = -1;
  int lowest_precedence = INT_MAX;
  *valid = true;

  for (int i = p; i <= q; i ++) {
    if (tokens[i].type == '(') {
      depth ++;
      continue;
    }
    if (tokens[i].type == ')') {
      depth --;
      if (depth < 0) {
        *valid = false;
        return -1;
      }
      continue;
    }
    if (depth == 0) {
      int current_precedence = precedence(tokens[i].type);
      if (current_precedence >= 0 && current_precedence <= lowest_precedence) {
        lowest_precedence = current_precedence;
        main_op = i;
      }
    }
  }

  if (depth != 0) {
    *valid = false;
  }
  return main_op;
}

static word_t parse_number(const char *str, bool *success) {
  char *end = NULL;
  errno = 0;
  unsigned long long value = strtoull(str, &end, 10);
  if (errno == ERANGE || end == str || *end != '\0' || value > UINT32_MAX) {
    *success = false;
    return 0;
  }
  return (word_t)value;
}

static word_t eval(int p, int q, bool *success) {
  if (p > q) {
    *success = false;
    return 0;
  }

  if (p == q) {
    if (tokens[p].type != TK_NUM) {
      *success = false;
      return 0;
    }
    return parse_number(tokens[p].str, success);
  }

  bool valid;
  if (check_parentheses(p, q, &valid)) {
    return eval(p + 1, q - 1, success);
  }
  if (!valid) {
    *success = false;
    return 0;
  }

  int op = find_main_op(p, q, &valid);
  if (!valid) {
    *success = false;
    return 0;
  }

  if (op < 0) {
    if (tokens[p].type != TK_NEG) {
      *success = false;
      return 0;
    }

    word_t value = eval(p + 1, q, success);
    return *success ? (word_t)(0 - value) : 0;
  }

  word_t val1 = eval(p, op - 1, success);
  if (!*success) {
    return 0;
  }
  word_t val2 = eval(op + 1, q, success);
  if (!*success) {
    return 0;
  }

  switch (tokens[op].type) {
    case '+': return val1 + val2;
    case '-': return val1 - val2;
    case '*': return val1 * val2;
    case '/':
      if (val2 == 0) {
        *success = false;
        return 0;
      }
      return val1 / val2;
    default:
      *success = false;
      return 0;
  }
}

word_t expr(char *e, bool *success) {
  if (success == NULL) {
    return 0;
  }

  *success = false;
  if (e == NULL || !make_token(e) || nr_token == 0) {
    return 0;
  }

  *success = true;
  return eval(0, nr_token - 1, success);
}
