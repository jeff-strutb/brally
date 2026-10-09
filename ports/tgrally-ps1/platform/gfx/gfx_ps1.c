/* gfx_ps1.c: the RCP's tasks on the PlayStation.  Each graphics task gets
 * the digest n64box makes of it (ports/tgrally's gfx.c, dl_digest, kept line
 * for line) for the trace the lockstep tools compare, and is drawn by the
 * GTE and GPU renderer (gfx/rcp_ps1.c). */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "plat.h"
#include "host.h"
#include "sha1.h"
#include "tgr_syms.h"
#include "tgr_core.h"

uint32_t tgr_rcp_frames;
uint64_t tgr_rcp_end_ns, tgr_rcp_end_max_ns;

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

void tgr_gfx_task(uint32_t dl)
{
    char d[17];
    s_task++;
    if (g_tgr.trace) {
        dl_digest(dl, d);
        tgr_trace("gfx", "%s", d);
    }
    if (!g_tgr.headless || g_tgr.shot_dir)     /* drawn only if a window or a shot will show it */
        tgr_rcp_task(dl);
}

void tgr_gfx_swap(uint32_t fb) { s_fb = fb; }
void tgr_gfx_present(void) {}
