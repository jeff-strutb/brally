/* io.c: the rest of libultra's surface the game calls: the PI (ROM DMA), the
 * VI, the RCP's task handshake, the audio interface, the caches, printing,
 * and libm's sine table -- each as tools/tgrally/n64box.py models it. */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "plat.h"
#include "tgr_core.h"
#include "sha1.h"

int32_t  osTvType = 1;              /* NTSC */
uint32_t osMemSize = 0x400000;      /* 4 MB, no Expansion Pak */
uint64_t osClockRate = 62500000;
uint32_t osRomBase = 0xB0000000;
int32_t  osResetType = 0;

/* ---- managers the game starts: nothing to run ------------------------------- */
void osCreateViManager(OSPri pri) { (void)pri; }
void osCreatePiManager(OSPri pri, OSMesgQueue *q, OSMesg *b, int32_t n) { (void)pri; (void)q; (void)b; (void)n; }
/* the VI's control register as libultra keeps it: the mode's, then the special
 * features' changes (the renderer's VI stage reads it, tgr_vi_ctrl) */
static uint32_t s_vi_ctrl = 0x311E, s_vi_mode_aa = 0x100;

void osViSetMode(OSViMode *mode)
{
    s_vi_ctrl = mode->regs[0];
    s_vi_mode_aa = s_vi_ctrl & 0x300;
}

void osViSetSpecialFeatures(uint32_t f)
{
    if (f & 0x01) s_vi_ctrl |= 0x08;                  /* OS_VI_GAMMA_ON */
    if (f & 0x02) s_vi_ctrl &= ~0x08u;
    if (f & 0x04) s_vi_ctrl |= 0x04;                  /* OS_VI_GAMMA_DITHER_ON */
    if (f & 0x08) s_vi_ctrl &= ~0x04u;
    if (f & 0x10) s_vi_ctrl |= 0x10;                  /* OS_VI_DIVOT_ON */
    if (f & 0x20) s_vi_ctrl &= ~0x10u;
    if (f & 0x40) {                                   /* OS_VI_DITHER_FILTER_ON: and anti-alias */
        s_vi_ctrl |= 0x10000;                         /* with resampling, always fetching */
        s_vi_ctrl &= ~0x300u;
    }
    if (f & 0x80) {
        s_vi_ctrl &= ~0x10000u;
        s_vi_ctrl = (s_vi_ctrl & ~0x300u) | s_vi_mode_aa;
    }
}

uint32_t tgr_vi_ctrl(void) { return s_vi_ctrl; }
void osViBlack(uint8_t on) { (void)on; }
void osInvalDCache(void *p, int32_t n) { (void)p; (void)n; }
void osWritebackDCacheAll(void) {}

/* ---- the VI ----------------------------------------------------------------- */
static uint32_t s_framebuffer;

void osViSwapBuffer(void *fb)
{
    s_framebuffer = tgr_addr32(fb);
    tgr_trace("swap", "%08X", s_framebuffer);
    tgr_gfx_swap(s_framebuffer);
}

void *osViGetCurrentFramebuffer(void)
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

int32_t osPiStartDma(OSIoMesg *mb, int32_t pri, int32_t direction, uint32_t devAddr, void *vAddr,
                     uint32_t nbytes, OSMesgQueue *mq)
{
    uint32_t src = devAddr & 0x0FFFFFFF;
    (void)pri;
    if (direction != OS_READ) {
        fprintf(stderr, "tgr: a DMA to the cartridge\n");
        abort();
    }
    tgr_rom_read(src, vAddr, nbytes);
    if (mb)
        mb->hdr.retQueue = tgr_addr32(mq);
    tgr_os_lock();
    tgr_post_mesg_at(tgr_count(), mq, tgr_addr32(mb));
    tgr_os_unlock();
    return 0;
}

int32_t osPiReadIo(uint32_t devAddr, uint32_t *data)
{
    uint32_t off = devAddr & 0x0FFFFFFF;
    uint8_t b[4];
    tgr_rom_read(off, b, 4);
    *data = (uint32_t)b[0] << 24 | (uint32_t)b[1] << 16 | (uint32_t)b[2] << 8 | b[3];
    return 0;
}

/* ---- the RCP: tasks --------------------------------------------------------- */
void osSpTaskLoad(OSTask *task)
{
    tgr_os_lock();
    if (task->t.type == M_GFXTASK) {
        tgr_gfx_task(task->t.data_ptr);
        tgr_post_event_at(tgr_count() + 1, OS_EVENT_SP);
        tgr_post_event_at(tgr_count() + 2, OS_EVENT_DP);
    } else {
        tgr_post_event_at(tgr_count() + 1, OS_EVENT_SP);
    }
    tgr_os_unlock();
}

void osSpTaskStartGo(OSTask *task) { (void)task; }

/* ---- the audio interface: buffers played at the set rate, 4 bytes a sample -- */
#define AI_CLOCK 48681812u
static struct { uint64_t start; uint32_t n; } s_aiq[2];
static int s_ain;
static uint32_t s_airate;

static uint64_t ai_ticks(uint32_t bytes)
{
    return (uint64_t)bytes * 46875000ull / ((uint64_t)(s_airate ? s_airate : 1) * 4);
}

static void ai_update(void)
{
    while (s_ain && s_airate) {
        uint64_t played = (tgr_count() - s_aiq[0].start) * s_airate * 4 / 46875000ull;
        uint64_t end;
        if (played < s_aiq[0].n)
            break;
        end = s_aiq[0].start + ai_ticks(s_aiq[0].n);
        s_aiq[0] = s_aiq[1];
        s_ain--;
        if (s_ain)
            s_aiq[0].start = end;               /* it began when its predecessor ended */
    }
}

uint32_t osAiGetLength(void)
{
    uint64_t played;
    ai_update();
    if (!s_ain || !s_airate)
        return 0;
    played = (tgr_count() - s_aiq[0].start) * s_airate * 4 / 46875000ull;
    return played >= s_aiq[0].n ? 0 : (uint32_t)(s_aiq[0].n - played) & ~7u;
}

uint32_t osAiGetStatus(void)
{
    ai_update();
    return s_ain >= 2 ? 0x80000000u : 0;
}

int32_t osAiSetFrequency(uint32_t f)
{
    uint32_t dac = (uint32_t)((double)AI_CLOCK / f + 0.5);
    s_airate = AI_CLOCK / dac;
    return (int32_t)s_airate;
}

int32_t osAiSetNextBuffer(void *buf, uint32_t size)
{
    uint64_t start;
    ai_update();
    if (s_ain >= 2)
        return -1;
    start = tgr_count();
    if (s_ain) {
        uint64_t end = s_aiq[s_ain - 1].start + ai_ticks(s_aiq[s_ain - 1].n);
        if (end > start)
            start = end;
    }
    s_aiq[s_ain].start = start;
    s_aiq[s_ain].n = size;
    s_ain++;
    if (g_tgr.trace) {
        Sha1 h;
        char d[17];
        sha1_init(&h);
        sha1_update(&h, buf, size);
        sha1_hex16(&h, d);
        tgr_trace("ai", "%s", d);
    }
    tgr_audio_buffer((const int16_t *)buf, (int)(size / 4), (int)s_airate);
    return 0;
}

/* ---- printing ---------------------------------------------------------------- */
void osSyncPrintf(const char *fmt, ...)
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
