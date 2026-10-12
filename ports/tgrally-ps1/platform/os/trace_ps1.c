/* trace_ps1.c: ports/tgrally/platform/os/trace.c, closed at the end of a run.
 * trace.c: what n64box logs at every retrace, for the lockstep comparison
 * (tools/tgrally/n64box.py --log): "frame kind value" lines. */
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "plat.h"

static FILE *s_f;

void tgr_trace(const char *kind, const char *fmt, ...)
{
    va_list ap;
    if (!g_tgr.trace)
        return;
    if (!s_f && !(s_f = fopen(g_tgr.trace, "w")))
        return;
    fprintf(s_f, "%u %s ", tgr_frame(), kind);
    va_start(ap, fmt);
    vfprintf(s_f, fmt, ap);
    va_end(ap);
    fputc('\n', s_f);
    fflush(s_f);
}

/* TGR_STATEHASH=1: the cars' records hashed at every retrace ("cars"), the
 * game's state apart from what it draws: a check on changes to how it draws */
extern unsigned char D_8031B760[], D_803239A0[];
static int s_statehash = -1;

static void state_hash(void)
{
    const uint32_t *w = (const uint32_t *)D_8031B760, *e = (const uint32_t *)D_803239A0;
    uint32_t h = 0x811C9DC5u;
    for (; w < e; w++)
        h = (h ^ *w) * 0x01000193u;
    tgr_trace("cars", "%08x", (unsigned)h);
}

/* TGR_CARTRACE=1: each car's position and velocity (in 1/1000) at every
 * retrace ("car"), to compare one physics with another */
static int s_cartrace = -1;
typedef struct { float pos[3], vel[3]; } CarSt;

static void car_trace(void)
{
    int i;
    for (i = 0; i < 4; i++) {
        const CarSt *s = (const CarSt *)(D_8031B760 + 0x2090 * i + 0x1C0);
        tgr_trace("car", "%d %d %d %d %d %d %d", i, (int)(s->pos[0] * 1000.0f), (int)(s->pos[1] * 1000.0f),
                  (int)(s->pos[2] * 1000.0f), (int)(s->vel[0] * 1000.0f), (int)(s->vel[1] * 1000.0f),
                  (int)(s->vel[2] * 1000.0f));
    }
}

/* the retrace: the controllers sampled, the frame shown */
void tgr_vi_retrace(void)
{
    tgr_input_frame(tgr_frame());
    tgr_gfx_present();
    if (s_statehash < 0) {
        s_statehash = getenv("TGR_STATEHASH") != NULL;
        s_cartrace = getenv("TGR_CARTRACE") != NULL;
    }
    if (s_statehash && g_tgr.trace)
        state_hash();
    if (s_cartrace && g_tgr.trace)
        car_trace();
}

void tgr_trace_close(void)
{
    if (s_f)
        fclose(s_f);
    s_f = NULL;
}
