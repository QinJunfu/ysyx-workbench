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
* See the Mulan PSL v2 for more details.
***************************************************************************************/

#ifndef __RISCV_CSR_H__
#define __RISCV_CSR_H__

#include <common.h>

enum {
  RISCV_CSR_MSTATUS = 0x300,
  RISCV_CSR_MISA = 0x301,
  RISCV_CSR_MTVEC = 0x305,
  RISCV_CSR_MSCRATCH = 0x340,
  RISCV_CSR_MEPC = 0x341,
  RISCV_CSR_MCAUSE = 0x342,
  RISCV_CSR_MIP = 0x344,
  RISCV_CSR_MCYCLE = 0xb00,
  RISCV_CSR_MINSTRET = 0xb02,
  RISCV_CSR_MHARTID = 0xf14,
};

void riscv_csr_reset(void);
bool riscv_csr_read(word_t addr, word_t *value);
bool riscv_csr_write(word_t addr, word_t value);
void riscv_csr_begin_inst(void);
void riscv_csr_finish_inst(void);

#endif
