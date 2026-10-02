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
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include "slice3_41.h"   /* br_globals: its objects */
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
/* The original takes no arguments: index is 0x10094294, table is
 * 0x100B37D0, dest is 0x10AD0854.  The port signature is the header's.
 * volatile on the index stops VC5 CSEing the three loads into one lea. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */ /* 0x10094294 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */       /* 0x100B37D0 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */       /* 0x10AD0854 */

void BrPalFetch(const uint8_t *pTable, int32_t index, uint8_t aOut[3])
{
    int i0, i1, i2, b0, b1, b2;

    i0 = (*(int32_t *)&g_id);
    i1 = (*(int32_t *)&g_id);
    b2 = g_aBr0B37D0[i0 * 3 + 2];
    i2 = (*(int32_t *)&g_id);
    b1 = g_aBr0B37D0[i1 * 3 + 1];
    b0 = g_aBr0B37D0[i2 * 3];
    (*(uint8_t (*)[3])&g_aBrRaceCar[0].f29AC)[2] = (uint8_t)b2;
    (*(uint8_t (*)[3])&g_aBrRaceCar[0].f29AC)[1] = (uint8_t)b1;
    (*(uint8_t (*)[3])&g_aBrRaceCar[0].f29AC)[0] = (uint8_t)b0;
}
