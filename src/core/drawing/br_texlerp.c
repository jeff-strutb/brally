/* br_texlerp.c -- drawing: bilinear texel sample.
 *
 * The rasteriser at 0x10024490 dispatches per-pixel to one of three
 * interpolators by texel format.  This file is that family.  Ghidra's
 * drafts drop the post-floor clamp against 0x10077438 / 0x1007743c
 * (0.0f and 255.0f); the original always saturates before __ftol.
 */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#endif
#include <stdio.h>
#include <math.h>

#ifdef BR_MATCHING_BUILD

extern float DAT_10077434;   /* -0.5f -- floor(x - (-0.5)) == round-nearest */
extern float DAT_10077438;   /*  0.0f */
extern float DAT_1007743c;   /*  255.0f */

/* WHAT IT DOES: bilinear-interpolate one 8-bit texel from four corner
 * samples at (s, t), round to nearest with floor(x + 0.5), and clamp into
 * 0..255 so an out-of-range blend saturates rather than wrapping. */
/* Byte-exact 2026-09-28, hand-transcribed.  The corners are read into four
 * named floats in memory order (p00, p10, p01, p11); the bottom-row lerp is
 * a named v, and the top-row lerp is written back into s, which is dead after
 * it -- that is the original's `fst v; fstp [s]` pair, v kept on the stack
 * and the top-row result homed in s's slot for its two later reads.  With a
 * separate u (the earlier transcription) VC5 keeps u on the stack and the
 * byte conversions share one temporary slot (6 B short). */
/* @implements 0x10024680 glide BrTexLerpU8 */
void BrTexLerpU8(unsigned char *pOut,
                 unsigned char *p00, unsigned char *p10,
                 unsigned char *p01, unsigned char *p11,
                 float s, float t)
{
    float a, b, c, d, v, r;

    a = *p00;
    b = *p10;
    c = *p01;
    d = *p11;
    v = (d - c) * s + c;
    s = (b - a) * s + a;
    r = (float)floor((double)((v - s) * t + s - DAT_10077434));
    if (r < DAT_10077438)
        r = DAT_10077438;
    if (r > DAT_1007743c)
        r = DAT_1007743c;
    *pOut = (unsigned char)(int)r;
}

extern float DAT_10077430;   /* 0.5f */

extern int FUN_10024df0(int mode);                    /* 0x10024DF0 texel size */
extern void BrTexLerp1555(unsigned short *pOut,
                          unsigned short *p00, unsigned short *p10,
                          unsigned short *p01, unsigned short *p11,
                          float s, float t);          /* 0x10024750 */
extern void BrTexLerp4444(unsigned short *pOut,
                          unsigned short *p00, unsigned short *p10,
                          unsigned short *p01, unsigned short *p11,
                          float s, float t);          /* 0x10024AA0 */

typedef void (*BrTexLerpFn)(char *pOut, char *p00, char *p10,
                            char *p01, char *p11, float s, float t);

/* WHAT IT DOES: scale a texture to a new size by sampling every destination
 * texel bilinearly from the source: pick the interpolator for the texel
 * format (8-bit palette for modes 2-3, 4444 for mode 12, 1555 for the rest),
 * then walk the destination row by row, mapping each texel centre back into
 * the source, clamping the four neighbour coordinates to the source edges,
 * and letting the interpolator blend them by the fractional position. */
/* T2 RESIDUE (468 orig / 468+table recomp B, raw 21+6, regnorm 16+1,
 * FIRSTDIV +0x15f of 0x1d3): byte-exact through the switch table, the
 * interleaved double-divide x87 head, both loop preambles, both floor/ftol
 * clamp blocks and the whole outer tail.  Everything left is ONE knot in the
 * callback-argument pointer block (+0x15f..+0x188) plus its 2-row echo at
 * the loop tail: the original picks the scalar (hix/lox) as lea BASE where
 * ours picks the hoisted row product, forms p00 with a destructive
 * `add eax,ebp` where ours uses a fresh lea, and loads pDst after all six
 * pushes where ours loads it two pushes early; tail fstp/counter-store
 * order swaps with it.  Same registers hold the same values on both sides
 * at the fork, so the base/index choice is allocator-internal.
 * Source facts that DID move bytes en route: explicit dead cases 4-11
 * (jump table over 2..12), branchless `((i <= 0) - 1) & i` low clamps,
 * loy-then-hiy in the outer body but hix-init / lox / hix-clamp-if split
 * in the inner, sfrac as its own local statement, tx = tx0 above the inner
 * for, and the inner loop as `for (cx = dw; cx > 0; cx--)`.
 * The destination step comes before the sample step, and the TU includes
 * <stdio.h> (the symbol-table size sets VC5's scheduling tie-breaks here). */
/* @implements 0x10024490 glide BrTexResample */
void BrTexResample(char *pDst, int dw, int dh, char *pSrc, int sw, int sh,
                   int mode)
{
    BrTexLerpFn pfn;
    int size;
    float tystep, txstep, ty, tx0, tfrac, tx, sfrac;
    int cy, cx;
    int iy, ix, hiy, hix, loy, lox;

    size = FUN_10024df0(mode);
    switch (mode) {
    case 2:
    case 3:
        pfn = (BrTexLerpFn)BrTexLerpU8;
        break;
    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
    case 9:
    case 10:
    case 11:
        pfn = (BrTexLerpFn)BrTexLerp1555;
        break;
    case 12:
        pfn = (BrTexLerpFn)BrTexLerp4444;
        break;
    default:
        pfn = (BrTexLerpFn)BrTexLerp1555;
        break;
    }
    tystep = (float)sh / (float)dh;
    txstep = (float)sw / (float)dw;
    ty = tystep * DAT_10077430 - DAT_10077430;
    if (dh > 0) {
        cy = dh;
        tx0 = txstep * DAT_10077430 - DAT_10077430;
        do {
            iy = (int)floor((double)ty);
            tfrac = ty - (float)iy;
            loy = ((iy <= 0) - 1) & iy;
            hiy = iy + 1;
            if (hiy >= sh - 1)
                hiy = sh - 1;
            tx = tx0;
            for (cx = dw; cx > 0; cx--) {
                    ix = (int)floor((double)tx);
                    sfrac = tx - (float)ix;
                    hix = ix + 1;
                    lox = ((ix <= 0) - 1) & ix;
                    if (hix >= sw - 1)
                        hix = sw - 1;
                    (*pfn)(pDst,
                           pSrc + (lox + loy * sw) * size,
                           pSrc + (hix + loy * sw) * size,
                           pSrc + (lox + hiy * sw) * size,
                           pSrc + (hix + hiy * sw) * size,
                           sfrac, tfrac);
                    pDst += size;
                    tx += txstep;
            }
            ty += tystep;
            cy--;
        } while (cy != 0);
    }
}

#endif /* BR_MATCHING_BUILD */
