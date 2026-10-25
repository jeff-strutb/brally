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
    RM_OPT_MENUS,                   /* RM_MENUS_*: whose menus the app shows */
    RM_OPT_VIEWDIST,                /* RM_VIEW_*: how far a race view sees */
    RM_OPT_COUNT
};
enum { RM_MUSIC_PC, RM_MUSIC_N64 };
enum { RM_MENUS_TGR, RM_MENUS_BR };
enum { RM_VIEW_ORIGINAL, RM_VIEW_UNLIMITED };

int  rm_option(int which);
/* the choice, now: both games told, the music crossfaded, the file written */
void rm_option_set(int which, int value);
/* the kept choices, applied (main.c, before either game starts) */
void rm_options_load(void);

/* the app closes, as its window's close does: Boss Rally's Quit when its
 * menus are the app's (mux.c); 0 where an app does not close itself (the
 * iPhone) */
int  rm_quit(void);

#endif
