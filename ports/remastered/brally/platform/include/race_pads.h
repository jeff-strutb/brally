/* race_pads.h: the players' cars driven from the app's controls (race_pads.c). */
#ifndef RACE_PADS_H
#define RACE_PADS_H
#ifdef __cplusplus
extern "C" {
#endif
void br_race_pads_on(int on);   /* race_handoff.c: a race run for the menus is on */
int  br_race_car2(int car);     /* the second player's car (race_handoff.c; `car` when there is none) */
/* the second player's handling, transmission, tyres and suspension into its car's
   set-up (race_handoff.c; left as they are when there is none) */
void br_race_setup2(int *handling, int *automatic, int *tires, int *suspension);
int  br_race_humans(void);      /* the people racing, 1 or 2 (BrPadTranslateAll's pads) */
void br_race_pads(void);        /* BrPadFrameBegin, after the game's own: the players' records */
/* split screen: the race step keeps the second view a snapshot slot (br_split_save),
   the frame is drawn from both (br_split_views: the snapshot's view, or the pair) */
void  br_split_save(int slot);
/* side by side, each view's HUD mapped into its half: a marker in the frame's display list
   (G_MOVEWORD index 8, which the game skips) at the HUD's start (view) and end (-1); the
   list is run at the frame's end, the marker then sets the mapping (glide.c) */
void  br_hud_view_mark(int view, int x, int y, int w, int h);
void  br_dl_marker(unsigned w1);
void *br_split_views(int slot, void *snap);
#ifdef __cplusplus
}
#endif
#endif
