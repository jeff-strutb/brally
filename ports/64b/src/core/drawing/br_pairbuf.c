/* br_pairbuf.c -- drawing: the pair of scratch buffers.
 *
 * RESPONSIBILITY: drawing/ -- turn geometry and images into pixels.
 *
 * Filed out of slice1_06.c, an address batch and not a module.  What the two
 * buffers hold is NOT established -- the reset below is all that has been
 * read off the original -- so this file says only what can be proved and the
 * name is deliberately descriptive of the shape rather than of a purpose.
 *
 * slice1_06.c's preamble is carried over verbatim: an include set that looks
 * redundant has already been shown elsewhere in this module to move VC5's
 * register allocation (see br_rdpmode.c).
 */

/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
/* The original BrOptSave takes no arguments (loose globals in, packed
 * array out); hide the header's port prototype behind a rename so the
 * matching twin can define the real symbol -- the slice5_63.c caller keeps
 * the port signature (cdecl, extra args harmless at run time). */
#define BrOptSave   BrOptSave_hdr
#define BrOptAvailB BrOptAvailB_hdr
/* The original BrNameListInit is a thiscall ctor with no stack args (vtbl
 * and fill string are fixed); hide the port's 3-arg prototype. */
#define BrNameListInit BrNameListInit_port
#include "slice3_41.h"   /* br_globals: its objects */
#include "slice1_06.h"
#undef BrNameListInit
#undef BrOptSave
#undef BrOptAvailB

#include <stdlib.h>
#include <string.h>

/* Layout facts the original's arithmetic depends on. */
/* BR_PENDLIST_MAX: br_coretypes.h */
typedef char br06_assert_devrec[
    (sizeof(BrDevRec) == BR_DEVREC_STRIDE) ? 1 : -1];
typedef char br06_assert_namelist[
    (BR_NAMELIST_COUNT * BR_NAMELIST_STRIDE == 0x1964 * 4) ? 1 : -1];

/* ==========================================================================
 * 0x1003E1D0
 * ========================================================================== */

/* WHAT IT DOES: wipes a pair of scratch buffers back to zeros, pointing each
 * at its own built-in storage first if it has not been given anywhere else to
 * live. What the buffers hold is not established here. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                         /* 0x10ACED34 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                         /* 0x10AD189C */
/* 64-bit core: declared once, in br_globals.h or its struct's header */ /* 0x10AF9890 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */ /* 0x10AF99DC */

/* WHAT IT DOES: wipe a pair of scratch buffers back to zeros, pointing
 * each at its own built-in storage first if it has nowhere else to live. */
/* @implements 0x1003E1D0 d3d BrPairBufReset */
int BrPairBufReset(BrPairBuf *pBuf)
{
    uint32_t *p;

    p = (*(uint32_t * *)((char *)&g_aBrRaceCar + 0xE8C)) /* BR_LP64_BYTE_VIEW */;
    if (p == NULL) {
        p = g_brPairStaticA;
        (*(uint32_t * *)((char *)&g_aBrRaceCar + 0xE8C)) /* BR_LP64_BYTE_VIEW */ = p;
    }
    memset(p, 0, BR_PAIRBUF_DWORDS * sizeof(uint32_t));

    p = (*(uint32_t * *)((char *)&g_aBrRaceCar + 0x39F4)) /* BR_LP64_BYTE_VIEW */;
    if (p == NULL) {
        p = g_brPairStaticB;
        (*(uint32_t * *)((char *)&g_aBrRaceCar + 0x39F4)) /* BR_LP64_BYTE_VIEW */ = p;
    }
    memset(p, 0, BR_PAIRBUF_DWORDS * sizeof(uint32_t));

    return 1;
}
