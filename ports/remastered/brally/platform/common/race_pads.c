/* race_pads.c: the players' cars driven from Top Gear Rally Remastered's
 * controls (ports/remastered/platform/controls.h), in the races run for its
 * menus (race_handoff.c), one or two players.
 *
 * Boss Rally's engine keeps a pad block for each player (g_aBrEnts: 0x106ED708,
 * each fed from its N64-style record in g_aBrEntRecs, 0x106ED630, by
 * BrPadTranslate), the cars of the people racing reading theirs; the PC game only
 * ever filled the first, from its own key bindings (BrPadPackButtons).  While a
 * race is run for the menus each player's record is filled here instead, every
 * frame after the game's own (BrPadFrameBegin): the keys held (g_plat_dik) and
 * the player's own pad through the player's bindings, the first player's pad the
 * host's first (on the iPhone the touch controls), the second's the host's
 * second.  The game's own reading still runs for its menus (the pause menu).
 *
 * Reset Car puts the player's car back on the road as the game does a car stuck
 * on its roof: the race's reset (BrCarRespawn, 0x1005C6D0), asked for by that
 * car's player.  The PC game asked for it with a key of its own through one
 * request for whichever human car stepped first (DAT_118eeee4); here each
 * player's press is kept for that player's car (the first humans' cars are the
 * players', in order) and taken in its own step (FUN_1005c8b0, Remastered's
 * patch) where the PC game's request is: after the step's physics, which would
 * otherwise put the reset flag back on a car the right way up.  Asked for only
 * while the player drives (br_race_driving: not paused, not a replay) and, as
 * the PC game's, in neither mode 4 nor 5.
 *
 * Built with the core's flags, for its types. */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "br_addr32.h"
#include "br_coretypes.h"
#include "slice1_05.h"       /* g_aBrEntRecs, g_aBrEnts */
#include "slice3_41.h"       /* BrDriverCar, g_aBrRaceCar */
#include "br_race.h"         /* g_brRaceNEntrant */
#include "../host/host.h"
#include "race_pads.h"

/* the app's (ports/remastered/platform/controls.h) */
void rm_player_drive(int player, const unsigned char *dik, const host_pad *pad, unsigned short *buttons,
                     int *sx, int *sy);
extern uint8_t g_plat_dik[256];         /* br_os_window.c: scan codes held down */
void plat_script_autopilot_want(int driver, int *want);   /* script_game.c (checks) */
const int *br_handoff_autopilot_want(void);              /* race_handoff.c (checks) */
unsigned rm_player_actions(int player, const unsigned char *dik, const host_pad *pad);   /* the app's */
#define RM_ACT_RESET 15                 /* (controls.h: keep in step) */
int br_race_driving(void);              /* race_handoff.c */

static int s_on;
extern int g_brMode0AA8B4;              /* the views (1, or 2: split) */

static unsigned char s_reset_held[2], s_reset[2];   /* Reset Car: held last frame; asked for */
static unsigned char s_reset_log[2];                /* (RM_LOG: the car after it, logged next frame) */

static void reset_log(int i, const char *when)
{
    const BrDriverCar *c = &g_aBrRaceCar[i];
    const BrVec3 *v = &c->aBody[0].rb.st.vel;
    fprintf(stderr, "race_pads: player %d reset car %s: at %.1f %.1f %.1f body speed %.2f\n", i + 1, when, c->pos.x,
            c->pos.y, c->pos.z, sqrtf(v->x * v->x + v->y * v->y + v->z * v->z));
}

void br_race_pads_on(int on)
{
    s_on = on;
    memset(s_reset, 0, sizeof s_reset);
}

/* the human step (FUN_1005c8b0): 1 when this car's player asked for a reset,
 * taken */
int br_race_reset_take(const void *car)
{
    int i;
    for (i = 0; i < 2; i++)
        if (car == &g_aBrRaceCar[i] && s_reset[i]) {
            s_reset[i] = 0;
            if (getenv("RM_LOG")) {
                reset_log(i, "asked");
                s_reset_log[i] = 1;
            }
            return 1;
        }
    return 0;
}

/* Change View: the game's views are a C button each (the driving step,
 * FUN_1005c8b0): C-up the car's own view (aSnap[0], where a race starts),
 * C-down the chase view (aSnap[1]), C-left the view from the bonnet
 * (aSnap[2], one view on the screen only), as the PC game's three view keys
 * were.  The app's one Change View action takes the next of them from the one
 * the player's car shows, the button held while the action is. */
static unsigned short s_view_bit[2];

static unsigned short next_view(int player)
{
    int k = g_aBrView[player].iCar;
    BrDriverCar *car;
    if (k < 0 || k >= 16)
        return 0x0008;                          /* C-up */
    car = &g_aBrRaceCar[k];
    if (car->pMatA == &car->aSnap[0])
        return 0x0004;                          /* C-down: the chase view */
    if (car->pMatA == &car->aSnap[1] && g_brMode0AA8B4 == 1)
        return 0x0002;                          /* C-left: the bonnet */
    return 0x0008;                              /* C-up: the car's own */
}

