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
#include <memory/paddr.h>
#include "../local-include/csr.h"

static bool wrote_mcycle = false;
static bool wrote_minstret = false;

void riscv_csr_reset(void) {
  cpu.mstatus = 0;
  cpu.mtvec = RESET_VECTOR;
  cpu.mscratch = 0;
  cpu.mepc = 0;
  cpu.mcause = 0;
  cpu.mip = 0;
  cpu.mcycle = 0;
  cpu.minstret = 0;
  wrote_mcycle = false;
  wrote_minstret = false;
}

bool riscv_csr_read(word_t addr, word_t *value) {
  switch (addr) {
    case RISCV_CSR_MSTATUS: *value = cpu.mstatus; return true;
    case RISCV_CSR_MISA: *value = UINT32_C(0x40000010); return true; // RV32E
    case RISCV_CSR_MTVEC: *value = cpu.mtvec; return true;
    case RISCV_CSR_MSCRATCH: *value = cpu.mscratch; return true;
    case RISCV_CSR_MEPC: *value = cpu.mepc; return true;
    case RISCV_CSR_MCAUSE: *value = cpu.mcause; return true;
    case RISCV_CSR_MIP: *value = cpu.mip; return true;
    case RISCV_CSR_MCYCLE: *value = (word_t)cpu.mcycle; return true;
    case RISCV_CSR_MINSTRET: *value = (word_t)cpu.minstret; return true;
    case RISCV_CSR_MHARTID: *value = 0; return true;
    default: return false;
  }
}

bool riscv_csr_write(word_t addr, word_t value) {
  switch (addr) {
    case RISCV_CSR_MSTATUS: cpu.mstatus = value; return true;
    case RISCV_CSR_MTVEC: cpu.mtvec = value; return true;
    case RISCV_CSR_MSCRATCH: cpu.mscratch = value; return true;
    case RISCV_CSR_MEPC: cpu.mepc = value; return true;
    case RISCV_CSR_MCAUSE: cpu.mcause = value; return true;
    case RISCV_CSR_MIP: cpu.mip = value; return true;
    case RISCV_CSR_MCYCLE:
      cpu.mcycle = (cpu.mcycle & UINT64_C(0xffffffff00000000)) | (uint32_t)value;
      wrote_mcycle = true;
      return true;
    case RISCV_CSR_MINSTRET:
      cpu.minstret = (cpu.minstret & UINT64_C(0xffffffff00000000)) | (uint32_t)value;
      wrote_minstret = true;
      return true;
    // NPC currently treats writes to these read-only CSRs as no-ops.
    case RISCV_CSR_MISA:
    case RISCV_CSR_MHARTID:
      return true;
    default:
      return false;
  }
}

void riscv_csr_begin_inst(void) {
  wrote_mcycle = false;
  wrote_minstret = false;
}

void riscv_csr_finish_inst(void) {
  if (!wrote_mcycle) cpu.mcycle ++;
  if (!wrote_minstret) cpu.minstret ++;
}

word_t isa_raise_intr(word_t NO, vaddr_t epc) {
  cpu.mepc = epc;
  cpu.mcause = NO;
  return cpu.mtvec;
}

word_t isa_query_intr() {
  return INTR_EMPTY;
}
