/* spu.c: the sound processing unit: 24 ADPCM voices reading sound RAM */
#include <stdint.h>
#include "ps1.h"
#include "spu.h"

#define SPU(a)      PS1_IO16(0x1C00 + (a))
#define VOICE(v, r) SPU((v) * 0x10 + (r))
#define SPUCNT      SPU(0x1AA)
#define SPUSTAT     SPU(0x1AE)

static uint32_t s_kon, s_koff;

/* TGR_SPULOG: every voice write and sound-RAM upload to spu.log, stamped with
 * the console's time, for tools/spusim.py */
#include <stdio.h>
#include <stdlib.h>
#include "plat.h"
static FILE *s_log;
static int s_log_init;

static void log_ev(int op, int v, uint32_t a, uint32_t b, const void *data, uint32_t n)
{
    uint32_t w[5];
    if (!s_log_init) {
        s_log_init = 1;
        if (getenv("TGR_SPULOG"))
            s_log = fopen("spu.log", "w");
    }
    if (!s_log)
        return;
    w[0] = (uint32_t)(tgr_count() / 1000);      /* N64 count / 1000: 46875 a second */
    w[1] = (uint32_t)op | (uint32_t)v << 8;
    w[2] = a;
    w[3] = b;
    w[4] = n;
    fwrite(w, 4, 5, s_log);
    if (n)
        fwrite(data, 1, n, s_log);
}

void spu_log_flush(void)
{
    if (s_log)
        fflush(s_log);
}

static void wait_stat(uint16_t mask, uint16_t val)
{
    int n = 100000;
    while ((SPUSTAT & mask) != val && --n)
        ;
}

void spu_init(void)
{
    int v;
    SPUCNT = 0;
    wait_stat(0x3F, 0);
    SPU(0x180) = 0;                             /* main volume off while setting up */
    SPU(0x182) = 0;
    SPU(0x184) = 0;                             /* no reverb */
    SPU(0x186) = 0;
    SPU(0x18C) = 0xFFFF;                        /* every voice off */
    SPU(0x18E) = 0x00FF;
    SPU(0x190) = 0;                             /* no pitch modulation, noise, echo */
    SPU(0x192) = 0;
    SPU(0x194) = 0;
    SPU(0x196) = 0;
    SPU(0x198) = 0;
    SPU(0x19A) = 0;
    SPU(0x1AC) = 0x0004;                        /* transfers: normal */
    SPU(0x1B0) = 0;                             /* CD and external audio off */
    SPU(0x1B2) = 0;
    SPU(0x1B4) = 0;
    SPU(0x1B6) = 0;
    for (v = 0; v < 24; v++) {
        VOICE(v, 0) = 0;
        VOICE(v, 2) = 0;
        VOICE(v, 4) = 0x1000;
        VOICE(v, 6) = 0x200;
        VOICE(v, 8) = 0x000F;                   /* ADSR: instant attack, full sustain */
        VOICE(v, 0xA) = 0x0000;
    }
    SPUCNT = 0xC000;                            /* on, unmuted */
    SPU(0x180) = 0x3FFF;
    SPU(0x182) = 0x3FFF;
}

/* to sound RAM by hand, 32 halfwords at a time through the FIFO */
void spu_write(uint32_t addr, const void *data, uint32_t bytes)
{
    log_ev(1, 0, addr, 0, data, bytes);
    const uint16_t *p = data;
    uint32_t n = (bytes + 1) / 2;
    SPUCNT = (uint16_t)(SPUCNT & ~0x30);        /* transfers stopped */
    wait_stat(0x30, 0);
    SPU(0x1A6) = (uint16_t)(addr >> 3);
    while (n) {
        uint32_t k = n > 32 ? 32 : n, i;
        for (i = 0; i < k; i++)
            SPU(0x1A8) = *p++;
        SPUCNT = (uint16_t)((SPUCNT & ~0x30) | 0x10);   /* manual write */
        wait_stat(0x400, 0);                    /* the FIFO drained */
        SPUCNT = (uint16_t)(SPUCNT & ~0x30);
        wait_stat(0x30, 0);
        n -= k;
    }
}

void spu_voice_start(int v, uint32_t addr)
{
    log_ev(2, v, addr, 0, 0, 0);
    VOICE(v, 6) = (uint16_t)(addr >> 3);
    VOICE(v, 0xE) = (uint16_t)(addr >> 3);
    s_kon |= 1u << v;
    s_koff &= ~(1u << v);
}

void spu_voice_off(int v)
{
    log_ev(3, v, 0, 0, 0, 0);
    s_koff |= 1u << v;
    s_kon &= ~(1u << v);
}

void spu_voice_pitch(int v, uint32_t pitch)
{
    log_ev(4, v, pitch > 0x3FFF ? 0x3FFF : pitch, 0, 0, 0);
    VOICE(v, 4) = (uint16_t)(pitch > 0x3FFF ? 0x3FFF : pitch);
}

void spu_voice_volume(int v, int l, int r)
{
    log_ev(5, v, (uint32_t)(l < 0 ? 0 : l > 0x3FFF ? 0x3FFF : l), (uint32_t)(r < 0 ? 0 : r > 0x3FFF ? 0x3FFF : r), 0, 0);
    VOICE(v, 0) = (uint16_t)(l < 0 ? 0 : l > 0x3FFF ? 0x3FFF : l);
    VOICE(v, 2) = (uint16_t)(r < 0 ? 0 : r > 0x3FFF ? 0x3FFF : r);
}

void spu_keys(void)
{
    if (s_koff) {
        SPU(0x18C) = (uint16_t)s_koff;
        SPU(0x18E) = (uint16_t)(s_koff >> 16);
    }
    if (s_kon) {
        SPU(0x188) = (uint16_t)s_kon;
        SPU(0x18A) = (uint16_t)(s_kon >> 16);
    }
    s_kon = s_koff = 0;
}
