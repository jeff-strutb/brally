/* br_flags.h: the port's behaviour flags, and the two profiles that set them.
 *
 * Every place where the port can behave either as the original game did or
 * in a way the original could not (a window of any shape, music that comes
 * back after a focus change) is one flag here. A profile sets them all:
 *
 *   original    the game as it shipped: a 4:3 picture, the original's own
 *               behaviour where it differs
 *   remastered  the port's improvements on
 *
 * BR_PROFILE=original|remastered picks the profile to start in (remastered
 * when unset); BR_FLAG_<NAME>=0|1 then overrides one flag, e.g.
 * BR_FLAG_ANY_ASPECT=0.  In play the Toggle Remaster control (Tab unless
 * rebound on the Game Key Configuration page, br_ctrltoggle.h) switches
 * between the two.
 * flags.c holds the table; add a flag there and here together. */
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
    /* the front end's music resumes when the window gets the focus back.
     * Off: the original, which paused it and resumed only in a race. */
    BR_FLAG_MENU_MUSIC_RESUME,
    BR_FLAG_COUNT
};

int plat_flag(int flag);                /* flags.c: 1 when on */
const char *plat_profile(void);         /* "original" or "remastered" */
void plat_profile_toggle(void);         /* the other profile, now (Toggle Remaster) */

#ifdef __cplusplus
}
#endif
#endif
