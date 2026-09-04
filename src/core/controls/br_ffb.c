/* br_ffb.c -- what the wheel pushes back with.
 *
 * RESPONSIBILITY: reading what the player is doing -- and the other half of
 * that conversation, the force-feedback effect the wheel is asked to play.
 * The three setters here only REMEMBER a choice; the commit that sends it is
 * still in src/core/slice3_45.c (it needs that file's DirectInput vtable cast
 * helpers).  The re-probe below is the wheel-detection pass.
 *
 * Moved here out of the address batches src/core/slice3_45.c and
 * src/core/slice6_77.c.
 */
#include "slice2_25.h"   /* g_brB4E1D0/D4/E0, g_aBrB4DF30 and its stride  */
#include "slice3_45.h"   /* BrFfbInit (0x100791D0), g_brFfb; pulls in
                          * slice1_10.h for BrFfbShutdown (0x10079550)     */

/* 0x10078E10 */
/* WHAT IT DOES: chooses which way the next shake of a force-feedback wheel
 * will push. It is remembered rather than sent, taking effect the next time
 * the effect is committed, and it does nothing at all unless force feedback
 * is switched on and a suitable wheel is attached. */
/* @implements 0x10078E10 d3d BrFfbSetDirection */
void BrFfbSetDirection(int32_t dir)
{
    if (g_brB4E1D0 != 1 && g_brB4E1D0 != 2) {
        return;
    }
    if (g_brB4E1E0 == 0) {
        return;
    }
    if (g_br18ABDBC == 0) {
        return;
    }
    if (g_brFlag6909E0 != 0) {
        return;
    }
    g_br0BD430[0] = dir;
}

/* 0x10078E50 */
/* WHAT IT DOES: asks for the next shake of the wheel to be the long one --
 * a quarter of a second. Like the direction it is only remembered, and only
 * when force feedback is actually available. */
/* @implements 0x10078E50 d3d BrFfbSetDurationLong */
void BrFfbSetDurationLong(void)
{
    if (g_brB4E1D0 != 1 && g_brB4E1D0 != 2) {
        return;
    }
    if (g_brB4E1E0 == 0) {
        return;
    }
    if (g_br18ABDBC == 0) {
        return;
    }
    if (g_brFlag6909E0 != 0) {
        return;
    }
    g_br0BD438 = 0x3D090;   /* 250000 us */
}

/* 0x10078E90 */
/* WHAT IT DOES: the same, for the short shake -- an eighth of a second. */
/* @implements 0x10078E90 d3d BrFfbSetDurationShort */
void BrFfbSetDurationShort(void)
{
    if (g_brB4E1D0 != 1 && g_brB4E1D0 != 2) {
        return;
    }
    if (g_brB4E1E0 == 0) {
        return;
    }
    if (g_br18ABDBC == 0) {
        return;
    }
    if (g_brFlag6909E0 != 0) {
        return;
    }
    g_br0BD438 = 0x1E848;   /* 125000 us */
}

/* WHAT IT DOES: re-detects the force-feedback wheel. It forces a known
 * configuration, runs the force-feedback setup and immediately tears it down
 * again -- the probe is the setup attempt itself -- and then restores the
 * settings the player had, selecting the matching device record on the way
 * back. */
/* @implements 0x100795D0 d3d BrFfbReprobe */
#ifdef BR_MATCHING_BUILD
/* The restore chain is a real switch: each arm stores its record ADDRESS
 * as an immediate and restores the exclusive flag itself (arms in memory
 * order default,3,2,1; case 1 restores the flag before the pointer). */
extern void BrExt_10079550(void);   /* glide 0x10072840, 0 args */

void BrFfbReprobe(void)
{
    int32_t nSavedMode = g_brB4E1D0;   /* esi */
    int32_t nSavedExcl = g_brB4E1E0;   /* edi */

    g_brB4E1D0 = 2;
    g_brB4E1D4 = g_aBrB4DF30[2];
    g_brB4E1E0 = 1;

    (void)BrFfbInit();
    BrExt_10079550();

    g_brB4E1D0 = nSavedMode;

    switch (nSavedMode) {
    default:
        g_brB4E1D4 = g_aBrB4DF30[0];
        g_brB4E1E0 = nSavedExcl;
        return;
    case 3:
        g_brB4E1D4 = g_aBrB4DF30[3];
        g_brB4E1E0 = nSavedExcl;
        return;
    case 2:
        g_brB4E1D4 = g_aBrB4DF30[2];
        g_brB4E1E0 = nSavedExcl;
        return;
    case 1:
        g_brB4E1E0 = nSavedExcl;
        g_brB4E1D4 = g_aBrB4DF30[1];
        return;
    }
}
#else
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
#endif
