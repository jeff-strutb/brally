/* options.h: Top Gear Rally Remastered's Remaster Options (options.c): what
 * the player chooses on Top Gear Rally's options screen
 * (ports/remastered/tgrally/src/menus/rmoptions.c) or the page of that name in
 * a race's pause menu (ports/remastered/brally/platform/common/pause_options.c),
 * applied to both games at once and kept in the app's save folder. */
#ifndef RM_OPTIONS_H
#define RM_OPTIONS_H

enum {
    RM_OPT_WIDESCREEN,              /* 1: a race fills the window (0: the games' 4:3) */
    RM_OPT_MUSIC,                   /* RM_MUSIC_*: which soundtrack plays */
    RM_OPT_LIGHTING,                /* shelved: always 0 (the Remastered lighting; RM_OPT_LIGHTING=1 for its checks) */
    RM_OPT_FPS,                     /* 1: the frame rate shown at the bottom centre */
    RM_OPT_TEXTURES,                /* shelved: always 0 (the Remastered textures; RM_OPT_TEXTURES=1 for its checks) */
    RM_OPT_COUNT
};
enum { RM_MUSIC_PC, RM_MUSIC_N64 };

int  rm_option(int which);
/* the choice, now: both games told, the music crossfaded, the file written */
void rm_option_set(int which, int value);
/* the kept choices, applied (main.c, before either game starts) */
void rm_options_load(void);

#endif
