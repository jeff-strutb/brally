/* br_textmode.c -- drawing.  See br_textmode.h. */
#include "br_mat.h"   /* br_globals: its objects */
#include "slice1_05.h"   /* br_globals: its objects */
#include "slice2_14.h"   /* br_globals: its objects */
#include "br_textmode.h"

#include <stddef.h>
#include <string.h>

/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* WHAT IT DOES: turn off a one-byte text-pass flag sitting next to the
 * alignment byte.  The next string is drawn without that special pass. */
/* @implements 0x10016810 glide BrClear_10019250 */
/* @n64 0x8022F4F8 located */
void BrClear_10019250(void)
{
    g_4B0360 = 0;
}

/* WHAT IT DOES: centre the next string the HUD/menu text emitter draws.
 * 0 is left, 1 is right, 2 is centre -- this is the centre setter. */
/* @implements 0x10016830 glide BrSet_10019270 */
/* @n64 0x8022F4CC located */
void BrSet_10019270(void)
{
    (*(uint8_t *)&DAT_104abb44) = 2;
}

/* WHAT IT DOES: wipe two 128-byte scratch tables used by a later pass. */
/* @implements 0x1000F620 d3d BrClearTables_1000F620 */
void BrClearTables_1000F620(void)
{
    memset(&(DAT_1035faf0[0]), 0, 0x80);
    memset(&(DAT_1035f750[0]), 0, 0x80);
}

/* WHAT IT DOES: rewind a two-word cursor so the next read starts at the
 * beginning. */
/* @implements 0x1006CDD0 glide BrPairReset_10073B90 */
void BR_THISCALL1 BrPairReset_10073B90(uint32_t *pThis)
{
    pThis[0] = 0;
    pThis[1] = 0;
}

/* WHAT IT DOES: rebuilds the clipper's free list: 64 vertex nodes chained
 * together, last node first. */
/* @implements 0x1000C9C0 glide BrNodeChainReset_1000F460 */
void BrNodeChainReset_1000F460(void)
{
    /* prev is zeroed BEFORE the cursor is materialised (xor ecx,ecx first). */
    uint32_t prev = 0;
    uint32_t *p = &(*(uint32_t *)((char *)&(*(uint32_t *)&g_2E54C0) + 0x9D8)) /* BR_LP64_BYTE_VIEW */;

    do {
        *p = prev;
        prev = (uint32_t)(uintptr_t)p;
        p = (uint32_t *)((char *)p - 0x28);
    } while ((int)(uintptr_t)p >= (int)(uintptr_t)&(*(uint32_t *)&g_2E54C0));
    g_pBrLerpFree = prev;
}

/* One camera's viewport record, 0x58 bytes; only the rectangle is read. */
typedef struct BrVisView {
    int x, y, w, h;
    unsigned char pad[0x48];
} BrVisView;

/* One corner of a screen box: x right and y up from the viewport centre
 * (cx, cy), each scaled by the half-extent and truncated through __ftol. */
static __inline void br_corner(short *p, int cx, int cy, float x, float y, float w, float h)
{
    p[0] = (short)(cx + (int)(x * w));
    p[1] = (short)(cy - (int)(y * h));
}

/* Transcribed from the Glide bytes: the viewport is pView[g_brIView]; the
 * half-extents are w >> 1 and h >> 1 (sar, not a division); the mirror test is
 * 0x106EA3F4 ^ 0x106E8204 with an fchs; __ftol for all four corners.
 * Load-bearing: the rectangle is read into locals first and cx is formed
 * before each half-extent converts; the output vector is reached through a
 * pointer, and the mirrored x is stored in both arms of an if/else (one store
 * at the join, reloaded for both corners), as the original has it. */
