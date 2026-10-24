/* io.c: the rest of what the game asks of the machine (tgr_os.h): loads
 * from the cartridge's data, the picture shown, the drawing and sound tasks,
 * printing, and libm's sine table. */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "plat.h"
#include "tgr_core.h"
#include "../render/rdr.h"

int32_t  tgr_tv_type = 1;              /* NTSC */
uint32_t tgr_mem_size = 0x400000;      /* 4 MB, no Expansion Pak */
uint64_t tgr_clock_rate = 62500000;
uint32_t tgr_rom_base = 0xB0000000;
int32_t  tgr_reset_type = 0;

/* how the picture is shown: what a change of resolution sets (anti-aliased,
 * divot filter, gamma, no dither filter), then the game's own settings */
static RdrVideo s_video = { 1, 0, 1, 1 };

void tgr_video_reset(void)
{
    s_video.antialias = 1;
    s_video.dither_filter = 0;
    s_video.divot = 1;
    s_video.gamma = 1;
}

void tgr_video_dither_filter(int on) { s_video.dither_filter = on != 0; }
void tgr_video_divot(int on)         { s_video.divot = on != 0; }
void tgr_video_gamma(int on)         { s_video.gamma = on != 0; }
const RdrVideo *tgr_video_state(void) { return &s_video; }

/* ---- the VI ----------------------------------------------------------------- */
static uint32_t s_framebuffer;

void tgr_video_show(void *fb)
{
    s_framebuffer = tgr_addr32(fb);
    tgr_trace("swap", "%08X", s_framebuffer);
    tgr_gfx_swap(s_framebuffer);
}

void *tgr_video_shown(void)
{
    return TGR_PTR(void *, s_framebuffer);
}

/* ---- the PI: the cartridge -------------------------------------------------- */
void tgr_rom_read(uint32_t off, void *dst, uint32_t n)
{
    uint64_t lo = TGR_ROMDATA_BASE, hi = lo + (uint64_t)(tgr_romdata_end - tgr_romdata), a = off, b = a + n;
    memset(dst, 0, n);
    if (a < lo)
        a = lo;
    if (b > hi)
        b = hi;
    if (a < b)
        memcpy((uint8_t *)dst + (a - off), tgr_romdata + (a - lo), (size_t)(b - a));
}

int32_t tgr_rom_load(TgrIoMsg *mb, int32_t pri, int32_t direction, uint32_t devAddr, void *vAddr,
                     uint32_t nbytes, TgrMsgQueue *mq)
{
    uint32_t src = devAddr & 0x0FFFFFFF;
    (void)pri;
    if (direction != TGR_READ) {
        fprintf(stderr, "tgr: a DMA to the cartridge\n");
        abort();
    }
    tgr_rom_read(src, vAddr, nbytes);
    if (mb)
        mb->hdr.retQueue = mq;
    tgr_post_mesg(mq, (TgrMsg)mb);
    return 0;
}

int32_t tgr_rom_word(uint32_t devAddr, uint32_t *data)
{
    uint32_t off = devAddr & 0x0FFFFFFF;
    uint8_t b[4];
    tgr_rom_read(off, b, 4);
    *data = (uint32_t)b[0] << 24 | (uint32_t)b[1] << 16 | (uint32_t)b[2] << 8 | b[3];
    return 0;
}

/* ---- the RCP: tasks --------------------------------------------------------- */
void tgr_task_load(TgrTask *task)
{
    if (task->t.type == TGR_TASK_GFX) {
        tgr_gfx_task(task->t.data_ptr);
        tgr_post_event(TGR_EVENT_SP);
        tgr_post_event(TGR_EVENT_DP);
    } else {
        tgr_post_event(TGR_EVENT_SP);
    }
}

void tgr_task_run(TgrTask *task) { (void)task; }

/* ---- printing ---------------------------------------------------------------- */
void tgr_print(const char *fmt, ...)
{
    va_list ap;
    if (!getenv("TGR_LOG"))
        return;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}

/* ---- libm: sins / coss through libultra's table (in the ROM's data) ---------- */
int16_t sins(uint16_t x)
{
    uint16_t v = (x >> 4) & 0xFFFF;
    int16_t r;
    if (v & 0x400)
        r = (int16_t)tgr_rd16(TGR_PTR(uint8_t *, 0x802A540Eu - (v & 0x3FF) * 2));
    else
        r = (int16_t)tgr_rd16(TGR_PTR(uint8_t *, 0x802A4C10u + (v & 0x3FF) * 2));
    if (v & 0x800)
        return (int16_t)-r;
    return r;
}

int16_t coss(uint16_t x)
{
    return sins((uint16_t)(x + 0x4000));
}
