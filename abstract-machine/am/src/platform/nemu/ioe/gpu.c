#include <am.h>
#include <nemu.h>

#define SYNC_ADDR (VGACTL_ADDR + 4)

void __am_gpu_init() {
}

void __am_gpu_config(AM_GPU_CONFIG_T *cfg) {
  uint32_t size = inl(VGACTL_ADDR);
  int width = size >> 16;
  int height = size & 0xffff;

  *cfg = (AM_GPU_CONFIG_T) {
    .present = true, .has_accel = false,
    .width = width, .height = height,
    .vmemsz = width * height * sizeof(uint32_t)
  };
}

void __am_gpu_fbdraw(AM_GPU_FBDRAW_T *ctl) {
  uint32_t *pixels = ctl->pixels;
  int screen_w = inl(VGACTL_ADDR) >> 16;

  for (int y = 0; y < ctl->h; y++) {
    for (int x = 0; x < ctl->w; x++) {
      int offset = (ctl->y + y) * screen_w + ctl->x + x;
      outl(FB_ADDR + offset * sizeof(uint32_t), pixels[y * ctl->w + x]);
    }
  }

  if (ctl->sync) {
    outl(SYNC_ADDR, 1);
  }
}

void __am_gpu_status(AM_GPU_STATUS_T *status) {
  status->ready = true;
}
