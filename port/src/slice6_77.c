/* slice6_77.c -- see slice6_77.h for the identification of both functions,
 * the evidence for the storage they use, and the gotchas. */
#include "slice6_77.h"

#include "br_slots.h"    /* BrSlotsResetArray -- the loop half of 0x100586A0 */
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
    /* The original clears the counter FIRST, before the loop. Kept in that
     * order; nothing here can observe the difference, but the two writes go to
     * objects 0x2F4 bytes apart and the sequence is what the disassembly says.
     *
     * 0x10AA288C is the DirectPlay send gate as well as this table's counter
     * -- see the warning at the foot of br_slots.h. Clearing it here re-opens
     * that gate, which is behaviour of the original, not of this port. */
    g_brAA288C = 0;
    BrSlotsResetArray(g_aBrAA2538);
}

/* ==========================================================================
 * 0x100795D0
 * ========================================================================== */

/* WHAT IT DOES: re-detects the force-feedback wheel. It forces a known
 * configuration, runs the force-feedback setup and immediately tears it down
 * again -- the probe is the setup attempt itself -- and then restores the
 * settings the player had, selecting the matching device record on the way
 * back. */
/* @implements 0x100795D0 d3d BrFfbReprobe */
void BrFfbReprobe(void)
{
    int32_t nSavedMode = g_brB4E1D0;   /* esi */
    int32_t nSavedExcl = g_brB4E1E0;   /* edi */

    /* 0x100795DE..0x100795F2 -- force the known probe configuration: mode 2,
     * record 2 (0x10B4E080), exclusive. */
    g_brB4E1D0 = 2;
    g_brB4E1D4 = g_aBrB4DF30[2];
    g_brB4E1E0 = 1;

    /* 0x100795FC / 0x10079601. BrFfbInit's result is discarded by the
     * original (it does not even keep eax past the next call), so the
     * "disabled vs already up" distinction its header describes is not used
     * here. BrFfbShutdown then unwinds the nested-init count this raised. */
    (void)BrFfbInit();
    BrFfbShutdown(&g_brFfb);

    /* 0x10079606 -- the mode is restored before the selection chain, because
     * the chain decrements the register that held it. */
    g_brB4E1D0 = nSavedMode;

    /* 0x1007960C..0x1007965F -- `dec/je` three times over 1, 2, 3 with
     * everything else falling through to record 0. Written the way
     * slice2_25.c writes the same chain for 0x10043400, so the two sites read
     * as the one idiom they are. Mode 0 and any mode > 3 both select 0. */
    if (nSavedMode >= 1 && nSavedMode <= 3) {
        g_brB4E1D4 = g_aBrB4DF30[nSavedMode];
    } else {
        g_brB4E1D4 = g_aBrB4DF30[0];
    }

    g_brB4E1E0 = nSavedExcl;
}
