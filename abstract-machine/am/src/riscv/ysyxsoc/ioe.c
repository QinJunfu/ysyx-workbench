#include <am.h>
#include <klib-macros.h>
#include <riscv/riscv.h>
#include <ysyxsoc.h>

#define YSYXSOC_VGA_WIDTH  640
#define YSYXSOC_VGA_HEIGHT 480
#define KEY_QUEUE_LEN 16

static uint64_t timer_boot;
static bool key_queue_down[KEY_QUEUE_LEN];
static int key_queue_code[KEY_QUEUE_LEN];
static unsigned int key_queue_head;
static unsigned int key_queue_tail;

static uint64_t timer_read(void) {
  uint32_t hi_before;
  uint32_t hi_after;
  uint32_t lo;

  do {
    hi_before = inl(YSYXSOC_CLINT_MTIME + 4);
    lo = inl(YSYXSOC_CLINT_MTIME);
    hi_after = inl(YSYXSOC_CLINT_MTIME + 4);
  } while (hi_before != hi_after);
  return ((uint64_t)hi_after << 32) | lo;
}

static void __am_uart_config(AM_UART_CONFIG_T *cfg) {
  cfg->present = true;
}

static void __am_uart_tx(AM_UART_TX_T *uart) {
  putch(uart->data);
}

static void __am_uart_rx(AM_UART_RX_T *uart) {
  uart->data = (inb(YSYXSOC_UART_LSR) & YSYXSOC_UART_LSR_DR)
                   ? (char)inb(YSYXSOC_UART_RBR)
                   : (char)0xff;
}

static int ps2_keycode(uint8_t code, bool extended) {
  if (extended) {
    switch (code) {
      case 0x75: return AM_KEY_UP;
      case 0x72: return AM_KEY_DOWN;
      case 0x6b: return AM_KEY_LEFT;
      case 0x74: return AM_KEY_RIGHT;
      case 0x70: return AM_KEY_INSERT;
      case 0x71: return AM_KEY_DELETE;
      case 0x6c: return AM_KEY_HOME;
      case 0x69: return AM_KEY_END;
      case 0x7d: return AM_KEY_PAGEUP;
      case 0x7a: return AM_KEY_PAGEDOWN;
      case 0x14: return AM_KEY_RCTRL;
      case 0x11: return AM_KEY_RALT;
      default: return AM_KEY_NONE;
    }
  }

  switch (code) {
    case 0x1c: return AM_KEY_A; case 0x32: return AM_KEY_B;
    case 0x21: return AM_KEY_C; case 0x23: return AM_KEY_D;
    case 0x24: return AM_KEY_E; case 0x2b: return AM_KEY_F;
    case 0x34: return AM_KEY_G; case 0x33: return AM_KEY_H;
    case 0x43: return AM_KEY_I; case 0x3b: return AM_KEY_J;
    case 0x42: return AM_KEY_K; case 0x4b: return AM_KEY_L;
    case 0x3a: return AM_KEY_M; case 0x31: return AM_KEY_N;
    case 0x44: return AM_KEY_O; case 0x4d: return AM_KEY_P;
    case 0x15: return AM_KEY_Q; case 0x2d: return AM_KEY_R;
    case 0x1b: return AM_KEY_S; case 0x2c: return AM_KEY_T;
    case 0x3c: return AM_KEY_U; case 0x2a: return AM_KEY_V;
    case 0x1d: return AM_KEY_W; case 0x22: return AM_KEY_X;
    case 0x35: return AM_KEY_Y; case 0x1a: return AM_KEY_Z;
    case 0x45: return AM_KEY_0; case 0x16: return AM_KEY_1;
    case 0x1e: return AM_KEY_2; case 0x26: return AM_KEY_3;
    case 0x25: return AM_KEY_4; case 0x2e: return AM_KEY_5;
    case 0x36: return AM_KEY_6; case 0x3d: return AM_KEY_7;
    case 0x3e: return AM_KEY_8; case 0x46: return AM_KEY_9;
    case 0x5a: return AM_KEY_RETURN;
    case 0x66: return AM_KEY_BACKSPACE;
    case 0x29: return AM_KEY_SPACE;
    case 0x0d: return AM_KEY_TAB;
    case 0x76: return AM_KEY_ESCAPE;
    case 0x12: return AM_KEY_LSHIFT;
    case 0x59: return AM_KEY_RSHIFT;
    case 0x14: return AM_KEY_LCTRL;
    case 0x11: return AM_KEY_LALT;
    default: return AM_KEY_NONE;
  }
}

