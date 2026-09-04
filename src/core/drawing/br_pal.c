/* br_pal.c -- drawing: reading a palette entry.
 *
 * RESPONSIBILITY: drawing/ -- turn geometry and images into pixels.
 *
 * Filed out of slice1_02.c, an address batch and not a module.  Records are
 * three bytes and the copy is straight through -- no channel reordering.
 * The original writes the last byte first; the order is immaterial.
 *
 * slice1_02.c's preamble is carried over verbatim.  An include set that
 * looks redundant has already been shown elsewhere in this module to move
 * VC5's register allocation (see br_rdpmode.c).
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
 * Shared primitives the original reaches through the CRT
 * ===================================================================== */

/* 0x1007DB00 is MSVC's `floor`: it forces the x87 control word to the value at
 * 0x100BD8E0 (0x173F -- RC = 01, round toward -infinity), runs `frndint`, and
 * restores. Verified rather than assumed, because with RC = 11 the same code
 * would be `trunc` and every rounding here would change. */
#define BrFloor(d) floor(d)


/* =====================================================================
 * 4. Palette fetch
 * ===================================================================== */

/* 0x100049C0.  Records are 3 bytes (`ecx + ecx*2 + base`) and the copy is
 * straight: source +0 -> dest +0, +1 -> +1, +2 -> +2 (no channel swap). The
 * original writes the last byte first; order is immaterial. */
/* WHAT IT DOES: reads one colour out of a palette: three bytes at the
 * entry's position, copied straight through with no channel reordering. */
/* @implements 0x100049C0 d3d BrPalFetch */
#ifdef BR_MATCHING_BUILD
/* The original takes no arguments: index is 0x10094294, table is
 * 0x100B37D0, dest is 0x10AD0854.  The port signature is the header's.
 * volatile on the index stops VC5 CSEing the three loads into one lea. */
extern volatile int32_t g_br094294; /* 0x10094294 */
extern uint8_t g_aBr0B37D0[];       /* 0x100B37D0 */
extern uint8_t g_brAD0854[3];       /* 0x10AD0854 */

void BrPalFetch(const uint8_t *pTable, int32_t index, uint8_t aOut[3])
{
    int i0, i1, i2, b0, b1, b2;

    i0 = g_br094294;
    i1 = g_br094294;
    b2 = g_aBr0B37D0[i0 * 3 + 2];
    i2 = g_br094294;
    b1 = g_aBr0B37D0[i1 * 3 + 1];
    b0 = g_aBr0B37D0[i2 * 3];
    g_brAD0854[2] = (uint8_t)b2;
    g_brAD0854[1] = (uint8_t)b1;
    g_brAD0854[0] = (uint8_t)b0;
}
#else
void BrPalFetch(const uint8_t *pTable, int32_t index, uint8_t aOut[3])
{
    const uint8_t *p = pTable + (ptrdiff_t)index * 3;

    aOut[2] = p[2];
    aOut[1] = p[1];
    aOut[0] = p[0];
}
#endif
