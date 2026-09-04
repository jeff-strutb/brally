/* slice6_77.c -- see slice6_77.h for the identification of both functions,
 * the evidence for the storage they use, and the gotchas. */
#include "slice6_77.h"

#include "br_slots.h"    /* BrSlot */
#include "slice2_25.h"   /* g_aBrAA2538, g_brAA288C, g_brB4E1D0/D4/E0,
                          * g_aBrB4DF30 and its stride/count                 */
#include "slice3_45.h"   /* BrFfbInit (0x100791D0), g_brFfb; pulls in
                          * slice1_10.h for BrFfbShutdown (0x10079550)       */

/* ==========================================================================
 * 0x100586A0
 * ========================================================================== */

/* WHAT IT DOES: clears the player slot table and its counter at the start or
 * end of a session. Beware that the counter it clears doubles as the gate
 * that permits network sends, so clearing it here re-opens that gate --
 * behaviour of the original, not of this port. */
/* @implements 0x100586A0 d3d BrSub100586A0 */
void BrSub100586A0(void)
{
    /* Cursor on field `a` (0x10AA253C): stores are [eax-4]/[eax]/[eax+4],
     * bound 0x10AA259C. Zero is reused from edx; -1 from ecx. The original
     * compare is signed (`jl`). */
    int32_t *p;
    int32_t nZero;
    int32_t nEmpty;

    nZero = 0;
    p = (int32_t *)((char *)g_aBrAA2538 + 4);
    g_brAA288C = nZero;
    nEmpty = -1;
    do {
        p[-1] = nEmpty;
        p[0]  = nZero;
        p[1]  = nZero;
        p += 3;
#ifdef BR_MATCHING_BUILD
    } while ((int32_t)p < (int32_t)((char *)g_aBrAA2538 + 0x64));
#else
    } while (p < (int32_t *)((char *)g_aBrAA2538 + 0x64));
#endif
}

/* ── Ghidra-matched functions ─────────────────────────── */
#ifdef BR_MATCHING_BUILD
/* The DLL entry point, the two CRT-region nops and traps, the matrix magic
 * check and the CRT exit-handler glue (0x10073714, 0x10073719, 0x10073974,
 * 0x10073979, 0x100745B0, 0x100745E0, 0x100747E0, 0x10074B00) now live in
 * src/core/startup/br_dllentry.c. */
#endif /* BR_MATCHING_BUILD */
