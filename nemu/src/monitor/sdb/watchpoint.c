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

#include "sdb.h"
#include <stdlib.h>
#include <string.h>

#define NR_WP 32

static WP wp_pool[NR_WP] = {};
static WP *head = NULL, *free_ = NULL;

void init_wp_pool() {
  int i;
  for (i = 0; i < NR_WP; i ++) {
    wp_pool[i].NO = i;
    wp_pool[i].next = (i == NR_WP - 1 ? NULL : &wp_pool[i + 1]);
  }

  head = NULL;
  free_ = wp_pool;
}

WP *new_wp(const char *expression, word_t value) {
  if (free_ == NULL) {
    printf("No free watchpoint available\n");
    return NULL;
  }
  WP *wp = free_;
  free_ = wp->next;
  wp->expr = strdup(expression);
  if (wp->expr == NULL) { wp->next = free_; free_ = wp; return NULL; }
  wp->value = value;
  wp->next = head;
  head = wp;
  return wp;
}

bool free_wp(int no) {
  WP **pp = &head;
  while (*pp != NULL && (*pp)->NO != no) pp = &(*pp)->next;
  if (*pp == NULL) return false;
  WP *wp = *pp;
  *pp = wp->next;
  free(wp->expr);
  wp->expr = NULL;
  wp->next = free_;
  free_ = wp;
  return true;
}

WP *find_wp(int no) {
  for (WP *wp = head; wp != NULL; wp = wp->next) if (wp->NO == no) return wp;
  return NULL;
}

void print_watchpoints(void) {
  if (head == NULL) { printf("No watchpoints\n"); return; }
  for (WP *wp = head; wp != NULL; wp = wp->next)
    printf("%d: %s = " FMT_WORD "\n", wp->NO, wp->expr, wp->value);
}

bool check_watchpoints(void) {
#ifndef CONFIG_WATCHPOINT
  return false;
#else
  bool stopped = false;
  for (WP *wp = head; wp != NULL; wp = wp->next) {
    bool success = false;
    word_t value = expr(wp->expr, &success);
    if (!success) continue;
    if (value != wp->value) {
      printf("Watchpoint %d triggered: %s\nold value = " FMT_WORD ", new value = " FMT_WORD "\n",
             wp->NO, wp->expr, wp->value, value);
      wp->value = value;
      stopped = true;
    }
  }
  return stopped;
#endif
}
