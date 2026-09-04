/* br_rcaswap.c -- gamedata: turning loaded N64-order data the right way round.
 *
 * The byte-order flips and the in-place pointer rebase that every .rca and
 * track blob goes through as it is read in. Filed out of slice2_16.c.
 *
 * See slice2_16.h for the per-function notes and gotchas.
 */
#ifdef BR_MATCHING_BUILD
/* The original binary is /MD: CRT calls resolve through the import table. */
#define _CRTIMP __declspec(dllimport)
#endif
#include "slice2_16.h"

/* 0x10018B60 */
/* The in-place N64->host pointer rebase the record fixup calls three times:
 * glide 0x100189E0, 43 bytes.  Zero stays zero; anything below the N64 load
 * base (SIGNED compare) becomes zero; the rest is rebased onto the host
 * image.  br_ai.c's static br_ai_reloc also claims this VA with a
 * by-value shape -- that claim is stale and should be retired when br_ai
 * is next touched. */
/* WHAT IT DOES: rewrite one pointer inside freshly loaded N64-format data so
 * it points at where the block actually landed in memory. A null stays null,
 * and anything pointing BELOW the block's own base is not a real pointer at
 * all and is zeroed rather than rebased into nonsense. The bases come from
 * BrSegSetBases. */
/* @implements 0x100189E0 glide BrSegPtrFixup */
void BrSegPtrFixup(uint32_t *p)
{
    if (*p == 0)
        return;
    if ((int32_t)*p < g_brSegN64Base) {
        *p = 0;
        return;
    }
    *p = (uint32_t)(g_brSegHostBase - g_brSegN64Base) + *p;
}
