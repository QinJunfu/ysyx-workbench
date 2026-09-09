#include "device.h"

#include <stdio.h>

#include <generated/autoconf.h>

static uint64_t npc_devices_elapsed_us(const NpcDevices *devices) {
  struct timeval now;
  int64_t seconds;
  int64_t microseconds;

  if (gettimeofday(&now, NULL) != 0) {
    return 0;
  }
  seconds = (int64_t)now.tv_sec - (int64_t)devices->start_time.tv_sec;
  microseconds = (int64_t)now.tv_usec - (int64_t)devices->start_time.tv_usec;
  if (microseconds < 0) {
    seconds -= 1;
    microseconds += 1000000;
  }
  if (seconds < 0) {
    return 0;
  }
  return (uint64_t)seconds * 1000000u + (uint64_t)microseconds;
}

static int npc_devices_is_mmio_byte(uint64_t address) {
#ifdef CONFIG_NPC_PLATFORM_YSYXSOC
  return (address >= UINT64_C(0x02000000) && address <= UINT64_C(0x0200ffff)) ||
         (address >= UINT64_C(0x10000000) && address <= UINT64_C(0x10000fff)) ||
         (address >= UINT64_C(0x10001000) && address <= UINT64_C(0x10001fff)) ||
         (address >= UINT64_C(0x10002000) && address <= UINT64_C(0x1000200f)) ||
         (address >= UINT64_C(0x10011000) && address <= UINT64_C(0x10011007)) ||
         (address >= UINT64_C(0x21000000) && address <= UINT64_C(0x211fffff)) ||
         (address >= UINT64_C(0x40000000) && address <= UINT64_C(0x7fffffff));
#else
  return (address >= NPC_UART_ADDR && address < (uint64_t)NPC_UART_ADDR + 4u) ||
         (address >= NPC_TIMER_LO_ADDR && address < (uint64_t)NPC_TIMER_HI_ADDR + 4u);
#endif
}

void npc_devices_init(NpcDevices *devices) {
  if (devices == NULL) {
    return;
  }
  devices->start_time.tv_sec = 0;
  devices->start_time.tv_usec = 0;
  devices->timer_latch_us = 0;
  (void)gettimeofday(&devices->start_time, NULL);
}

int npc_devices_read(NpcDevices *devices, uint32_t address, uint32_t *value) {
  if (devices == NULL || value == NULL) {
    return 0;
  }
  if (address == NPC_TIMER_HI_ADDR) {
    devices->timer_latch_us = npc_devices_elapsed_us(devices);
    *value = (uint32_t)(devices->timer_latch_us >> 32);
    return 1;
  }
  if (address == NPC_TIMER_LO_ADDR) {
    *value = (uint32_t)devices->timer_latch_us;
    return 1;
  }
  return 0;
}

int npc_devices_write(NpcDevices *devices, uint32_t address, uint32_t value, uint8_t mask) {
  (void)devices;

  if (address != NPC_UART_ADDR) {
    return 0;
  }
  if ((mask & 0x1u) != 0) {
    (void)fputc((int)(value & 0xffu), stderr);
    (void)fflush(stderr);
  }
  return 1;
}

int npc_devices_accesses_mmio(uint32_t address, uint8_t mask) {
  unsigned int index;
  int has_active_byte;

  has_active_byte = 0;
  for (index = 0; index < 4; ++index) {
    if ((mask & (uint8_t)(1u << index)) != 0) {
      if (!npc_devices_is_mmio_byte((uint64_t)address + index)) {
        return 0;
      }
      has_active_byte = 1;
    }
  }
  return has_active_byte;
}
