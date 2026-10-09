/* gpu.h: the GPU (hw/gpu.c) */
#ifndef PS1_GPU_H
#define PS1_GPU_H
#include <stdint.h>
void gpu_init(void);
void gpu_wait(void);
int  gpu_busy(void);
void gpu_show(int x, int y);
void gpu_mode(int hires);
int  gpu_env(uint32_t *p, int x, int y, int w, int h);
void gpu_send_list(const uint32_t *first);
void gpu_fill(int x, int y, int w, int h, uint32_t rgb);
void gpu_load(int x, int y, int w, int h, const uint16_t *p);
void gpu_read(int x, int y, int w, int h, uint16_t *p);
#endif
