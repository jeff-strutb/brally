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

void gpu_wait(void)
{
    wait_dma();
    wait_cmd();
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

/* 1: 640 x 480 interlaced; 0: 320 x 240 */
void gpu_mode(int hires)
{
    GP1 = hires ? 0x08000027 : 0x08000001;
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

/* CPU to VRAM, now (w x h halfwords from p, w * h even) */
void gpu_load(int x, int y, int w, int h, const uint16_t *p)
{
    int n = (w * h + 1) / 2, i;
    const uint32_t *q = (const uint32_t *)p;
    gpu_wait();
    GP0 = 0x01000000;                       /* clear the texture cache */
    GP0 = 0xA0000000;
    GP0 = (uint32_t)y << 16 | (uint32_t)x;
    GP0 = (uint32_t)h << 16 | (uint32_t)w;
    for (i = 0; i < n; i++)
        GP0 = q[i];
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
