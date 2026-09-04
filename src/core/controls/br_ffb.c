/* br_ffb.c -- what the wheel pushes back with.
 *
 * RESPONSIBILITY: reading what the player is doing -- and the other half of
 * that conversation, the force-feedback effect the wheel is asked to play.
 * The three setters here only REMEMBER a choice; the commit that sends it is
 * still in src/core/slice3_45.c (it needs that file's DirectInput vtable cast
 * helpers).
 *
 * Moved here out of src/core/slice3_45.c (an address batch, not a module).
 */
#include "slice3_45.h"

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