static void key_queue_push(bool down, int code) {
  unsigned int next = (key_queue_tail + 1) % KEY_QUEUE_LEN;
  if (code == AM_KEY_NONE || next == key_queue_head) return;
  key_queue_down[key_queue_tail] = down;
  key_queue_code[key_queue_tail] = code;
  key_queue_tail = next;
}

static void ps2_poll(void) {
  static bool extended;
  static bool released;

  while (1) {
    uint32_t data = inl(YSYXSOC_PS2_DATA);
    uint8_t code;
    if ((data & YSYXSOC_PS2_DATA_READY) == 0) break;
    code = data;
    if (code == 0xe0) {
      extended = true;
    } else if (code == 0xf0) {
      released = true;
    } else {
      key_queue_push(!released, ps2_keycode(code, extended));
      extended = false;
      released = false;
    }
  }
}

static void __am_input_config(AM_INPUT_CONFIG_T *cfg) {
  cfg->present = true;
}

static void __am_input_keybrd(AM_INPUT_KEYBRD_T *kbd) {
  ps2_poll();
  if (key_queue_head == key_queue_tail) {
    kbd->keydown = false;
    kbd->keycode = AM_KEY_NONE;
    return;
  }
  kbd->keydown = key_queue_down[key_queue_head];
  kbd->keycode = key_queue_code[key_queue_head];
  key_queue_head = (key_queue_head + 1) % KEY_QUEUE_LEN;
}

static void __am_gpu_config(AM_GPU_CONFIG_T *cfg) {
  *cfg = (AM_GPU_CONFIG_T) {
    .present = true,
    .has_accel = false,
    .width = YSYXSOC_VGA_WIDTH,
    .height = YSYXSOC_VGA_HEIGHT,
    .vmemsz = YSYXSOC_VGA_FB_SIZE,
  };
}

static void __am_gpu_status(AM_GPU_STATUS_T *status) {
  status->ready = true;
}

static void __am_gpu_fbdraw(AM_GPU_FBDRAW_T *ctl) {
  uint32_t *pixels = ctl->pixels;
  volatile uint32_t *fb = (volatile uint32_t *)(uintptr_t)YSYXSOC_VGA_FB_BASE;

  if (pixels == NULL || ctl->w <= 0 || ctl->h <= 0) return;
  for (int y = 0; y < ctl->h; y++) {
    int screen_y = ctl->y + y;
    if (screen_y < 0 || screen_y >= YSYXSOC_VGA_HEIGHT) continue;
    for (int x = 0; x < ctl->w; x++) {
      int screen_x = ctl->x + x;
      if (screen_x >= 0 && screen_x < YSYXSOC_VGA_WIDTH) {
        fb[screen_y * YSYXSOC_VGA_WIDTH + screen_x] = pixels[y * ctl->w + x];
      }
    }
  }
}

static void __am_timer_config(AM_TIMER_CONFIG_T *cfg) {
  cfg->present = true;
  cfg->has_rtc = false;
}

static void __am_timer_rtc(AM_TIMER_RTC_T *rtc) {
  *rtc = (AM_TIMER_RTC_T) { .year = 1900 };
}

static void __am_timer_uptime(AM_TIMER_UPTIME_T *uptime) {
  /* ysyxSoC CLINT advances at 10 MHz. */
  uptime->us = (timer_read() - timer_boot) / 10;
}

typedef void (*handler_t)(void *buf);
static void *lut[128] = {
  [AM_UART_CONFIG] = __am_uart_config,
  [AM_UART_TX] = __am_uart_tx,
  [AM_UART_RX] = __am_uart_rx,
  [AM_TIMER_CONFIG] = __am_timer_config,
  [AM_TIMER_RTC] = __am_timer_rtc,
  [AM_TIMER_UPTIME] = __am_timer_uptime,
  [AM_INPUT_CONFIG] = __am_input_config,
  [AM_INPUT_KEYBRD] = __am_input_keybrd,
  [AM_GPU_CONFIG] = __am_gpu_config,
  [AM_GPU_STATUS] = __am_gpu_status,
  [AM_GPU_FBDRAW] = __am_gpu_fbdraw,
};

static void fail(void *buf) {
  (void)buf;
  panic("access nonexist register");
}

bool ioe_init(void) {
  for (int i = 0; i < LENGTH(lut); i++) {
    if (lut[i] == NULL) lut[i] = fail;
  }
  ysyxsoc_uart_init();
  timer_boot = timer_read();
  return true;
}

void ioe_read(int reg, void *buf) {
  ((handler_t)lut[reg])(buf);
}

void ioe_write(int reg, void *buf) {
  ((handler_t)lut[reg])(buf);
}
