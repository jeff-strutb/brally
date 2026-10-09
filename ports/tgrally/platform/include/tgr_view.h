/* tgr_view.h: the port's behaviour flags, their two profiles, and how a race
 * fills a window of any shape (platform/os/view.c).
 *
 * As the Boss Rally port (ports/brally/platform/include/br_flags.h): every
 * place where the port can behave as the cartridge did or in a way the N64
 * could not is one flag, and a profile sets them all:
 *
 *   original    the game as it shipped: the N64's 4:3 picture
 *   remastered  the port's improvements on
 *
 * TGR_PROFILE=original|remastered picks the profile to start in (remastered
 * when unset); TGR_FLAG_<NAME>=0|1 then overrides one flag.  In play, Tab or
 * the game controller's View button (the one button the N64 pad has no use
 * for) switches between the two.
 *
 * With TGR_FLAG_ANY_ASPECT on and a window of another shape than 4:3, a race
 * frame fills the window: each view's lens is widened to the window's shape
 * (Hor+ when wider, Vert+ when taller) and its culling wedge with it, the
 * rear-view mirror keeps its shape, and the HUD keeps its own shape at the
 * edges it sat by (gfx/rcp.c).  Menus stay 4:3, centred. */
#ifndef TGR_VIEW_H
#define TGR_VIEW_H

enum tgr_flag {
    /* any window shape: the race fills it, the camera widened (wider than
     * 4:3 sees more to the sides, taller more above and below), the HUD at
     * its own shape at the window's edges.  Off: the N64's 4:3 picture,
     * letterboxed or pillarboxed. */
    TGR_FLAG_ANY_ASPECT,
    /* races run by Boss Rally's engine: the menus are Top Gear Rally's, and
     * each race the player starts is handed to Boss Rally (the same tracks;
     * host_race.h, src/racing/handoff.c), its outcome taken back into the
     * results, the season and the records.  Only where the host carries Boss
     * Rally (the iPhone app); elsewhere, and off, the race is the N64's. */
    TGR_FLAG_BR_RACES,
    TGR_FLAG_COUNT
};

int  tgr_flag(int flag);                /* 1 when on */
const char *tgr_profile(void);          /* "original" or "remastered" */
void tgr_profile_toggle(void);          /* the other profile, now */

/* how far a race view is widened (kx) or heightened (ky) past the game's
 * 4:3 to fill the window: 1, 1 when it is not (the flag off, no window, or a
 * 4:3 one).  TGR_WINDOW=WxH stands in for a window that is not there
 * (screenshots). */
void tgr_view_scale(float *kx, float *ky);

/* the race's views are being drawn (racing/racetick.c, port): only their
 * cameras are widened (the car select's and the paint shop's are not) */
void tgr_view_race(int on);
/* 1 while a race is being driven: its views drawn within the last few
 * retraces and the race not paused (a touch host steers then; elsewhere a
 * touch is a menu's swipe or tap) */
int  tgr_view_driving(void);

/* the game's camera (drawing/frameloop.c, port): the lens and the culling
 * wedge of a view.  fovy in degrees and aspect as the game makes them, kept
 * when the view is not widened; a mirrored view (the rear-view mirror) keeps
 * its own shape. */
void tgr_view_lens(float *fovy, float *aspect, int mirror);
void tgr_view_wedge(float *half_w, float *half_h, int mirror);
/* the Mtx (in the arena) the camera just made: a race view's projection, or
 * the mirror's, so the RCP knows what it draws (gfx/rcp.c) */
void tgr_view_proj(const void *mtx, int mirror);
/* the RCP: what a projection loaded from this address is: 0 neither, 1 a
 * widened race view's, 2 the mirror's beside one; *kx, *ky the scale its
 * lens was made for */
int  tgr_view_proj_kind(const void *mtx, float *kx, float *ky);

#endif
