/* tgr_touch.h: the game's menus by touch, for a host with a touch screen
 * (ports/tgrally/ios).  Nothing here changes what the game does with a pad:
 * a touch becomes presses of the N64 pad, played in at the retrace as a
 * player's thumb would (platform/os/touch.c).
 *
 * The game's menu code (port lines, src/) names what it draws as tap
 * targets each frame, in its screen pixels: the carousel's arrows and the
 * item between them, the rows of a list with the row the cursor is on, and
 * the button prompts (Select, Go Back, OK, Cancel, Exit: recognised by the
 * text printer).  A tap is matched against the targets of the frame on the
 * screen. */
#ifndef TGR_TOUCH_H
#define TGR_TOUCH_H

/* ---- the game's side (port lines in src/) -------------------------------- */
/* the generic carousel (the ring of a front-end menu or the car select):
 * its arrows step left and right, the item between them is A */
void tgr_touch_carousel(void);
/* the selected row takes up and down as a value (a volume): a vertical swipe
 * is not turned round for it */
void tgr_touch_value_row(void);
/* the next string printed is row `row` of a list whose cursor is on `cur`
 * (a higher row is further down): a tap moves the cursor there, then A */
void tgr_touch_row(int row, int cur);
/* the text printer (drawing/textstate.c): a string at x0, y, w wide, in a
 * font of `size`; a tagged row, or a prompt, becomes a target */
int  tgr_touch_wants(const char *s);
void tgr_touch_text(const char *s, int x0, int y, int w, int size);

/* ---- the platform's side --------------------------------------------------- */
void tgr_touch_built(void);                  /* a display list is handed to the RCP */
/* the frame drawn from it is on the screen: fb_w by fb_h game pixels, or (a
   race) stretched kx by ky (gfx/rcp.c) */
void tgr_touch_shown(float fb_w, float fb_h, int race, float kx, float ky);
/* each retrace, before the pad is read: the touch's presses over the pad's */
void tgr_touch_input(unsigned short *buttons, int *x, int *y);

/* ---- the host's side --------------------------------------------------------- */
/* a tap at (u, v): fractions of the area the game is drawn in (0..1 across
   and down; outside it, beyond) */
void tgr_touch_tap(float u, float v);
/* a swipe: dx, dy its direction (screen axes, y down) */
void tgr_touch_swipe(float dx, float dy);
/* a press of one N64 button (libultra's bits: START_BUTTON is 0x1000) */
void tgr_touch_press(unsigned short button);

#endif
