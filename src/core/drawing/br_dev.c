/* br_dev.c -- drawing.
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


/* ==========================================================================
 * 0x10037070
 * ========================================================================== */

/* WHAT IT DOES: answers whether any of a fixed set of records -- reached
 * through an index table rather than in order -- is both in use, of one
 * particular kind, and already holding the value being asked about. It reads
 * like a "is this already taken?" test for input-device assignments, but
 * nothing in this packet confirms that, so treat the purpose as unconfirmed. */
BrDevCtx *g_brP6EECCC;                      /* 0x106EECCC */

/* @implements 0x10037070 d3d BrDevRecMatch */
int BrDevRecMatch(uint32_t value)
{
    int32_t i;

    for (i = 0; i < BR_DEVREC_SLOTS; i++) {
        const BrDevRec *pRec = &g_brP6EECCC->pRecs[g_brP6EECCC->abIndex[i]];

        if (pRec->f04 == 0u) {
            continue;
        }
        if ((pRec->f20 & BR_DEVREC_TYPE_MASK) != BR_DEVREC_TYPE_MATCH) {
            continue;
        }
        if (pRec->f04 == value) {
            return 1;
        }
    }
    return 0;
}

#endif /* BR_MATCHING_BUILD */
