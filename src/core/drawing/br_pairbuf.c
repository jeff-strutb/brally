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

#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
/* The original BrOptSave takes no arguments (loose globals in, packed
 * array out); hide the header's port prototype behind a rename so the
 * matching twin can define the real symbol -- the slice5_63.c caller keeps
 * the port signature (cdecl, extra args harmless at run time). */
#define BrOptSave   BrOptSave_hdr
#define BrOptAvailB BrOptAvailB_hdr
#ifdef BR_MATCHING_BUILD
/* The original BrNameListInit is a thiscall ctor with no stack args (vtbl
 * and fill string are fixed); hide the port's 3-arg prototype. */
#define BrNameListInit BrNameListInit_port
#include "slice1_06.h"
#undef BrNameListInit
#else
#include "slice1_06.h"
#endif
#undef BrOptSave
#undef BrOptAvailB
#else
#include "slice1_06.h"
#endif

#include <stdlib.h>
#include <string.h>

/* Layout facts the original's arithmetic depends on. */
typedef char br06_assert_pendlist[
    (offsetof(BrPendList, count) == BR_PENDLIST_MAX * sizeof(void *)) ? 1 : -1];
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
#ifdef BR_MATCHING_BUILD
uint32_t *g_brPairA;                         /* 0x10ACED34 */
uint32_t *g_brPairB;                         /* 0x10AD189C */
uint32_t  g_brPairStaticA[BR_PAIRBUF_DWORDS]; /* 0x10AF9890 */
uint32_t  g_brPairStaticB[BR_PAIRBUF_DWORDS]; /* 0x10AF99DC */

/* WHAT IT DOES: wipe a pair of scratch buffers back to zeros, pointing
 * each at its own built-in storage first if it has nowhere else to live. */
/* @implements 0x1003E1D0 d3d BrPairBufReset */
int BrPairBufReset(BrPairBuf *pBuf)
{
    uint32_t *p;

    p = g_brPairA;
    if (p == NULL) {
        p = g_brPairStaticA;
        g_brPairA = p;
    }
    memset(p, 0, BR_PAIRBUF_DWORDS * sizeof(uint32_t));

    p = g_brPairB;
    if (p == NULL) {
        p = g_brPairStaticB;
        g_brPairB = p;
    }
    memset(p, 0, BR_PAIRBUF_DWORDS * sizeof(uint32_t));

    return 1;
}
#else
/* WHAT IT DOES: wipe a pair of scratch buffers back to zeros, pointing
 * each at its own built-in storage first if it has nowhere else to live. */
/* port-only variant of BrPairBufReset (matching build uses the #ifdef branch above) */
int BrPairBufReset(BrPairBuf *pBuf)
{
    if (pBuf->pA == NULL) {
        pBuf->pA = pBuf->aStaticA;
    }
    memset(pBuf->pA, 0, BR_PAIRBUF_DWORDS * sizeof(uint32_t));

    if (pBuf->pB == NULL) {
        pBuf->pB = pBuf->aStaticB;
    }
    memset(pBuf->pB, 0, BR_PAIRBUF_DWORDS * sizeof(uint32_t));

    return 1;
}
#endif
