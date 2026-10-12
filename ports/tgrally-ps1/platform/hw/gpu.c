/* gpu.c: the PlayStation's GPU.  Two 320x240 frame buffers in VRAM, one
 * shown while the other is drawn; drawing is a linked list of GP0 packets
 * sent by DMA channel 2; VRAM reads for frame dumps. */
#include <stdint.h>
#include <string.h>
#include "ps1.h"
#include "gpu.h"

#define GP0      PS1_IO(0x1810)
#define GP1      PS1_IO(0x1814)
#define GPUSTAT  PS1_IO(0x1814)
#define GPUREAD  PS1_IO(0x1810)
#define D2_MADR  PS1_IO(0x10A0)
#define D2_BCR   PS1_IO(0x10A4)
#define D2_CHCR  PS1_IO(0x10A8)
#define DPCR     PS1_IO(0x10F0)

static void wait_cmd(void) { while (!(GPUSTAT & (1u << 26))) ; }    /* ready for a command */
static void wait_dma(void) { while (D2_CHCR & (1u << 24)) ; }

uint64_t ps1_time_ns(void);
uint64_t gpu_wait_ns;                           /* the CPU's time waiting for the GPU (TGR_PERF) */

void gpu_wait(void)
{
    if ((D2_CHCR & (1u << 24)) || !(GPUSTAT & (1u << 26))) {
        uint64_t t = ps1_time_ns();
        wait_dma();
        wait_cmd();
        gpu_wait_ns += ps1_time_ns() - t;
    }
}

void gpu_init(void)
{
    DPCR |= 0x800;                          /* DMA channel 2 on */
    GP1 = 0x00000000;                       /* reset */
    GP1 = 0x08000001;                       /* 320x240, NTSC, 15-bit, not interlaced */
    GP1 = 0x06000000 | (0x260 + 320 * 8) << 12 | 0x260;   /* horizontal display range */
    GP1 = 0x07000000 | (0x10 + 240) << 10 | 0x10;          /* vertical display range */
    GP1 = 0x05000000;                       /* display from (0, 0) */
    GP1 = 0x04000002;                       /* DMA: CPU to GP0 */
    gpu_fill(0, 0, 1024, 512, 0);
    gpu_wait();
    GP1 = 0x03000000;                       /* display on */
}

/* 1: 640 x 240; 0: 320 x 240 */
void gpu_mode(int hires)
{
    GP1 = hires ? 0x08000003 : 0x08000001;
}

void gpu_display(int on)
{
    GP1 = on ? 0x03000000 : 0x03000001;
}

void gpu_show(int x, int y)
{
    GP1 = 0x05000000 | (uint32_t)y << 10 | (uint32_t)x;
}

/* the drawing area, its offset and the draw mode, as GP0 words in the list */
int gpu_env(uint32_t *p, int x, int y, int w, int h)
{
    p[0] = 0xE1000000 | 0x200 | 0x400;      /* dither, draw to the shown area too */
    p[1] = 0xE3000000 | (uint32_t)y << 10 | (uint32_t)x;
    p[2] = 0xE4000000 | (uint32_t)(y + h - 1) << 10 | (uint32_t)(x + w - 1);
    p[3] = 0xE5000000 | (uint32_t)y << 11 | (uint32_t)x;
    p[4] = 0xE6000000;
    return 5;
}

/* send a linked list (each packet: n << 24 | next, n words; 0xFFFFFF ends it) */
void gpu_send_list(const uint32_t *first)
{
    wait_dma();
    wait_cmd();
    GP1 = 0x04000002;
    D2_MADR = (uint32_t)first & 0xFFFFFF;
    D2_BCR = 0;
    D2_CHCR = 0x01000401;
}

int gpu_busy(void) { return (D2_CHCR & (1u << 24)) != 0 || !(GPUSTAT & (1u << 26)); }

void gpu_fill(int x, int y, int w, int h, uint32_t rgb)
{
    gpu_wait();
    GP0 = 0x02000000 | (rgb & 0xFFFFFF);
    GP0 = (uint32_t)y << 16 | (uint32_t)x;
    GP0 = (uint32_t)h << 16 | (uint32_t)((w + 15) & ~15);
}

/* CPU to VRAM, now (w x h halfwords from p; an odd count is padded) */
void gpu_load(int x, int y, int w, int h, const uint16_t *p)
{
    int n = w * h, i;
    gpu_wait();
    GP0 = 0x01000000;                       /* clear the texture cache */
    GP0 = 0xA0000000;
    GP0 = (uint32_t)y << 16 | (uint32_t)x;
    GP0 = (uint32_t)h << 16 | (uint32_t)w;
    if (((uint32_t)p & 3) == 0)
        for (i = 0; i + 1 < n; i += 2)
            GP0 = *(const uint32_t *)(p + i);
    else
        for (i = 0; i + 1 < n; i += 2)
            GP0 = p[i] | (uint32_t)p[i + 1] << 16;
    if (n & 1)
        GP0 = p[n - 1];
    wait_cmd();
}

/* VRAM to CPU (w * h even) */
void gpu_read(int x, int y, int w, int h, uint16_t *p)
{
    int n = (w * h + 1) / 2, i;
    uint32_t *q = (uint32_t *)p;
    gpu_wait();
    GP0 = 0x01000000;
    GP0 = 0xC0000000;
    GP0 = (uint32_t)y << 16 | (uint32_t)x;
    GP0 = (uint32_t)h << 16 | (uint32_t)w;
    while (!(GPUSTAT & (1u << 27)))         /* ready to send VRAM */
        ;
    for (i = 0; i < n; i++)
        q[i] = GPUREAD;
}
