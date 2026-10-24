/* pause_options.h: the race's pause menu as Top Gear Rally Remastered has it
 * (pause_options.c): BrRaceStep and BrFrameDraw call these. */
#ifndef PAUSE_OPTIONS_H
#define PAUSE_OPTIONS_H

#ifdef __cplusplus
extern "C" {
#endif

/* the menu's rows: the game's six with Remaster Options before the quits */
#define RM_PAUSE_ROWS 7
#define RM_PAUSE_ROW  4             /* Remaster Options */

int  rm_pause_active(void);         /* 1 while the Remaster Options page is up */
void rm_pause_open(void);
void rm_pause_step(int presses);    /* the page's frame: g_BrX18ABAD0 */
void rm_pause_draw(int x, int h);   /* the page, centred on x, the screen h tall */
int  rm_pause_tap(float nx, float ny, int x, int h, int *press);
/* While paused the input is polled more often than the race step reads it,
 * and each poll's presses replace the last's: a short press could be lost
 * (the original's menu loses them too).  Each poll's are noted here and the
 * race step takes them all at once. */
void rm_pause_note(int presses);
int  rm_pause_take(void);
void rm_fps_frame(int x, int h);    /* each race frame: the frame rate, when shown */

/* br_gfx.c: a tap at nx, ny (0..1 across the window) in the game's pixels, as
 * a piece of 2D with box b (x0 y0 x1 y1) is placed on the window */
void plat_tap_to_game(float nx, float ny, const float *b, float *gx, float *gy);

#ifdef __cplusplus
}
#endif
#endif
