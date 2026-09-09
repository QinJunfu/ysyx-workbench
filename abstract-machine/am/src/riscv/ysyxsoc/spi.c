#include <am.h>
#include <riscv/riscv.h>
#include <ysyxsoc.h>

#ifndef YSYXSOC_SPI_CLOCK_DIVIDER
#define YSYXSOC_SPI_CLOCK_DIVIDER 1
#endif

static uint32_t bswap32(uint32_t value) {
  return ((value & UINT32_C(0x000000ff)) << 24) |
         ((value & UINT32_C(0x0000ff00)) << 8) |
         ((value & UINT32_C(0x00ff0000)) >> 8) |
         ((value & UINT32_C(0xff000000)) >> 24);
}

void ysyxsoc_spi_transfer(uint32_t slave, unsigned int bits,
                          const uint32_t tx[4], uint32_t rx[4]) {
  uint32_t control;

  if (bits == 0 || bits > 128 || tx == NULL) halt(1);

  outl(YSYXSOC_SPI_TXRX0, tx[0]);
  outl(YSYXSOC_SPI_TXRX1, tx[1]);
  outl(YSYXSOC_SPI_TXRX2, tx[2]);
  outl(YSYXSOC_SPI_TXRX3, tx[3]);
  outl(YSYXSOC_SPI_DIVIDER, YSYXSOC_SPI_CLOCK_DIVIDER);
  outl(YSYXSOC_SPI_SS, slave);

  control = YSYXSOC_SPI_CTRL_ASS | YSYXSOC_SPI_CTRL_TX_NEG |
            (bits == 128 ? 0 : bits);
  outl(YSYXSOC_SPI_CTRL, control);
  outl(YSYXSOC_SPI_CTRL, control | YSYXSOC_SPI_CTRL_GO);
  while ((inl(YSYXSOC_SPI_CTRL) & YSYXSOC_SPI_CTRL_GO) != 0) {}

  if (rx != NULL) {
    rx[0] = inl(YSYXSOC_SPI_TXRX0);
    rx[1] = inl(YSYXSOC_SPI_TXRX1);
    rx[2] = inl(YSYXSOC_SPI_TXRX2);
    rx[3] = inl(YSYXSOC_SPI_TXRX3);
  }
}

uint32_t ysyxsoc_flash_read(uint32_t addr) {
  uint32_t offset = addr;
  uint32_t tx[4] = {0, 0, 0, 0};
  uint32_t rx[4];

  if (addr >= YSYXSOC_FLASH_BASE &&
      addr - YSYXSOC_FLASH_BASE < YSYXSOC_FLASH_SIZE) {
    offset = addr - YSYXSOC_FLASH_BASE;
  }
  if (offset >= YSYXSOC_FLASH_SIZE || (offset & 3) != 0) halt(1);

  /* 03h + 24-bit byte address + 32 clocks for one returned word. */
  tx[1] = (UINT32_C(0x03) << 24) | (offset & UINT32_C(0x00ffffff));
  ysyxsoc_spi_transfer(YSYXSOC_SPI_SS_FLASH, 64, tx, rx);
  return bswap32(rx[0]);
}

uint32_t flash_read(uint32_t addr) {
  return ysyxsoc_flash_read(addr);
}