/* WHAT IT DOES: works out the screen box a sphere covers. The centre is run
 * through the view matrix; if it is not (nearly) in the camera plane it is
 * divided through by depth, its x mirrored when exactly one of the two mirror
 * flags is set, and a square of half-size n (scaled by the same 1/depth) is
 * drawn round it. The corners are mapped onto the current viewport, x right
 * and y up from its centre, and written as shorts: min corner to pMin, max
 * corner to pMax. Nothing is written when the depth is within 0.001 of 0. */
/* @t3 0x1000C9E0 2026-09-27 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 308/308 insns 99/99 rows 2+2 regions 1 oracle EQUIVALENT
 * @t3-effort passes 4 zero-movement 3 4
 * Residue: one load placement -- the reload of v[0] is issued before
 * `fild n` here, after r * v[1] in the original.  Scheduling only; the
 * dossier and dead list are in the body comment.  Do not reopen before the
 * end-grind. */
/* @t4-pass 0x1000C9E0 3 2026-09-27 probes 82 bytes 308 insns 99 regions 1 rows 4 census yes  (hand: symbol-table census, extern-int pads 0-1296 ahead of the function) */
/* @t4-pass 0x1000C9E0 4 2026-09-27 probes 12 bytes 308 insns 99 regions 1 rows 4 census no  (hand: 12 compiler option sets incl. /Ox /Ob0 /Ob2 /G3 /G5 /Gi /TP) */
/* @implements 0x1000C9E0 glide FUN_1000c9e0 */
void FUN_1000c9e0(BrVisView *pView, const void *pPt, int n, short *pMin,
                  short *pMax)
{
    /* 64-bit core: declared once, in br_globals.h or its struct's header */
    /* 64-bit core: declared once, in br_globals.h or its struct's header */
    /* 64-bit core: declared once, in br_globals.h or its struct's header */
    /* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, by its definition's header */
    /* Source facts (2026-09-27): x0 is the plain reload of the mirrored
     * v[0], and both corners go through br_corner with the min corner's
     * arguments parenthesised -- a named x0 = v[0] - rad gets a dead `fst`
     * home (x87 register pressure), the parenthesised helper arguments do
     * not.  sy is declared before rad: that is what makes sy + rad copy rad
     * (fld st(3); fadd st(3)) as the original does; the other way round
     * copies sy.  (r * v[1]) is parenthesised.
     * RESIDUE (T2, 12 B, 308/308, REGNORM 0+0): one load placement -- the
     * reload of v[0] is issued before `fild n` here, after r * v[1] in the
     * original.  x0 first is what keeps the register-variable stack order
     * (x0, sy, rad, r); with rad or sy ahead of it VC5 reshuffles the stack
     * (fld/fxch/fstp st(2)).  Inert: every float declaration order with sy
     * ahead of rad (2520), x0 spelling (pv[0], v[0], *pv), block scope,
     * `register`, A/B/helper parens, rad/sy operand order, a box helper
     * that does the stores.
     * @t4-pass 0x1000C9E0 1 2026-09-26 probes 2400 bytes 308 insns 101 regions 1 rows 14 census yes  (declaration-order random search + name search)
     * @t4-pass 0x1000C9E0 2 2026-09-26 probes 289 bytes 308 insns 101 regions 1 rows 14 census no  (hill-climb from the best order: no neighbour improves) */
    float v[4];
    int cx, cy, vx, vy, vw, vh;
    float sy, fhh, r, sx, x0, rad, fhw;
    float *pv = v;

    vx = pView[g_BrEnvSection].x;
    vy = pView[g_BrEnvSection].y;
    vw = pView[g_BrEnvSection].w;
    vh = pView[g_BrEnvSection].h;
    cx = vx + (vw >> 1);
    fhw = (float)(vw >> 1);
    cy = vy + (vh >> 1);
    fhh = (float)(vh >> 1);
    BrMat4TransformPoint4(v, pPt, (*(float (*)[16])&g_BrCurMat));
    if (pv[3] > 0.001f || pv[3] < -0.001f) {
        r = 1.0f / pv[3];
        sx = r * pv[0];
        if (g_brRaceBeginDifficulty ^ BrG_6C1174)   /* 0x106EA3F4 ^ 0x106E8204 */
            pv[0] = -sx;
        else
            pv[0] = sx;
        x0 = pv[0];
        rad = ((float)n * r);
        sy = (r * pv[1]);
        pv[0] = pv[0] + rad;
        pv[1] = sy + rad;
        br_corner(pMin, cx, cy, (x0 - rad), (sy - rad), fhw, fhh);
        br_corner(pMax, cx, cy, pv[0], pv[1], fhw, fhh);
    }
}

