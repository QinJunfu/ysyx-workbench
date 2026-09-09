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

#include <memory/host.h>
#include <memory/paddr.h>
#include <device/mmio.h>
#include <isa.h>

#if defined(CONFIG_YSYXSOC_MEMORY)
typedef struct {
  paddr_t base;
  size_t size;
  uint8_t *storage;
  const char *name;
  bool writable;
} MemoryRegion;

static MemoryRegion memory_regions[] = {
  { YSYXSOC_SRAM_BASE,  YSYXSOC_SRAM_SIZE,  NULL, "sram",  true  },
  { YSYXSOC_MROM_BASE,  YSYXSOC_MROM_SIZE,  NULL, "mrom",  false },
  { YSYXSOC_FLASH_BASE, YSYXSOC_FLASH_SIZE, NULL, "flash", false },
  { YSYXSOC_PSRAM_BASE, YSYXSOC_PSRAM_SIZE, NULL, "psram", true  },
  { YSYXSOC_SDRAM_BASE, YSYXSOC_SDRAM_SIZE, NULL, "sdram", true  },
};
#elif defined(CONFIG_PMEM_MALLOC)
static uint8_t *pmem = NULL;
#else // CONFIG_PMEM_GARRAY
static uint8_t pmem[CONFIG_MSIZE] PG_ALIGN = {};
#endif

#ifdef CONFIG_YSYXSOC_MEMORY
static MemoryRegion *find_region(paddr_t addr, size_t len) {
  for (size_t i = 0; i < ARRLEN(memory_regions); i++) {
    MemoryRegion *region = &memory_regions[i];
    if (addr >= region->base &&
        (size_t)(addr - region->base) <= region->size &&
        len <= region->size - (size_t)(addr - region->base)) {
      return region;
    }
  }
  return NULL;
}

uint8_t *guest_to_host(paddr_t paddr) {
  MemoryRegion *region = find_region(paddr, 1);
  Assert(region != NULL, "address " FMT_PADDR " is outside ysyxSoC memory", paddr);
  return region->storage + (paddr - region->base);
}

paddr_t host_to_guest(uint8_t *haddr) {
  uintptr_t host = (uintptr_t)haddr;
  for (size_t i = 0; i < ARRLEN(memory_regions); i++) {
    MemoryRegion *region = &memory_regions[i];
    uintptr_t left = (uintptr_t)region->storage;
    if (host >= left && host - left < region->size) {
      return region->base + (host - left);
    }
  }
  panic("host address %p is outside ysyxSoC memory", haddr);
}
#else
uint8_t *guest_to_host(paddr_t paddr) { return pmem + paddr - CONFIG_MBASE; }
paddr_t host_to_guest(uint8_t *haddr) { return haddr - pmem + CONFIG_MBASE; }
#endif

static word_t pmem_read(paddr_t addr, int len) {
  word_t ret = host_read(guest_to_host(addr), len);
  return ret;
}

static void pmem_write(paddr_t addr, int len, word_t data) {
  host_write(guest_to_host(addr), len, data);
}

static void out_of_bound(paddr_t addr, int len) {
#ifdef CONFIG_YSYXSOC_MEMORY
  panic("address = " FMT_PADDR ", len = %d is outside ysyxSoC memory at pc = " FMT_WORD,
        addr, len, cpu.pc);
#else
  panic("address = " FMT_PADDR " is out of bound of pmem [" FMT_PADDR ", " FMT_PADDR "] at pc = " FMT_WORD,
      addr, PMEM_LEFT, PMEM_RIGHT, cpu.pc);
#endif
}

void init_mem() {
#if defined(CONFIG_YSYXSOC_MEMORY)
  for (size_t i = 0; i < ARRLEN(memory_regions); i++) {
    MemoryRegion *region = &memory_regions[i];
    region->storage = malloc(region->size);
    assert(region->storage != NULL);
    IFDEF(CONFIG_MEM_RANDOM, memset(region->storage, rand(), region->size));
    IFNDEF(CONFIG_MEM_RANDOM, memset(region->storage, 0, region->size));
    Log("physical memory %s [" FMT_PADDR ", " FMT_PADDR "]",
        region->name, region->base,
        region->base + (paddr_t)region->size - (paddr_t)1);
  }
#elif defined(CONFIG_PMEM_MALLOC)
  pmem = malloc(CONFIG_MSIZE);
  assert(pmem);
  IFDEF(CONFIG_MEM_RANDOM, memset(pmem, rand(), CONFIG_MSIZE));
  Log("physical memory area [" FMT_PADDR ", " FMT_PADDR "]", PMEM_LEFT, PMEM_RIGHT);
#else
  IFDEF(CONFIG_MEM_RANDOM, memset(pmem, rand(), CONFIG_MSIZE));
  Log("physical memory area [" FMT_PADDR ", " FMT_PADDR "]", PMEM_LEFT, PMEM_RIGHT);
#endif
}

static word_t paddr_read_impl(paddr_t addr, int len, bool trace_mtrace) {
  word_t ret;

  if (likely(in_pmem_range(addr, len))) {
    ret = pmem_read(addr, len);
    goto done;
  }
  else {
    IFDEF(CONFIG_DEVICE, ret = mmio_read(addr, len); goto done;)
    out_of_bound(addr, len);
    return 0;
  }

done:
  if (trace_mtrace) {
    IFDEF(CONFIG_MTRACE, trace_write("MTRACE R " FMT_PADDR " len = %d, data = " FMT_WORD "\n", addr, len, ret));
  }
  return ret;
}

word_t paddr_read(paddr_t addr, int len) {
  return paddr_read_impl(addr, len, true);
}

word_t paddr_ifetch(paddr_t addr, int len) {
  return paddr_read_impl(addr, len, false);
}

void paddr_write(paddr_t addr, int len, word_t data) {
  if (likely(in_pmem_range(addr, len))) {
#ifdef CONFIG_YSYXSOC_MEMORY
    MemoryRegion *region = find_region(addr, len);
    if (!region->writable) {
      panic("write to read-only ysyxSoC %s at " FMT_PADDR " (len = %d)",
            region->name, addr, len);
    }
#endif
    pmem_write(addr, len, data);
    goto done;
  }
  IFDEF(CONFIG_DEVICE, mmio_write(addr, len, data); goto done;)
  out_of_bound(addr, len);
  return;

done:
  IFDEF(CONFIG_MTRACE, trace_write("MTRACE W " FMT_PADDR " len = %d, data = " FMT_WORD "\n", addr, len, data));
}
