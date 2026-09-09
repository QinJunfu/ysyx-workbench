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

#ifndef __MEMORY_PADDR_H__
#define __MEMORY_PADDR_H__

#include <common.h>

#define PMEM_LEFT  ((paddr_t)CONFIG_MBASE)
#define PMEM_RIGHT ((paddr_t)CONFIG_MBASE + CONFIG_MSIZE - 1)
#define RESET_VECTOR (PMEM_LEFT + CONFIG_PC_RESET_OFFSET)

#ifdef CONFIG_YSYXSOC_MEMORY
#define YSYXSOC_SRAM_BASE   ((paddr_t)0x0f000000u)
#define YSYXSOC_SRAM_SIZE   ((size_t)0x00002000u)
#define YSYXSOC_MROM_BASE   ((paddr_t)0x20000000u)
#define YSYXSOC_MROM_SIZE   ((size_t)0x00001000u)
#define YSYXSOC_FLASH_BASE  ((paddr_t)0x30000000u)
#define YSYXSOC_FLASH_SIZE  ((size_t)0x01000000u)
#define YSYXSOC_PSRAM_BASE  ((paddr_t)0x80000000u)
#define YSYXSOC_PSRAM_SIZE  ((size_t)0x00400000u)
#define YSYXSOC_SDRAM_BASE  ((paddr_t)0xa0000000u)
#if defined(CONFIG_YSYXSOC_SDRAM_32MB)
#define YSYXSOC_SDRAM_SIZE  ((size_t)0x02000000u)
#elif defined(CONFIG_YSYXSOC_SDRAM_64MB)
#define YSYXSOC_SDRAM_SIZE  ((size_t)0x04000000u)
#else
#define YSYXSOC_SDRAM_SIZE  ((size_t)0x08000000u)
#endif
#endif

/* convert the guest physical address in the guest program to host virtual address in NEMU */
uint8_t* guest_to_host(paddr_t paddr);
/* convert the host virtual address in NEMU to guest physical address in the guest program */
paddr_t host_to_guest(uint8_t *haddr);

static inline bool in_pmem_range(paddr_t addr, size_t len) {
#ifdef CONFIG_YSYXSOC_MEMORY
#define IN_YSYXSOC_REGION(base, size) \
  (addr >= (base) && (size_t)(addr - (base)) <= (size) && \
   len <= (size) - (size_t)(addr - (base)))
  return IN_YSYXSOC_REGION(YSYXSOC_SRAM_BASE, YSYXSOC_SRAM_SIZE) ||
         IN_YSYXSOC_REGION(YSYXSOC_MROM_BASE, YSYXSOC_MROM_SIZE) ||
         IN_YSYXSOC_REGION(YSYXSOC_FLASH_BASE, YSYXSOC_FLASH_SIZE) ||
         IN_YSYXSOC_REGION(YSYXSOC_PSRAM_BASE, YSYXSOC_PSRAM_SIZE) ||
         IN_YSYXSOC_REGION(YSYXSOC_SDRAM_BASE, YSYXSOC_SDRAM_SIZE);
#undef IN_YSYXSOC_REGION
#else
  return addr >= PMEM_LEFT && (size_t)(addr - PMEM_LEFT) <= CONFIG_MSIZE &&
         len <= CONFIG_MSIZE - (size_t)(addr - PMEM_LEFT);
#endif
}

static inline bool in_pmem(paddr_t addr) {
  return in_pmem_range(addr, 1);
}

word_t paddr_read(paddr_t addr, int len);
word_t paddr_ifetch(paddr_t addr, int len);
void paddr_write(paddr_t addr, int len, word_t data);

#endif
