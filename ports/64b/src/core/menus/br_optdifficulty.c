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

/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
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
/* Orig stores through absolute globals, not g_pBr72Env->field. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* WHAT IT DOES: push the current difficulty setting out to the two globals
 * the rest of the game reads, but only when it has actually changed. GOTCHA:
 * any value above 4 falls through to the default, which writes the same pair
 * as case 0 -- so an out-of-range difficulty silently behaves as the
 * easiest. */
/* @implements 0x1003DA90 glide BrSub10044540 */
void BrSub10044540(void)
{
    int32_t n = (*(int32_t *)&DAT_10ac5d70);

    if (g_brAA2A44 == n) {
        return;
    }
    g_brAA2A44 = n;

    /* One unsigned cmp-4/ja plus a 0..4 jump table; case 4 is in the table,
     * default (>4) writes the case-0 pair in reverse store order. */
    switch (n) {
    case 0:  (*(int32_t *)&g_br0AB3E8) = 0x102;  (*(int32_t *)&g_brSel0ABDF4) = 1;    break;
    case 1:  (*(int32_t *)&g_br0AB3E8) = 0x81;   (*(int32_t *)&g_brSel0ABDF4) = 0;    break;
    case 2:  (*(int32_t *)&g_br0AB3E8) = 0x4050; (*(int32_t *)&g_brSel0ABDF4) = 6;    break;
    case 3:  (*(int32_t *)&g_br0AB3E8) = 0x202C; (*(int32_t *)&g_brSel0ABDF4) = 3;    break;
    case 4:  (*(int32_t *)&g_br0AB3E8) = 0x1E00; (*(int32_t *)&g_brSel0ABDF4) = 0x0B; break;
    default:
        (*(int32_t *)&g_brSel0ABDF4) = 1;
        (*(int32_t *)&g_br0AB3E8) = 0x102;
        break;
    }
}
