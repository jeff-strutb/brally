/* br_dik.c -- the per-frame keyboard poll and its edge detection.
 *
 * RESPONSIBILITY: reading what the player is doing -- pulling the DirectInput
 * keyboard state in and turning it into "pressed just now" answers.
 *
 * Moved here out of the address batches under src/core/; the bodies are the
 * text that was matched there, unchanged.
 */
#include "slice3_39.h"

/* WHAT IT DOES: reports which key was newly pressed this frame, taking the
 * first one it finds, or -1 if none were. This is how a "press any key"
 * prompt is answered. */
/* @implements 0x1005FFD0 d3d BrFn1005FFD0 */
int32_t BrFn1005FFD0(void)
{
    int32_t i;

    for (i = 0; i < BR_DIK_COUNT; ++i) {
        if (g_BrDikEdge[i] != 0) {
            return i;
        }
    }
    return -1;
}

/* WHAT IT DOES: read the keyboard and, if that succeeded, work out which
 * keys changed since last time. The per-frame input poll; a failed read
 * leaves the previous state alone rather than reporting everything as
 * released. */
/* @implements 0x10059020 glide BrDikPollAndEdge */
void BrDikPollAndEdge(void)
{
    if (BrDikGetDeviceState(g_BrDikState) >= 0) {
        BrMenuSub1005FF60();
    }
}

/* WHAT IT DOES: refreshes both sets of input edges for this frame --
 * keyboard keys and controller buttons -- so the menus can tell a fresh
 * press from a held one. */
/* @implements 0x1003E070 d3d BrFn1003E070 */
void BrFn1003E070(void)
{
    BrMenuSub1005FF60();
    BrMenuSub1005FFF0();
}

