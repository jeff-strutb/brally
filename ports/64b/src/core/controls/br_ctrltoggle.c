/* br_ctrltoggle.c -- port: the "Toggle Remaster" control.  See br_ctrltoggle.h.
 * Not in the original. */
#include <string.h>
#include "br_coretypes.h"
#include "br_ctrltoggle.h"
#include "br_flags.h"

#define DIK_TAB 0x0F

/* the keyboard profile has Tab as the primary; the joystick and mouse ones
 * keep it as a keyboard alternate, so Tab works whatever the device */
static const uint16_t k_aDefault[4][3] = {
    { DIK_TAB, 0, 0 }, { 0, DIK_TAB, 0 }, { 0, DIK_TAB, 0 }, { 0, DIK_TAB, 0 },
};

uint16_t g_aBrToggleBind[4][3] = {
    { DIK_TAB, 0, 0 }, { 0, DIK_TAB, 0 }, { 0, DIK_TAB, 0 }, { 0, DIK_TAB, 0 },
};

uint16_t *BrToggleBindRec(int kind)
{
    if (kind < 0)
        kind = g_BrCtrlCfg.active;
    return g_aBrToggleBind[kind & 3];
}

void BrToggleBindAssign(const void *cfg, int kind, int key, int mod)
{
    const BrCtrlCfg *c = (const BrCtrlCfg *)cfg;
    uint16_t *r = BrToggleBindRec(kind & 3);
    int s, a;
    /* as BrCtrlCfgAssign builds the word: the class from `key`, the code from `mod` */
    r[0] = (uint16_t)((unsigned char)(mod ^ key) ^ key);
    for (s = 1; s < 3; s++) {
        r[s] = k_aDefault[kind & 3][s];
        for (a = 0; a < BR_CTRL_ACTIONS; a++)
            if (c->profile[kind & 3].e[a][0] == r[s]) {
                r[s] = 0;
                break;
            }
        if (r[s] == r[0])
            r[s] = 0;
    }
}

void BrToggleBindDefaults(int kind)
{
    if (kind < 0)
        memcpy(g_aBrToggleBind, k_aDefault, sizeof g_aBrToggleBind);
    else
        memcpy(g_aBrToggleBind[kind & 3], k_aDefault[kind & 3], sizeof g_aBrToggleBind[0]);
}

/* the list's text boxes hold about ten characters (ACCELERATE is the longest) */
const char *BrToggleLabel(void) { return "REMASTER"; }

void BrTogglePoll(void)
{
    /* the binding's own edge, sampled here: the game's previous-frame key
     * buffers advance at their own pace, so one press is one toggle */
    static int s_was;
    int down = BrInputIsDown(BR_ACT_TOGGLE_REMASTER) != 0;
    if (down && !s_was)
        plat_profile_toggle();
    s_was = down;
}

static const char k_magic[4] = { 'R', 'M', 'T', '1' };

int BrToggleBindWrite(FILE *f)
{
    return fwrite(k_magic, 4, 1, f) == 1 && fwrite(g_aBrToggleBind, sizeof g_aBrToggleBind, 1, f) == 1;
}

void BrToggleBindRead(FILE *f)
{
    char m[4];
    uint16_t t[4][3];
    if (fread(m, 4, 1, f) == 1 && memcmp(m, k_magic, 4) == 0 && fread(t, sizeof t, 1, f) == 1)
        memcpy(g_aBrToggleBind, t, sizeof t);
    else
        BrToggleBindDefaults(-1);
}
