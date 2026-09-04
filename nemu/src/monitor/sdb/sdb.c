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
#include <cpu/cpu.h>
#include <memory/paddr.h>
#include <ctype.h>
#include <errno.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "sdb.h"
#include "utils.h"

static int is_batch_mode = false;

void init_regex();
void init_wp_pool();

static char *next_arg(char **args) {
  char *start = *args;
  while (start != NULL && isspace((unsigned char)*start)) {
    start ++;
  }

  if (start == NULL || *start == '\0') {
    *args = start;
    return NULL;
  }

  char *end = start;
  while (*end != '\0' && !isspace((unsigned char)*end)) {
    end ++;
  }
  if (*end != '\0') {
    *end ++ = '\0';
  }
  *args = end;
  return start;
}

static bool parse_uint64(const char *str, int base, uint64_t *value) {
  char *end = NULL;

  if (str == NULL || *str == '\0' || *str == '-') {
    return false;
  }

  errno = 0;
  unsigned long long parsed = strtoull(str, &end, base);
  if (errno == ERANGE || end == str || *end != '\0') {
    return false;
  }

  *value = parsed;
  return true;
}

/* We use the `readline' library to provide more flexibility to read from stdin. */
static char* rl_gets() {
  static char *line_read = NULL;

  if (line_read) {
    free(line_read);
    line_read = NULL;
  }

  line_read = readline("(nemu) ");

  if (line_read && *line_read) {
    add_history(line_read);
  }

  return line_read;
}

static int cmd_c(char *args) {
  cpu_exec(-1);
  return 0;
}

static int cmd_si(char *args) {
  uint64_t n = 1;
  char *n_str = next_arg(&args);

  if (n_str != NULL && (!parse_uint64(n_str, 10, &n) || n == 0 || next_arg(&args) != NULL)) {
    printf("Usage: si [N], where N is a positive decimal integer\n");
    return 0;
  }

  cpu_exec(n);
  return 0;
}

static int cmd_info(char *args) {
  char *subcmd = next_arg(&args);
  if (subcmd == NULL || next_arg(&args) != NULL) {
    printf("Usage: info r\n");
    return 0;
  }

  if (strcmp(subcmd, "r") == 0) {
    isa_reg_display();
  }
  else {
    printf("Unknown info subcommand '%s'\n", subcmd);
  }
  return 0;
}

static int cmd_p(char *args) {
  while (args != NULL && isspace((unsigned char)*args)) {
    args ++;
  }
  if (args == NULL || *args == '\0') {
    printf("Usage: p EXPR\n");
    return 0;
  }

  bool success;
  word_t result = expr(args, &success);
  if (!success) {
    printf("Bad expression\n");
    return 0;
  }

  printf(FMT_WORD "\n", result);
  return 0;
}

static int cmd_x(char *args) {
  char *n_str = next_arg(&args);
  char *addr_str = next_arg(&args);
  uint64_t n;
  uint64_t addr_value;

  if (n_str == NULL || addr_str == NULL || next_arg(&args) != NULL ||
      !parse_uint64(n_str, 10, &n) || n == 0 ||
      !parse_uint64(addr_str, 16, &addr_value) ||
      (uint64_t)(paddr_t)addr_value != addr_value) {
    printf("Usage: x N ADDR, where ADDR is a hexadecimal address\n");
    return 0;
  }

  paddr_t addr = (paddr_t)addr_value;
  if (!in_pmem(addr) || PMEM_RIGHT - addr < 3) {
    printf("Address " FMT_PADDR " is outside readable physical memory\n", addr);
    return 0;
  }

  uint64_t max_words = (uint64_t)(PMEM_RIGHT - addr) / 4 + 1;
  if (n > max_words) {
    printf("Requested range exceeds physical memory\n");
    return 0;
  }

  for (uint64_t i = 0; i < n; i ++) {
    printf(FMT_PADDR ": " FMT_WORD "\n", addr, paddr_read(addr, 4));
    addr += 4;
  }

  return 0;
}

static int cmd_q(char *args) {
  nemu_state.state = NEMU_QUIT;
  return -1;
}

static int cmd_help(char *args);

static struct {
  const char *name;
  const char *description;
  int (*handler) (char *);
} cmd_table [] = {
  { "help", "Display information about all supported commands", cmd_help },
  { "c", "Continue the execution of the program", cmd_c },
  { "si", "Step through N instructions (default: 1)", cmd_si },
  { "info", "Display program status (info r)", cmd_info },
  { "p", "Evaluate an arithmetic expression", cmd_p },
  { "x", "Examine N 4-byte words at a hexadecimal address", cmd_x },
  { "q", "Exit NEMU", cmd_q },
};

#define NR_CMD ARRLEN(cmd_table)

static int cmd_help(char *args) {
  char *arg = next_arg(&args);
  int i;

  if (arg == NULL) {
    /* no argument given */
    for (i = 0; i < NR_CMD; i ++) {
      printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
    }
  }
  else {
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(arg, cmd_table[i].name) == 0) {
        printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
        return 0;
      }
    }
    printf("Unknown command '%s'\n", arg);
  }
  return 0;
}

void sdb_set_batch_mode() {
  is_batch_mode = true;
}

void sdb_mainloop() {
  if (is_batch_mode) {
    cmd_c(NULL);
    return;
  }

  for (char *str; (str = rl_gets()) != NULL; ) {
    char *args = str;
    char *cmd = next_arg(&args);
    if (cmd == NULL) { continue; }

#ifdef CONFIG_DEVICE
    extern void sdl_clear_event_queue();
    sdl_clear_event_queue();
#endif

    int i;
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(cmd, cmd_table[i].name) == 0) {
        if (cmd_table[i].handler(args) < 0) { return; }
        break;
      }
    }

    if (i == NR_CMD) { printf("Unknown command '%s'\n", cmd); }
  }
}

void init_sdb() {
  /* Compile the regular expressions. */
  init_regex();

  /* Initialize the watchpoint pool. */
  init_wp_pool();
}