/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* WHAT IT DOES: point the three per-view scratch buffers at view index
 * 0x106ED67C's slice -- each base is stored to two cursors (base and write
 * head).  Strides 80000 / 32000 / 256000 bytes lower as lea chains. */
/* @t4-pass 0x1000CB20 1 2026-09-07 probes 38 bytes 96 insns 24 regions 1 rows 0 census yes  (tools/crank.py) */
/* @t4-pass 0x1000CB20 2 2026-09-07 probes 38 bytes 96 insns 24 regions 1 rows 0 census yes  (tools/crank.py) */
/* @t4-pass 0x1000CB20 3 2026-09-09 probes 10 bytes 96 insns 24 regions 1 rows 2 census no  (hand, fn.py variants: addend order, constant spellings, product/sum locals, store order, all inert or worse) */
/* @t4-pass 0x1000CB20 4 2026-09-09 probes 10 bytes 96 insns 24 regions 1 rows 2 census yes  (hand, fn.py variants: pair temps, typed/paren/minus forms, shl decompositions, all inert or worse; corpus MISS at +0x33 len 12) */
/* @t3 0x1000CB20 2026-09-09 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 96/96 insns 24/24 rows 1+1 regions 1 oracle EQUIVALENT
 * @t3-effort passes 4 zero-movement 3 4
 * residue is one coalescing fork: the 32000-product chain lands in edx and
 * folds the absolute base into `lea ecx,[edx+A]` where the original keeps
 * ecx and `add ecx,A` (paired by t3.py canon's reloc'd-base lea/add class,
 * ff83432).  Dead probes in the RESIDUE note and the two ledger lines.
 * Do not reopen before the end-grind. */
/* @implements 0x1000CB20 glide BrViewBuffersRebase */

void BrViewBuffersRebase(void)

{
  /* ABSOLUTE base addresses (add ecx,imm32, no reloc) -- the same
   * absolute-address spelling br_scenedl.c proved for its row transforms;
   * a symbol base emits lea reg,[reg+disp32] instead. */
  DAT_1035f7d8 = 0x1035fba8 + (*(int *)((char *)&g_aBrEntRecs + 0x4C)) * 80000;
  DAT_102e16b0 = 0x1035fba8 + (*(int *)((char *)&g_aBrEntRecs + 0x4C)) * 80000;
  /* RESIDUE (4B): the 32000 product's last lea lands in edx and folds the
   * base add into a lea (orig keeps ecx and a plain add) -- coalescing
   * residue; temp-binding and addend order probed, both no better. */
  DAT_1035faec = 0x103874a8 + (*(int *)((char *)&g_aBrEntRecs + 0x4C)) * 32000;
  DAT_1035fba4 = 0x103874a8 + (*(int *)((char *)&g_aBrEntRecs + 0x4C)) * 32000;
  DAT_102e16ac = 0x102e1710 + (*(int *)((char *)&g_aBrEntRecs + 0x4C)) * 0x3e800;
  DAT_1035f7dc = 0x102e1710 + (*(int *)((char *)&g_aBrEntRecs + 0x4C)) * 0x3e800;
  return;
}
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* 0x10019260, 12 call sites. */
/* WHAT IT DOES: clears one of the text drawing flags. A wrapper onto the
 * body that lives with the rest of the text code. */
/* @implements 0x10016820 glide BrTextFlag358Clear */
void BrTextFlag358Clear(void)
{
    g_br4B0358 = 0;
}

