#ifndef NPC_DEVICE_H
#define NPC_DEVICE_H

#include <stdint.h>
#include <sys/time.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NPC_UART_ADDR 0x10000000u
#define NPC_TIMER_LO_ADDR 0x20000000u
#define NPC_TIMER_HI_ADDR 0x20000004u

typedef struct {
  struct timeval start_time;
  uint64_t timer_latch_us;
} NpcDevices;

void npc_devices_init(NpcDevices *devices);
int npc_devices_read(NpcDevices *devices, uint32_t address, uint32_t *value);
int npc_devices_write(NpcDevices *devices, uint32_t address, uint32_t value, uint8_t mask);
int npc_devices_accesses_mmio(uint32_t address, uint8_t mask);

#ifdef __cplusplus
}
#endif

#endif
