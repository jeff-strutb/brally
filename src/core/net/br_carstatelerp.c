/* br_carstatelerp.c -- Network car-state smoothing: BrCarStateLerp blends two received car states
 * (with forward prediction up to ten steps past the second) so remote cars
 * move smoothly between network updates.
 *
 * Filed out of the address batch slice1_02.c; its preamble is carried verbatim.
 */

#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include "slice1_02.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
/* =====================================================================
 * 2. Car-state packet
 * ===================================================================== */

/* BrCarStateLerp walks the struct as a flat float array, exactly as the
 * original walks the 0xA0-byte record. Catch any accidental padding here
 * rather than at run time. */
typedef char BrCarStateSizeCheck[
    (sizeof(BrCarState) == BR_CARSTATE_FLOATS * sizeof(float)) ? 1 : -1];

/* 0x1008F0CC = 128.0f and 0x1008F118 = 1.0f: the decoder uses BOTH as the
 * "true" value for different 1-bit fields, so they are not interchangeable. */
#define BR_ONE_128  128.0f
#define BR_ONE_1      1.0f
/* 0x100079E0.  0x1008F0C8 = 0.0f, 0x1008F148 = 10.0f, 0x1008F118 = 1.0f. */
/* WHAT IT DOES: blends between two car states -- the heart of the game's
 * smoothing between network updates. The blend factor may run up to ten
 * times past the second state, because the game is predicting ahead of the
 * last packet it received rather than merely filling in between two it has.
 * When the two facings point opposite ways it flips one of them first, so a
 * car does not spin the long way round; and the last field is copied
 * outright rather than blended. */
/* @t4-pass 0x10007D50 1 2026-09-09 probes 10 bytes 291 insns 100 regions 3 rows 0 census no  (hand, fn.py variants: clamp/negate/loop/copy spellings, decl orders, all inert or worse) */
/* @t4-pass 0x10007D50 2 2026-09-09 probes 10 bytes 291 insns 100 regions 3 rows 0 census yes  (hand, fn.py variants: loop rewrites while/do, condition swaps, index respellings, all inert or worse; corpus query at +0x20) */
/* @t3 0x10007D50 2026-09-09 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 291/291 insns 100/100 rows 0+0 regions 3 oracle UNCLASSIFIED
 * @t3-effort passes 2 zero-movement 1 2
 * residue is register colouring/scheduling only: identical register-blind
 * multiset (rows 0+0), size- and insn-exact, 3 masked regions.  Dead
 * probes in the two ledger lines.
 * Do not reopen before the end-grind (CLAUDE.md rule 12). */
/* @implements 0x100079E0 d3d BrCarStateLerp */
void BrCarStateLerp(BrCarState *pDst, float t,
                    const BrCarState *pA, const BrCarState *pB)
{
    const float *a = (const float *)pA;
    const float *b = (const float *)pB;
    float       *d = (float *)pDst;
    int          i;
    int          negate;

    /* Clamp to [0, 10]. Both guards are written so that the x87 "unordered"
     * result takes the same branch a NaN takes here: NaN ends up at 0. */
    if (!(t > 0.0f))
        t = 0.0f;
    else if (!(t < 10.0f))
        t = 10.0f;

    /* Quaternion double-cover fix: only when the leading components have
     * opposite signs AND are at least 1.0 apart. Note this is a magnitude
     * threshold, not a dot-product sign test. */
    negate = (a[0] >= 0.0f && b[0] < 0.0f && (a[0] - b[0]) >= 1.0f)
          || (b[0] >= 0.0f && a[0] < 0.0f && (b[0] - a[0]) >= 1.0f);

    if (negate) {
        for (i = 0; i < 4; ++i)
            d[i] = (-b[i] - a[i]) * t + a[i];
    } else {
        for (i = 0; i < 4; ++i)
            d[i] = (b[i] - a[i]) * t + a[i];
    }

    /* The tail loop is a separate 36-iteration run over +0x10..+0x9C and is
     * NOT affected by the negation. */
    for (i = 4; i < BR_CARSTATE_FLOATS; ++i)
        d[i] = (b[i] - a[i]) * t + a[i];

    /* And then the last field is overwritten outright (a raw dword move in the
     * original), discarding the value the loop just produced. */
    pDst->f9C = pB->f9C;
}
