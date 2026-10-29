/* controls.h: Top Gear Rally Remastered's controls (controls.c): what each of
 * the two players drives with, chosen on Top Gear Rally's controller screen
 * (ports/remastered/tgrally/src/menus/rmcontrols.c), read by Boss Rally's races
 * (ports/remastered/brally/platform/common/race_pads.c) and by Top Gear Rally's
 * menus for the second player (its controller port 2), kept in the app's save
 * folder ("controls.cfg").
 *
 * Each action has one key (a DirectInput scan code, as the host's key events
 * carry it; 0 none) and one gamepad button (host_pad's numbering, 0..15; -1
 * none).  A player drives with the keyboard and their own pad at once: the first
 * player's pad is the host's first (on the iPhone, the touch controls), the
 * second's the host's second.  The left stick always steers. */
#ifndef RM_CONTROLS_H
#define RM_CONTROLS_H

enum {
    RM_ACT_ACCEL,
    RM_ACT_BRAKE,
    RM_ACT_REVERSE,
    RM_ACT_LEFT,
    RM_ACT_RIGHT,
    RM_ACT_GEAR_UP,
    RM_ACT_GEAR_DOWN,
    RM_ACT_VIEW,
    RM_ACT_UP,          /* the menus' own: the stick's up and down */
    RM_ACT_DOWN,
    RM_ACT_START,       /* Start in the menus; a race's pause */
    RM_ACT_Z,           /* the N64 buttons Top Gear Rally's menus also read (the paint shop, */
    RM_ACT_C_DOWN,      /* the car select) */
    RM_ACT_C_LEFT,
    RM_ACT_C_RIGHT,
    RM_ACT_COUNT
};
enum { RM_BIND_KEY, RM_BIND_PAD };

/* the N64 pad's buttons, as both games read a pad */
enum {
    RM_N64_A = 0x8000, RM_N64_B = 0x4000, RM_N64_Z = 0x2000, RM_N64_START = 0x1000,
    RM_N64_DU = 0x0800, RM_N64_DD = 0x0400, RM_N64_DL = 0x0200, RM_N64_DR = 0x0100,
    RM_N64_L = 0x0020, RM_N64_R = 0x0010, RM_N64_CU = 0x0008, RM_N64_CD = 0x0004,
    RM_N64_CL = 0x0002, RM_N64_CR = 0x0001
};

struct host_pad;

const char *rm_action_name(int act);                 /* "Accelerate" */
int  rm_bind(int player, int act, int kind);
void rm_bind_set(int player, int act, int kind, int code);   /* taken from any other action of the player */
void rm_bind_defaults(int player);
void rm_bind_reset(int player);                     /* the defaults, kept */
void rm_controls_load(void);                         /* (main.c, before either game starts) */
/* a binding's name: "UP ARROW", "PAD A"; "-" for none */
const char *rm_bind_label(int kind, int code, char *buf, int n);

/* Each action is one N64 button, the same in both games: Boss Rally's races read
 * an N64 pad (race_pads.c) and Top Gear Rally's menus are the N64's.  Accelerate
 * is A (the menus' select), Brake B (back), Gear Up R, Gear Down L, Change View
 * C-up, Pause Start; Up, Down and the steering the stick's four ways; Z and the
 * other three C buttons the menus' alone; Reverse is A with the stick back, the
 * races' alone. */

/* a player's car controls this moment as an N64 pad: dik the keys held (DirectInput
 * scan codes, nonzero held), pad the player's own (NULL: none).  Accelerate is A,
 * brake B, reverse A with the stick back, steering the stick (and the d-pad's
 * sides for a key: the race steers keys on a curve), gears R and L, the view C-up */
void rm_player_drive(int player, const unsigned char *dik, const struct host_pad *pad,
                     unsigned short *buttons, int *sx, int *sy);
/* the same player in a menu: every action's button, the stick's four ways from
 * the steering and Up and Down (and the pad's own stick) */
void rm_player_menu(int player, const unsigned char *dik, const struct host_pad *pad,
                    unsigned short *buttons, int *sx, int *sy);

/* the actions the player holds this moment, a bit each (1 << RM_ACT_...); the pad's
 * stick pushed most of the way counts as the steering's.  Europe's and Japan's cheats
 * read them: the cartridges matched the pad's buttons, Remastered the actions that
 * play those buttons' parts (rmregion.c, race_handoff.c) */
unsigned rm_player_actions(int player, const unsigned char *dik, const struct host_pad *pad);

#endif
