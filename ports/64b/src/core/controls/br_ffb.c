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
#include "slice3_42.h"   /* br_globals: its objects */
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
    if ((*(int32_t *)&g_BrCtrlCfg.active) != 1 && (*(int32_t *)&g_BrCtrlCfg.active) != 2) {
        return;
    }
    if ((*(int32_t *)&DAT_10b71540) == 0) {
        return;
    }
    if ((*(int32_t *)&DAT_118eeed4) == 0) {
        return;
    }
    if ((*(int *)&DAT_105ccb68[8]) != 0) {
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
    if ((*(int32_t *)&g_BrCtrlCfg.active) != 1 && (*(int32_t *)&g_BrCtrlCfg.active) != 2) {
        return;
    }
    if ((*(int32_t *)&DAT_10b71540) == 0) {
        return;
    }
    if ((*(int32_t *)&DAT_118eeed4) == 0) {
        return;
    }
    if ((*(int *)&DAT_105ccb68[8]) != 0) {
        return;
    }
    g_br0BD438 = 0x3D090;   /* 250000 us */
}

/* 0x10078E90 */
/* WHAT IT DOES: the same, for the short shake -- an eighth of a second. */
/* @implements 0x10078E90 d3d BrFfbSetDurationShort */
void BrFfbSetDurationShort(void)
{
    if ((*(int32_t *)&g_BrCtrlCfg.active) != 1 && (*(int32_t *)&g_BrCtrlCfg.active) != 2) {
        return;
    }
    if ((*(int32_t *)&DAT_10b71540) == 0) {
        return;
    }
    if ((*(int32_t *)&DAT_118eeed4) == 0) {
        return;
    }
    if ((*(int *)&DAT_105ccb68[8]) != 0) {
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
/* The restore chain is a real switch: each arm stores its record ADDRESS
 * as an immediate and restores the exclusive flag itself (arms in memory
 * order default,3,2,1; case 1 restores the flag before the pointer). */
/* BrExt_10079550: prototype in br_funcs.h */

void BrFfbReprobe(void)
{
    int32_t nSavedMode = (*(int32_t *)&g_BrCtrlCfg.active);   /* esi */
    int32_t nSavedExcl = (*(int32_t *)&DAT_10b71540);   /* edi */

    (*(int32_t *)&g_BrCtrlCfg.active) = 2;
    (*(void * *)&g_BrPadModeBytes) = (*(unsigned char (*)[4][168])&g_BrCtrlCfg)[2];
    (*(int32_t *)&DAT_10b71540) = 1;

    (void)BrFfbInit();
    BrExt_10079550();

    (*(int32_t *)&g_BrCtrlCfg.active) = nSavedMode;

    switch (nSavedMode) {
    default:
        (*(void * *)&g_BrPadModeBytes) = (*(unsigned char (*)[4][168])&g_BrCtrlCfg)[0];
        (*(int32_t *)&DAT_10b71540) = nSavedExcl;
        return;
    case 3:
        (*(void * *)&g_BrPadModeBytes) = (*(unsigned char (*)[4][168])&g_BrCtrlCfg)[3];
        (*(int32_t *)&DAT_10b71540) = nSavedExcl;
        return;
    case 2:
        (*(void * *)&g_BrPadModeBytes) = (*(unsigned char (*)[4][168])&g_BrCtrlCfg)[2];
        (*(int32_t *)&DAT_10b71540) = nSavedExcl;
        return;
    case 1:
        (*(int32_t *)&DAT_10b71540) = nSavedExcl;
        (*(void * *)&g_BrPadModeBytes) = (*(unsigned char (*)[4][168])&g_BrCtrlCfg)[1];
        return;
    }
}
