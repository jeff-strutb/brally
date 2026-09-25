/* br_textmode.c -- drawing.  See br_textmode.h. */
#include "br_textmode.h"

#include <stddef.h>
#include <string.h>

#ifdef BR_MATCHING_BUILD
extern uint8_t  g_4B0360;
extern uint8_t  g_4B035C;
extern uint32_t g_2E5E98, g_2E54C0, g_2E5ECC;
extern uint32_t g_364308, g_363F68;
#else
uint8_t  g_4B0360;
uint8_t  g_4B035C;
uint32_t g_2E5E98, g_2E54C0, g_2E5ECC;
uint32_t g_364308[32];
uint32_t g_363F68[32];
#endif

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
    g_4B035C = 2;
}

/* WHAT IT DOES: wipe two 128-byte scratch tables used by a later pass. */
/* @implements 0x1000F620 d3d BrClearTables_1000F620 */
void BrClearTables_1000F620(void)
{
    memset(&g_364308, 0, 0x80);
    memset(&g_363F68, 0, 0x80);
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
    uint32_t *p = &g_2E5E98;

    do {
        *p = prev;
        prev = (uint32_t)(uintptr_t)p;
        p = (uint32_t *)((char *)p - 0x28);
    } while ((int)(uintptr_t)p >= (int)(uintptr_t)&g_2E54C0);
    g_2E5ECC = prev;
}

#ifdef BR_MATCHING_BUILD
/* One camera's viewport record, 0x58 bytes; only the rectangle is read. */
typedef struct BrVisView {
    int x, y, w, h;
    unsigned char pad[0x48];
} BrVisView;

/* Transcribed from the Glide bytes: the viewport is pView[g_brIView]; the
 * half-extents are w >> 1 and h >> 1 (sar, not a division); the mirror test is
 * 0x106EA3F4 ^ 0x106E8204 with an fchs; __ftol for all four corners.
 * T2, 310/308 B, REGNORM 1+1 (2026-09-25). Load-bearing: the rectangle is
 * read into locals first and cx is formed before each half-extent converts;
 * the output vector is reached through a pointer, and the mirrored x is
 * stored in both arms of an if/else (one store at the join, reloaded for both
 * corners), as the original has it. Residue: x0 gets a memory home here
 * (one extra fst) where the original keeps it on the x87 stack. Dead:
 * all 5040 float declaration orders, statement orders, inline x0/sy/rad,
 * volatile anything (inert through the pointer), /Op, pad count 1..40. */
/* WHAT IT DOES: works out the screen box a sphere covers. The centre is run
 * through the view matrix; if it is not (nearly) in the camera plane it is
 * divided through by depth, its x mirrored when exactly one of the two mirror
 * flags is set, and a square of half-size n (scaled by the same 1/depth) is
 * drawn round it. The corners are mapped onto the current viewport, x right
 * and y up from its centre, and written as shorts: min corner to pMin, max
 * corner to pMax. Nothing is written when the depth is within 0.001 of 0. */
/* @implements 0x1000C9E0 glide FUN_1000c9e0 */
void FUN_1000c9e0(BrVisView *pView, const void *pPt, int n, short *pMin,
                  short *pMax)
{
    extern int   g_brIView;              /* 0x106EC798 */
    extern float DAT_106e9a38[16];       /* the view matrix */
    extern int   DAT_106ea3f4;
    extern int   DAT_106e8204;
    extern void  BrMat4TransformPoint4(float *pOut, const void *pV,
                                       const float *pM);
    float v[4];
    int cx, cy, vx, vy, vw, vh;
    float fhw, rad, fhh, r, sy, x0, sx;
    float *pv = v;

    vx = pView[g_brIView].x;
    vy = pView[g_brIView].y;
    vw = pView[g_brIView].w;
    vh = pView[g_brIView].h;
    cx = vx + (vw >> 1);
    fhw = (float)(vw >> 1);
    cy = vy + (vh >> 1);
    fhh = (float)(vh >> 1);
    BrMat4TransformPoint4(v, pPt, DAT_106e9a38);
    if (pv[3] > 0.001f || pv[3] < -0.001f) {
        r = 1.0f / pv[3];
        sx = r * pv[0];
        if (DAT_106ea3f4 ^ DAT_106e8204)
            pv[0] = -sx;
        else
            pv[0] = sx;
        rad = (float)n * r;
        sy = r * pv[1];
        x0 = pv[0] - rad;
        pv[0] = pv[0] + rad;
        pv[1] = sy + rad;
        pMin[0] = (short)(cx + (int)(x0 * fhw));
        pMin[1] = (short)(cy - (int)((sy - rad) * fhh));
        pMax[0] = (short)(cx + (int)(fhw * pv[0]));
        pMax[1] = (short)(cy - (int)(fhh * pv[1]));
    }
}
#endif /* BR_MATCHING_BUILD */

#ifdef BR_MATCHING_BUILD
extern int DAT_102e16ac;
extern int DAT_102e16b0;
extern char DAT_102e1710;
extern int DAT_1035f7d8;
extern int DAT_1035f7dc;
extern int DAT_1035faec;
extern int DAT_1035fba4;
extern char DAT_1035fba8;
extern char DAT_103874a8;
extern int DAT_106ed67c;

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
 * Do not reopen before the end-grind (CLAUDE.md rule 12). */
/* @implements 0x1000CB20 glide BrViewBuffersRebase */

void BrViewBuffersRebase(void)

{
  /* ABSOLUTE base addresses (add ecx,imm32, no reloc) -- the same
   * absolute-address spelling br_scenedl.c proved for its row transforms;
   * a symbol base emits lea reg,[reg+disp32] instead. */
  DAT_1035f7d8 = 0x1035fba8 + DAT_106ed67c * 80000;
  DAT_102e16b0 = 0x1035fba8 + DAT_106ed67c * 80000;
  /* RESIDUE (4B): the 32000 product's last lea lands in edx and folds the
   * base add into a lea (orig keeps ecx and a plain add) -- coalescing
   * residue; temp-binding and addend order probed, both no better. */
  DAT_1035faec = 0x103874a8 + DAT_106ed67c * 32000;
  DAT_1035fba4 = 0x103874a8 + DAT_106ed67c * 32000;
  DAT_102e16ac = 0x102e1710 + DAT_106ed67c * 0x3e800;
  DAT_1035f7dc = 0x102e1710 + DAT_106ed67c * 0x3e800;
  return;
}
extern uint8_t g_br4B0358;

/* 0x10019260, 12 call sites. */
/* WHAT IT DOES: clears one of the text drawing flags. A wrapper onto the
 * body that lives with the rest of the text code. */
/* @implements 0x10016820 glide BrTextFlag358Clear */
void BrTextFlag358Clear(void)
{
    g_br4B0358 = 0;
}

#endif /* BR_MATCHING_BUILD */