int br_race_humans(void)
{
    int n = g_brRaceNEntrant;
    return n < 1 ? 1 : n > 2 ? 2 : n;
}

void br_race_pads(void)
{
    int i;
    if (!s_on || getenv("BR_RACE_PADS_OFF"))   /* (a check: the game's own reading instead) */
        return;
    for (i = 0; i < br_race_humans(); i++) {
        host_pad p;
        unsigned short b;
        int x, y, has = host_pad_read_n(i, &p);
        unsigned char *r = g_aBrEntRecs[i].b;
        rm_player_drive(i, g_plat_dik, has ? &p : 0, &b, &x, &y);
        if (s_reset_log[i]) {
            reset_log(i, "after");
            s_reset_log[i] = 0;
        }
        {
            int held = (rm_player_actions(i, g_plat_dik, has ? &p : 0) >> RM_ACT_RESET) & 1;
            if (held && !s_reset_held[i] && br_race_driving() && g_brRaceRules.mode != 4 && g_brRaceRules.mode != 5)
                s_reset[i] = 1;                 /* (pressed: taken in the car's next step) */
            s_reset_held[i] = (unsigned char)held;
        }
        if (b & 0x0008) {                       /* Change View (RM_N64_CU): the next view */
            if (!s_view_bit[i])
                s_view_bit[i] = next_view(i);
            b = (unsigned short)((b & ~0x0008) | s_view_bit[i]);
        } else {
            s_view_bit[i] = 0;
        }
        if ((i == 1 && getenv("BR_HANDOFF_AUTOPILOT2")) || (i == 0 && br_handoff_autopilot_want())) {
            int w2[4];                          /* (a check: the autopilot drives this car) */
            const int *want = i ? w2 : br_handoff_autopilot_want();
            if (i)
                plat_script_autopilot_want(1, w2);
            b = (unsigned short)((want[0] ? 0x8800 : 0) | (want[1] ? 0x8400 : 0) | (want[2] ? 0x0200 : 0) |
                                 (want[3] ? 0x0100 : 0));
            x = want[2] ? -80 : want[3] ? 80 : 0;
            y = want[1] ? -80 : 0;
        }
        r[0] = (unsigned char)(b & 0xFF);       /* the N64's word as the game reads it */
        r[1] = (unsigned char)(b >> 8);
        r[2] = (unsigned char)(signed char)x;
        r[3] = (unsigned char)(signed char)y;
        r[4] = 0;                               /* plugged in */
    }
}

/* ---- the second view ------------------------------------------------------------------
 * Each race frame is drawn from a snapshot of the race (BrSnap, one a slot), which keeps
 * one view's record (tailA): the PC game never raced two people on one screen.  With
 * two, the second view's record (its car, its dials' textures) is kept here a slot,
 * beside the snapshot, and the frame is drawn from the two together. */
#define NSLOT 8
static BrView s_view2[NSLOT], s_pair[2];
static unsigned char s_saved[NSLOT];

void br_split_save(int slot)
{
    if (slot >= 0 && slot < NSLOT) {
        s_view2[slot] = g_aBrView[1];
        s_saved[slot] = 1;
    }
}

void *br_split_views(int slot, void *snap)
{
    if (g_brMode0AA8B4 < 2 || slot < 0 || slot >= NSLOT)
        return snap;
    memcpy(&s_pair[0], snap, sizeof s_pair[0]);
    s_pair[1] = s_saved[slot] ? s_view2[slot] : g_aBrView[1];   /* (the live frame's slot: the race's own) */
    return s_pair;
}

/* ---- each view's HUD side by side ---------------------------------------------------- */
extern uint32_t *g_BrGfxPtr;                /* the frame's display list: its write cursor */
static void dl_emit(uint32_t w0, uint32_t w1)
{
    g_BrGfxPtr[0] = w0;
    g_BrGfxPtr[1] = w1;
    g_BrGfxPtr += 2;
}
void plat_hud_view(int on, int x, int y, int w, int h);   /* br_gfx.c */
#define HUD_MARK 0x48550000u                /* "HU", then the view + 1 (0: off) */
static int s_hudr[2][4];

void br_hud_view_mark(int view, int x, int y, int w, int h)
{
    if (view >= 0 && view < 2) {
        s_hudr[view][0] = x;
        s_hudr[view][1] = y;
        s_hudr[view][2] = w;
        s_hudr[view][3] = h;
    }
    dl_emit(0xBC000008u, HUD_MARK | (unsigned)(view >= 0 && view < 2 ? view + 1 : 0));
}

void br_dl_marker(unsigned w1)
{
    unsigned k;
    if ((w1 & 0xFFFF0000u) != HUD_MARK)
        return;
    k = w1 & 0xFFFFu;
    if (k >= 1 && k <= 2)
        plat_hud_view(1, s_hudr[k - 1][0], s_hudr[k - 1][1], s_hudr[k - 1][2], s_hudr[k - 1][3]);
    else
        plat_hud_view(0, 0, 0, 0, 0);
}
