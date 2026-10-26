/* br_flags.h: Remastered's switches in Boss Rally (flags.c).
 *
 * Each place where Top Gear Rally Remastered draws Boss Rally's races in a way
 * the original could not is one switch here, on until the app's Remaster
 * Options set it (ports/remastered/platform/options.c, through br_flag_set);
 * BR_FLAG_<NAME>=0|1 sets one at start (checks), e.g. BR_FLAG_ANY_ASPECT=0.
 * The lighting is shelved: off unless its switch is set. */
#ifndef BR_FLAGS_H
#define BR_FLAGS_H

#ifdef __cplusplus
extern "C" {
#endif

enum br_flag {
    /* any window shape: the race fills the window, its camera widened (wider
     * than 4:3 sees more to the sides, taller sees more above and below) and
     * the HUD kept at its own shape at its edges. Off: the original's 4:3
     * picture, letterboxed or pillarboxed. */
    BR_FLAG_ANY_ASPECT,
    /* the modern lighting pipeline (platform/light, docs/brally/remaster/
     * lighting/DESIGN.md): the 3D scene drawn physically lit from what the
     * game's display lists describe, the HUD and menus laid over it as the
     * game draws them. Off: the original's picture. Switched live; the
     * game's simulation never sees it. */
    BR_FLAG_LIGHTING,
    /* View Distance Unlimited (rm_view.h): every view's far distance past
     * the whole world, so nothing pops in at the original's 400. Off: the
     * original's distance. */
    BR_FLAG_VIEW_UNLIMITED,
    BR_FLAG_COUNT
};

int  plat_flag(int flag);               /* flags.c: 1 when on */
void br_flag_set(int flag, int on);     /* the app's: on or off, now */

#ifdef __cplusplus
}
#endif
#endif
