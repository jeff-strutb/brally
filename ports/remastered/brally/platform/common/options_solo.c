/* options_solo.c: the Remaster Options for Boss Rally built alone (HOST null,
 * macos, windows: the lighting's checks and shots), where the app's own
 * (ports/remastered/platform/options.c) is not linked.  Each is its default
 * (widescreen on, no frame rate; the lighting and textures shelved, off),
 * RM_OPT_<NAME>=N sets one; a choice on the pause menu's page applies at
 * once and is not kept. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "br_flags.h"

/* (the order of the app's options.h, as pause_options.c has it) */
enum { RM_OPT_WIDESCREEN, RM_OPT_MUSIC, RM_OPT_LIGHTING, RM_OPT_FPS, RM_OPT_TEXTURES, RM_OPT_COUNT };

static const struct { const char *env; int def, flag; } k_opt[RM_OPT_COUNT] = {
    [RM_OPT_WIDESCREEN] = { "RM_OPT_WIDESCREEN", 1, BR_FLAG_ANY_ASPECT },
    [RM_OPT_MUSIC]      = { "RM_OPT_MUSIC", 1, -1 },            /* the N64's: no such music alone */
    [RM_OPT_LIGHTING]   = { "RM_OPT_LIGHTING", 0, BR_FLAG_LIGHTING },     /* shelved */
    [RM_OPT_FPS]        = { "RM_OPT_FPS", 0, -1 },
    [RM_OPT_TEXTURES]   = { "RM_OPT_TEXTURES", 0, BR_FLAG_TEXTURES },     /* shelved */
};

static int s_val[RM_OPT_COUNT], s_loaded;

static void load(void)
{
    int i;
    for (i = 0; i < RM_OPT_COUNT; i++) {
        const char *e = getenv(k_opt[i].env);
        s_val[i] = e ? atoi(e) : k_opt[i].def;
    }
    s_loaded = 1;
}

int rm_option(int which)
{
    if (!s_loaded)
        load();
    return which >= 0 && which < RM_OPT_COUNT ? s_val[which] : 0;
}

/* the flags themselves start from BR_FLAG_<NAME> (flags.c); only a choice
 * made on the page moves them here */
void rm_option_set(int which, int value)
{
    if (!s_loaded)
        load();
    if (which < 0 || which >= RM_OPT_COUNT)
        return;
    s_val[which] = value;
    if (k_opt[which].flag >= 0)
        br_flag_set(k_opt[which].flag, value != 0);
    if (getenv("RM_LOG"))
        fprintf(stderr, "remastered: option %d %d\n", which, value);
}
