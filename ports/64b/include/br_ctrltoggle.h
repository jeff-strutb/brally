/* br_ctrltoggle.h -- port: the "Toggle Remaster" control (br_ctrltoggle.c).
 *
 * A 29th control action, bound like the game's own 28 and listed last on the
 * Game Key Configuration page: pressed, it switches between the Original and
 * Remastered profiles (br_flags.h).  Tab by default.  Not in the original.
 *
 * The 28 actions' bindings sit in one flat table the original saves as it
 * is (BossRally.cfg) and indexes across its four device profiles, so this
 * one cannot join it: it has its own record per profile, in the same three
 * 16-bit words (primary, two keyboard alternates), which the binding
 * accessors serve for this action and BossRally.cfg carries after the
 * original's fields, where the original's reader never looks. */
#ifndef BR_CTRLTOGGLE_H
#define BR_CTRLTOGGLE_H

#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BR_ACT_TOGGLE_REMASTER  28          /* past the original's 0..27 */
#define BR_STR_TOGGLE_REMASTER  0x7000      /* its label: not a string-table id */

extern uint16_t g_aBrToggleBind[4][3];      /* per device profile */

/* the binding record (3 words) of profile `kind`, or of the active profile
 * when kind < 0 -- the same shape as an action's in BrCtrlProfile */
uint16_t   *BrToggleBindRec(int kind);
/* BrCtrlCfgAssign for this action: primary set, alternates back to their
 * defaults unless that key is some action's primary in the profile */
void        BrToggleBindAssign(const void *cfg, int kind, int key, int mod);
void        BrToggleBindDefaults(int kind);  /* kind < 0: every profile */
const char *BrToggleLabel(void);
/* once a frame: switches the profile on the press */
void        BrTogglePoll(void);
/* BossRally.cfg: after the original's fields */
int         BrToggleBindWrite(FILE *f);
void        BrToggleBindRead(FILE *f);     /* absent (an original file): defaults */

#ifdef __cplusplus
}
#endif
#endif
