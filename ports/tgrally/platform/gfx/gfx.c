/* gfx.c: the RCP (placeholder until the display-list interpreter lands):
 * for each graphics task, the digest n64box makes of it (the display list
 * walked through its branches with the matrices, vertices and other data it
 * names), so the two runs can be compared task for task. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "plat.h"
#include "../render/rdr.h"
#include "brr_png.h"
#include "host.h"
#include "sha1.h"
#include "tgr_syms.h"
#include "tgr_core.h"

static uint32_t s_fb;

static const uint8_t *ram(uint32_t a) { return tgr_rdram + (a & 0x7FFFFF); }
static uint32_t rd32(uint32_t a) { const uint8_t *p = ram(a); return (uint32_t)p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3]; }

/* n64box.Box.dl_digest, line for line (its segment table follows F3DEX2's
 * G_MOVEWORD, which this F3DEX 1.x game never sends, so segments read as 0) */
static void dl_digest(uint32_t dl, char out[17])
{
    Sha1 h;
    uint32_t seg[16] = {0}, stack[64];
    int sp = 0;
    long seen = 0;
    sha1_init(&h);
    stack[sp++] = dl;
#define ADDR(a) ((seg[((a) >> 24) & 0xF] + ((a) & 0xFFFFFF)) | 0x80000000u)
    while (sp && seen < 200000) {
        uint32_t pc = stack[--sp];
        while (seen < 200000) {
            uint32_t w0 = rd32(pc), w1 = rd32(pc + 4), op = w0 >> 24;
            uint8_t b[8];
            seen++;
            if (op == 0xF5 && ((w0 >> 21) & 7) != 2)
                w1 &= ~0x00F00000u;     /* a non-CI tile's palette: unused (BrTexLoad) */
            b[0] = w0 >> 24; b[1] = w0 >> 16; b[2] = w0 >> 8; b[3] = w0;
            b[4] = w1 >> 24; b[5] = w1 >> 16; b[6] = w1 >> 8; b[7] = w1;
            sha1_update(&h, b, 8);
            pc += 8;
            if (op == 0xDB && ((w0 >> 16) & 0xFF) == 0x06) {
                seg[((w0 & 0xFFFF) >> 2) & 0xF] = w1 & 0x1FFFFFFF;
            } else if (op == 0x01) {
                sha1_update(&h, ram(ADDR(w1)), 64);
            } else if (op == 0x04) {
                uint32_t n = (w0 >> 10) & 0x3F;
                sha1_update(&h, ram(ADDR(w1)), 16 * (n ? n : 1));
            } else if (op == 0x03) {
                sha1_update(&h, ram(ADDR(w1)), 16);
            } else if (op == 0x06) {
                if (((w0 >> 16) & 0xFF) == 1) {
                    pc = ADDR(w1);
                } else {
                    if (sp < 64)
                        stack[sp++] = pc;
                    pc = ADDR(w1);
                }
            } else if (op == 0xB8) {
                break;
            }
        }
    }
    sha1_hex16(&h, out);
}

void tgr_gfx_init(void) {}

static int s_task;

/* the whole of game memory as the original would hold it, to a file: a
 * 4-byte display list (little-endian), then the 8 MB.  The pointer
 * variables are native objects, so the original addresses they hold go
 * back where the original kept them.  tools/lockstep.py reads it; lldb can
 * call it at any breakpoint (tools/fncheck.py). */
void tgr_dump_state(const char *path, uint32_t dl)
{
    FILE *f = fopen(path, "wb");
    uint8_t *m;
    int i;
    uint32_t k;
    if (!f)
        return;
    m = (uint8_t *)malloc(TGR_RDRAM_SIZE);
    memcpy(m, tgr_rdram, TGR_RDRAM_SIZE);
    for (i = 0; i < tgr_nnatives; i++) {
        const TgrNat *n = &tgr_natives[i];
        for (k = 0; k < n->count; k++) {
            void *p = n->nat[k];
            uint32_t a = 0;
            if (p) {
                uintptr_t d = (uintptr_t)p - (uintptr_t)tgr_rdram;
                a = d < TGR_RDRAM_SIZE ? 0x80000000u + (uint32_t)d : tgr_fnaddr(p);
            }
            tgr_wr32(m + (n->addr & 0x7FFFFF) + 4 * k, a);
        }
    }
    fwrite(&dl, 4, 1, f);
    fwrite(m, 1, TGR_RDRAM_SIZE, f);
    fclose(f);
    free(m);
}

static int s_task;

/* TGR_DUMP_TASK=k: game memory when the k-th graphics task is submitted */
static void dump_task(uint32_t dl)
{
    const char *e = getenv("TGR_DUMP_TASK");
    char path[512];
    if (!e || atoi(e) != s_task)
        return;
    snprintf(path, sizeof path, "%s.task%d.bin", getenv("TGR_DUMP") ? getenv("TGR_DUMP") : "tgr", s_task);
    tgr_dump_state(path, dl);
}

void tgr_gfx_task(uint32_t dl)
{
    char d[17];
    dump_task(dl);
    s_task++;
    if (g_tgr.trace) {
        dl_digest(dl, d);
        tgr_trace("gfx", "%s", d);
    }
    if (!g_tgr.headless || g_tgr.shot_dir)     /* drawn only if a window or a shot will show it */
        tgr_rcp_task(dl);
}

void tgr_gfx_swap(uint32_t fb) { s_fb = fb; }

/* a named screenshot: --shots DIR --shot-at F1,F2,... */
static int shot_wanted(uint32_t frame)
{
    const char *p = g_tgr.shot_at;
    while (p && *p) {
        if ((uint32_t)strtoul(p, NULL, 10) == frame)
            return 1;
        p = strchr(p, ',');
        if (p)
            p++;
    }
    return 0;
}

/* the retrace: the last finished frame to the window, and a shot if one is asked for */
void tgr_gfx_present(void)
{
    int w, h, shot = g_tgr.shot_dir && shot_wanted(tgr_frame());
    const uint32_t *px;
    uint32_t f = tgr_frame();
    if (rdr_presents() && !shot)
        return;                                 /* the renderer shows its own frames */
    px = rdr_frame_pixels(&w, &h);
    if (!px || !w)
        return;
    if (shot) {
        char path[512];
        snprintf(path, sizeof path, "%s/frame%05u.png", g_tgr.shot_dir, f);
        brr_png_write(path, (const uint8_t *)px, w, h, w * 4, BRR_PNG_BGRA);
    }
    if (!g_tgr.headless && !rdr_presents())
        host_present(px, w, h);
}
