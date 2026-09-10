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
#include <math.h>

#ifdef BR_MATCHING_BUILD

extern float DAT_10077434;   /* -0.5f -- floor(x - (-0.5)) == round-nearest */
extern float DAT_10077438;   /*  0.0f */
extern float DAT_1007743c;   /*  255.0f */

/* WHAT IT DOES: bilinear-interpolate one 8-bit texel from four corner
 * samples at (s, t), round to nearest with floor(x + 0.5), and clamp into
 * 0..255 so an out-of-range blend saturates rather than wrapping. */
/* @t4-pass 0x10024680 1 2026-09-09 probes 10 bytes 199 insns 62 regions 1 rows 2 census no  (hand, fn.py variants: operand order, casts, clamp polarity, decls -- most inert; no-float-cast worse) */
/* @t4-pass 0x10024680 2 2026-09-09 probes 10 bytes 199 insns 62 regions 1 rows 2 census yes  (hand, fn.py variants: floor/clamp/addend spellings, split r; corpus MISS at +0x6b -- orig fst-keep then fstp-overwrite) */
/* @t3 0x10024680 2026-09-09 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 199/205 insns 62/64 rows 2+0 regions 1 oracle UNCLASSIFIED
 * @t3-effort passes 2 zero-movement 1 2
 * residue is one spill-fst keep (orig fst-keeps a lerp result then
 * fstp-overwrites the same slot) plus one fxch; identical register-blind
 * arithmetic otherwise.  Ghidra dropped the 0..255 clamp.  Dead probes in
 * the two ledger lines.  Do not reopen before the end-grind (CLAUDE.md rule 12). */
/* @implements 0x10024680 glide BrTexLerpU8 */
void BrTexLerpU8(unsigned char *pOut,
                 unsigned char *p00, unsigned char *p10,
                 unsigned char *p01, unsigned char *p11,
                 float s, float t)
{
    float u, v, r;

    u = ((float)*p10 - (float)*p00) * s + (float)*p00;
    v = ((float)*p11 - (float)*p01) * s + (float)*p01;
    r = (float)floor((double)((v - u) * t + u - DAT_10077434));
    if (r < DAT_10077438)
        r = DAT_10077438;
    if (r > DAT_1007743c)
        r = DAT_1007743c;
    *pOut = (unsigned char)(int)r;
}

#endif /* BR_MATCHING_BUILD */
