#include "dpi.h"

#include <stdint.h>

static NpcSimulator *npc_dpi_simulator;



void npc_dpi_set_simulator(NpcSimulator *simulator) {
  npc_dpi_simulator = simulator;
}

int pmem_read(int raddr) {
  if (npc_dpi_simulator == NULL) {
    return 0;
  }
  return (int)npc_simulator_dpi_read(npc_dpi_simulator, (uint32_t)raddr);
}

void pmem_write(int waddr, int wdata, char wmask) {
  if (npc_dpi_simulator != NULL) {
    npc_simulator_dpi_write(npc_dpi_simulator, (uint32_t)waddr, (uint32_t)wdata,
                            (uint8_t)wmask);
  }
}

void npc_commit(int pc, int inst, int dnpc, int gpr0, int gpr1, int gpr2, int gpr3,
                int gpr4, int gpr5, int gpr6, int gpr7, int gpr8, int gpr9,
                int gpr10, int gpr11, int gpr12, int gpr13, int gpr14, int gpr15,
                int mem_valid, int mem_write, int mem_addr, int mem_data, int mem_mask,
                int halt, int halt_code, int invalid) {
  NpcCommit commit;

  if (npc_dpi_simulator == NULL) {
    return;
  }
  commit.pc = (uint32_t)pc;
  commit.inst = (uint32_t)inst;
  commit.dnpc = (uint32_t)dnpc;
  commit.gpr[0] = (uint32_t)gpr0;
  commit.gpr[1] = (uint32_t)gpr1;
  commit.gpr[2] = (uint32_t)gpr2;
  commit.gpr[3] = (uint32_t)gpr3;
  commit.gpr[4] = (uint32_t)gpr4;
  commit.gpr[5] = (uint32_t)gpr5;
  commit.gpr[6] = (uint32_t)gpr6;
  commit.gpr[7] = (uint32_t)gpr7;
  commit.gpr[8] = (uint32_t)gpr8;
  commit.gpr[9] = (uint32_t)gpr9;
  commit.gpr[10] = (uint32_t)gpr10;
  commit.gpr[11] = (uint32_t)gpr11;
  commit.gpr[12] = (uint32_t)gpr12;
  commit.gpr[13] = (uint32_t)gpr13;
  commit.gpr[14] = (uint32_t)gpr14;
  commit.gpr[15] = (uint32_t)gpr15;
  commit.mem_valid = mem_valid != 0;
  commit.mem_write = mem_write != 0;
  commit.mem_addr = (uint32_t)mem_addr;
  commit.mem_data = (uint32_t)mem_data;
  commit.mem_mask = (uint8_t)mem_mask;
  commit.halt = halt != 0;
  commit.halt_code = (uint32_t)halt_code;
  commit.invalid = invalid != 0;
  npc_simulator_dpi_commit(npc_dpi_simulator, &commit);
}
