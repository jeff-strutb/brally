/* br_optdifficulty.c -- menus: publish the chosen difficulty as the two
 * derived globals the rest of the game reads, and only when it has actually
 * changed (0x1003DA90 glide / 0x10044540 d3d).
 *
 * Filed out of slice6_72.c, whose preamble it keeps verbatim below so the
 * compiler's view of the body is unchanged.  The original banner follows.
 *
 * slice6_72.c -- BRD3D.dll, packet 72.  See slice6_72.h.
 *
 * Float literals below are the exact values of the 32-bit patterns the
 * original pushes or loads.  The ones read as memory operands were taken out
 * of orig/BRD3D.dll's .rdata with tools/pe.py, not assumed:
 *
 *   0x1008F410 =    0.0     0x1008F514 =    2.0
 *   0x1008F3BC =  255.0     0x1008F3C0 =    1/255 (0x3B808081)
 *   0x1008F680 =  -19.0     0x1008F684 =  -38.0     0x1008F688 =  -57.0
 *   0x1008F68C =  -76.0     0x1008F690 =  -95.0     0x1008F694 = -114.0
 *   0x1008F698 = -133.0     0x1008F69C =  -33.0     0x1008F6A0 =  +19.0
 *
 * `fsub m32` is the non-reversed form, st(0) = st(0) - m32, so every row
 * constant except 0x1008F6A0 -- which is POSITIVE -- is an addition.
 */

#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include <string.h>

#include "slice6_72.h"

/* ==========================================================================
 * 0x10044540
 * ==========================================================================
 *
 * GOTCHA: the guard compares the LAST published value with the current one
 * and returns without touching anything when they agree -- so the two outputs
 * are stale, not merely unchanged, if something else wrote them.
 * GOTCHA: `cmp eax,4` + `ja` is UNSIGNED, so a negative selector takes the
 * out-of-range arm rather than indexing backwards.
 */
/* WHAT IT DOES: turns the chosen car group into the two derived numbers the
 * rest of the game actually consults -- a bit pattern that looks like the set
 * of cars that group allows, and a small count -- so nothing else has to know
 * the group numbering. It only recomputes when the group differs from the last
 * time it ran, which means that if anything else writes those two values they
 * are left stale rather than corrected. */
#ifdef BR_MATCHING_BUILD
/* Orig stores through absolute globals, not g_pBr72Env->field. */
extern int32_t g_brAA2A18;
extern int32_t g_brAA2A44;
extern int32_t g_br0AB3E8;
extern int32_t g_br0AC654;
#endif

/* WHAT IT DOES: push the current difficulty setting out to the two globals
 * the rest of the game reads, but only when it has actually changed. GOTCHA:
 * any value above 4 falls through to the default, which writes the same pair
 * as case 0 -- so an out-of-range difficulty silently behaves as the
 * easiest. */
/* @implements 0x1003DA90 glide BrSub10044540 */
void BrSub10044540(void)
{
#ifdef BR_MATCHING_BUILD
    int32_t n = g_brAA2A18;

    if (g_brAA2A44 == n) {
        return;
    }
    g_brAA2A44 = n;

    /* One unsigned cmp-4/ja plus a 0..4 jump table; case 4 is in the table,
     * default (>4) writes the case-0 pair in reverse store order. */
    switch (n) {
    case 0:  g_br0AB3E8 = 0x102;  g_br0AC654 = 1;    break;
    case 1:  g_br0AB3E8 = 0x81;   g_br0AC654 = 0;    break;
    case 2:  g_br0AB3E8 = 0x4050; g_br0AC654 = 6;    break;
    case 3:  g_br0AB3E8 = 0x202C; g_br0AC654 = 3;    break;
    case 4:  g_br0AB3E8 = 0x1E00; g_br0AC654 = 0x0B; break;
    default:
        g_br0AC654 = 1;
        g_br0AB3E8 = 0x102;
        break;
    }
#else
    Br72Env *pE = g_pBr72Env;
    int32_t  n  = pE->nAA2A18;

    if (pE->nAA2A44 == n) {
        return;
    }
    pE->nAA2A44 = n;

    if ((uint32_t)n > 4u) {
        /* 0x100445CD -- the same pair as case 0, written the other way
         * round.  Order preserved for the record; no observer here. */
        pE->n0AC654 = 1;
        pE->n0AB3E8 = 0x102;
        return;
    }

    switch (n) {                        /* jump table at 0x100445E4 */
    case 0:  pE->n0AB3E8 = 0x102;  pE->n0AC654 = 1;    break;
    case 1:  pE->n0AB3E8 = 0x81;   pE->n0AC654 = 0;    break;
    case 2:  pE->n0AB3E8 = 0x4050; pE->n0AC654 = 6;    break;
    case 3:  pE->n0AB3E8 = 0x202C; pE->n0AC654 = 3;    break;
    default: pE->n0AB3E8 = 0x1E00; pE->n0AC654 = 0x0B; break;   /* case 4 */
    }
#endif
}
