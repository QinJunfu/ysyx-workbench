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

#include <common.h>
#include <cpu/cpu.h>

extern uint64_t g_nr_guest_inst;

#ifndef CONFIG_TARGET_AM
#include <stdarg.h>

FILE *log_fp = NULL;

void init_log(const char *log_file) {
  log_fp = stdout;
  if (log_file != NULL) {
    FILE *fp = fopen(log_file, "w");
    Assert(fp, "Can not open '%s'", log_file);
    log_fp = fp;
  }
  Log("Log is written to %s", log_file ? log_file : "stdout");
}

bool log_enable() {
  return MUXDEF(CONFIG_TRACE, (g_nr_guest_inst >= CONFIG_TRACE_START) &&
         (g_nr_guest_inst <= CONFIG_TRACE_END), false);
}

void trace_write(const char *fmt, ...) {
  bool write_log = log_enable() && log_fp != NULL;
  bool write_stdout = cpu_exec_print_step();

  if (!write_log && !write_stdout) {
    return;
  }

  va_list ap;
  if (write_log && log_fp != stdout) {
    va_start(ap, fmt);
    vfprintf(log_fp, fmt, ap);
    fflush(log_fp);
    va_end(ap);
  }

  if (write_stdout || (write_log && log_fp == stdout)) {
    va_start(ap, fmt);
    vfprintf(stdout, fmt, ap);
    fflush(stdout);
    va_end(ap);
  }
}
#else
void trace_write(const char *fmt, ...) {
  (void)fmt;
}
#endif
