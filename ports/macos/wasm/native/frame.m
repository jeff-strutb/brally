/* frame.m -- the port-owned frame loop (ports/macos/NATIVE_RENDERER.md 4.1).
 *
 * Native overrides of the two game functions the loop is timed through: the
 * swap (every frame's one grBufferSwap) and the millisecond clock the race's
 * time-sync loop reads.  For now both behave exactly as the translated
 * bodies do; they are the seam the display-timed loop takes over.
 */
#include "w2c_native.h"

u32 h_grBufferNumPending(void);
void h_grBufferSwap(u32 interval);

/* WHY: every presented frame goes through here, so frame pacing is decided
 * here.  Same calls as the game's own body (br_dlglide.c): wait for Glide's
 * pending buffers to drain, then swap on the next retrace. */
/* @replaces 0x1001DD50 BrGlideFlipWait */
void n_BrGlideFlipWait(void)
{
    W_TRACE("n_BrGlideFlipWait");
    while ((s32)h_grBufferNumPending() > 0)
        ;
    h_grBufferSwap(1);
}

/* WHY: the race's time-sync loop (BrRaceStep 0x1001C5EE) and the snapshot
 * blend read game time here; the frame loop will answer it from the target
 * presentation time.  Until then, the game's own clock. */
/* @replaces 0x1006E280 BrSub10075020 */
u32 n_BrSub10075020(void)
{
    W_TRACE("n_BrSub10075020");
    return W_ORIG_BrSub10075020();
}
