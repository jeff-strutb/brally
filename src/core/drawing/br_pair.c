/* br_pair.c -- drawing.
 *
 * Filed out of the address batches: these functions were
 * matched first and grouped by what they are afterwards.
 * Every function carries its original address.
 */
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import
 * table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include <stdint.h>

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

#endif /* BR_MATCHING_BUILD */
