/* race_handoff.c: Boss Rally's races, run for another game's front end
 * (host_race.h: Top Gear Rally Remastered, ports/remastered, runs Top Gear
 * Rally's menus and hands each race here).
 *
 * Boss Rally already runs race after race in one process: its menus (app
 * state 4) leave by shutting their phase down, the next frame loads the race
 * (state 3, BrGlRaceStart) and installs BrRaceStep, and after the race and
 * its replay the step re-initialises the session and the menus are built
 * again.  This drives that loop from outside, at BrAppFrame's entry
 * (plat_app_frame):
 *
 *   idle    the menus are up: the thread waits here for a race; one comes,
 *           its settings go where the game's own command line puts them
 *           (chosenTrack= and the rest, br_basedir.c) and the menu phase is
 *           shut down as the attract demo does it
 *   start   the race step is installed: the countdown starts at once (the
 *           game would wait for the start key)
 *   race    the light script's finish-save (5) takes the player's result; the
 *           replay that follows is left at once (its Escape)
 *   back    the menus are up again: the result goes to the caller
 *
 * The attract demos (mode 4: the intro, the credits, the ending, each a
 * recorded race the game plays back) and the end-of-season ceremony (mode 5,
 * on its own track) go the same way.  Any key the caller sends skips one, as
 * the game's own pause key does (the race step's mode 4/5 branch); and the
 * ceremony returns to the menus, not to Boss Rally's own season or ending
 * (the caller has its own).
 *
 * Nothing here runs unless a caller asked for races (br_race_run), or
 * BR_HANDOFF_TEST asks for one (headless checks: the autopilot drives).
 * BR_HANDOFF_AUTOPILOT=1 lets the autopilot drive the caller's races too (a
 * check of the whole round trip).  BR_HANDOFF_TAPS=F:nx,ny[;...] taps at nx,
 * ny (0..1 across the window) F frames after the pause menu opened (a check
 * of the pause menu's taps).
 * Built with the core's flags, for its types. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __APPLE__
#include <TargetConditionals.h>
#endif
#include "br_addr32.h"
#include "br_coretypes.h"
#include "slice3_41.h"     /* BrDriverCar, BrDriver */
#include "br_phase.h"
#include "br_race.h"         /* g_brRaceRules */
#include "br_gamestep.h"     /* g_pfnStep */
#include "../host/host.h"    /* host_pad_read */
#include "slice2_25.h"       /* g_brPAA29B8 */
#include "../host/host_race.h"
#include "pause_options.h"
#include "race_pads.h"
#include "rm_rumble.h"

typedef struct host_mutex host_mutex;   /* host.h (not includable beside the core's Win32 surface) */
typedef struct host_cond host_cond;
host_mutex *host_mutex_new(void);
void        host_mutex_lock(host_mutex *m);
void        host_mutex_unlock(host_mutex *m);
host_cond  *host_cond_new(void);
int         host_cond_wait(host_cond *c, host_mutex *m, uint32_t timeout_ms);
void        host_cond_broadcast(host_cond *c);
void plat_script_autopilot(void);       /* script_game.c */
void plat_script_autopilot_off(void);
void plat_script_key(int vk, int dik, int down);   /* script.c */
void rm_menuopts_refresh(void);         /* src/core/menus/rm_menuopts.cpp */

enum { IDLE, START, RACE, BACK };

static host_mutex *s_m;
static host_cond *s_c;
static int s_serving;               /* a caller asked for races: wait for them at the menus */
static int s_menus;                 /* the menus are the app's (br_menus_run): they run, not wait */
static int s_pending, s_done;
static HostRace s_req;
static HostRaceResult s_res, s_res2;   /* the first player's, the second's */
static int s_phase = IDLE, s_got, s_test;
static int s_skip;                   /* the demo or ceremony was skipped */
static int s_frames;                 /* frames the race step ran (a check's report) */
#define PLAYBACK (s_req.mode == HOST_RACE_ATTRACT || s_req.mode == HOST_RACE_SEASON_END)
#define AUTOPILOT ((s_test || getenv("BR_HANDOFF_AUTOPILOT")) && !getenv("BR_PADFAKE"))
static int s_ap_paused;              /* the autopilot let go for the pause menu */
static int s_underway;               /* this race reached its racing light (4): the light script
                                       holds the last race's finish until the step re-inits it */

static void init(void)
{
    if (!s_m) {
        s_m = host_mutex_new();
        s_c = host_cond_new();
    }
}

/* the caller's side (another thread): one race, waited for */
int br_race_run(const HostRace *race, HostRaceResult *res)
{
    init();
    host_mutex_lock(s_m);
    s_serving = 1;
    s_req = *race;
    s_done = 0;
    s_pending = 1;
    host_cond_broadcast(s_c);
    while (!s_done)
        host_cond_wait(s_c, s_m, 0xFFFFFFFFu);
    *res = s_res;
    if (race->players == 2)
        res[1] = s_res2;              /* (host_race.h: the second player's beside it) */
    s_done = 0;
    host_mutex_unlock(s_m);
    return 1;
}

/* the caller's side (another thread): the game's menus are the app's until
 * the player chooses the caller's again (br_menus_leave, from the Remaster
 * Options page: menu_options.c); then they wait for races again */
void br_menus_run(void)
{
    init();
    host_mutex_lock(s_m);
    s_menus = 1;
    host_cond_broadcast(s_c);
    while (s_menus)
        host_cond_wait(s_c, s_m, 0xFFFFFFFFu);
    host_mutex_unlock(s_m);
}

/* the game's side: the caller's menus again (its next frame at the menus waits) */
void br_menus_leave(void)
{
    init();
    host_mutex_lock(s_m);
    s_menus = 0;
    host_cond_broadcast(s_c);
    host_mutex_unlock(s_m);
}

int br_menus_own(void) { return s_menus; }

/* the caller is there: from now on the game waits at its menus for races */
void br_race_serve(void)
{
    init();
    host_mutex_lock(s_m);
    s_serving = 1;
    host_cond_broadcast(s_c);
    host_mutex_unlock(s_m);
}

/* BR_HANDOFF_TEST=mode,track,mirrored,weather,laps,car[;...]: races one after
 * another in the one process, driven by the autopilot; each result printed */
static const char *s_test_next;

static int test_request(void)
{
    const char *e = s_test ? s_test_next : getenv("BR_HANDOFF_TEST");
    HostRace r;
    if (!e || !*e)
        return 0;
    memset(&r, 0, sizeof r);
    r.handling = 1;
    r.automatic = 1;
    r.tires = 1;
    r.suspension = 1;
    r.car2 = -1;
    r.automatic2 = r.automatic;
    r.tires2 = r.tires;
    r.suspension2 = r.suspension;
    if (sscanf(e, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d", &r.mode, &r.track, &r.mirrored, &r.weather, &r.laps, &r.car,
               &r.piece, &r.players, &r.car2, &r.automatic2, &r.tires2, &r.suspension2) < 6)
        return 0;
    s_test = 1;
    s_test_next = strchr(e, ';') ? strchr(e, ';') + 1 : "";
    s_req = r;
    s_pending = 1;
    return 1;
}

/* the weather: both games number it alike (sunny, foggy, rainy, snowy, night;
 * Boss Rally's strings 131-135), and BrRaceDifficultySet (0x100627B0) sets
 * the same four flags in the same order as Top Gear Rally's BrRaceSetKind */
/* the race's settings, where the game's command line and menus put them */
static void controls(void);
static void configure(const HostRace *r)
{
    int track = r->track + (r->mirrored ? 6 : 0);
    g_brRaceRules.mode = r->mode >= 0 && r->mode <= HOST_RACE_SEASON_END ? r->mode : HOST_RACE_ARCADE;
    if (r->mode == HOST_RACE_ATTRACT)
        g_5bc760 = r->piece;          /* 0 the intro, 1 the credits, 2 the ending */
    g_Br0B380C = track;
    g_226e7c = r->car;
    g_226e80 = r->weather;
    g_7b320 = r->handling;
    g_7b324 = r->automatic;
    g_7b32c = r->tires;
    g_7b328 = r->suspension;
    g_CBE8 = r->laps > 0 ? r->laps : 3;
    g_brRaceNEntrant = r->players == 2 && r->mode <= HOST_RACE_PRACTICE ? 2 : 1;
    if (getenv("BR_HANDOFF_BINDINGS")) {          /* each profile's first actions' bindings */
        int pr, a;
        for (pr = 0; pr < 4; pr++) {
            fprintf(stderr, "handoff: profile %d:", pr);
            for (a = 0; a < 12; a++)
                fprintf(stderr, " %d=%04x/%04x/%04x", a, g_BrCtrlCfg.profile[pr].e[a][0],
                        g_BrCtrlCfg.profile[pr].e[a][1], g_BrCtrlCfg.profile[pr].e[a][2]);
            fprintf(stderr, "\n");
        }
    }
    controls();
}

/* the race's controls: touch reaches the game as the joystick (dx.c), in the
   gamepad profile (2: the stick's x steers, button 0 accelerates, button 1
   brakes); a desktop's keyboard drives the keyboard profile (0: the arrows),
   its pad the gamepad profile while one is connected (looked at every frame);
   the headless check drives the keys */
static void controls(void)
{
    int pr;
#if defined(__APPLE__) && TARGET_OS_IPHONE
    pr = 2;
#else
    host_pad p;
    pr = host_pad_read(&p) ? 2 : 0;
#endif
    if (AUTOPILOT)
        pr = 0;
    if (g_BrCtrlCfg.active != pr || g_BrPadModeBytes != (const unsigned char *)&g_BrCtrlCfg.profile[pr]) {
        g_BrCtrlCfg.active = pr;
        g_BrPadModeBytes = (const unsigned char *)&g_BrCtrlCfg.profile[pr];
    }
}

/* ---- Europe's and Japan's FINISH! cheat, on the first player's own controls ------------
 * The cartridges watch the first pad for d-pad up, up, right, A, C-down, d-pad right, each
 * let go before the next, and end the race there: every lap done, the first player first
 * and the second second.  Remastered watches the actions that play those buttons' parts
 * (controls.h): Gear Up, Gear Up, Steer Right, Accelerate, Change View, Steer Right, and
 * finishes the players' cars as Boss Rally's finish line does (br_race.c), every lap
 * counted and the time as it stands. */
unsigned rm_player_actions(int player, const unsigned char *dik, const host_pad *pad);   /* the app's */
extern uint8_t g_plat_dik[256];         /* br_os_window.c: scan codes held down */
enum { ACT_ACCEL = 1 << 0, ACT_RIGHT = 1 << 4, ACT_GEAR_UP = 1 << 5, ACT_VIEW = 1 << 7 };
static const unsigned k_finish[] = {    /* newest first, as the cartridges keep them */
    ACT_RIGHT, 0, ACT_VIEW, 0, ACT_ACCEL, 0, ACT_RIGHT, 0, ACT_GEAR_UP, 0, ACT_GEAR_UP, 0,
};
#define NFINISH (int)(sizeof k_finish / sizeof k_finish[0])
static unsigned s_hist[NFINISH];        /* the first player's actions, a change each, newest first */

static void finish_now(void)
{
    int k, i, n = br_race_humans(), fresh = 0;
    for (k = 0; k < n; k++)
        if (!(g_aBrRaceDriver[k].f68 & BR_DRIVER_SKIP))
            fresh++;
    for (i = n; i < g_brRaceNDriver; i++)      /* (any car already in: behind them) */
        if ((g_aBrRaceDriver[i].f68 & BR_DRIVER_SKIP) && g_aBrRaceDriver[i].pCar)
            g_aBrRaceDriver[i].pCar->fFF8 += fresh;
    for (k = 0; k < n; k++) {
        BrDriver *d = &g_aBrRaceDriver[k];
        BrDriverCar *c = d->pCar;
        if (d->f68 & BR_DRIVER_SKIP)
            continue;
        d->f68 |= BR_DRIVER_SKIP;          /* (the driver's lap and clock are the car's, copied each frame) */
        if (c) {
            c->lap = c->lapB = g_CBE8;      /* the time stands as it is, as on the cartridges */
            c->f29B0 = 0.9f;
            c->fFF8 = k;
            c->pszBanner = g_brRaceRules.mode == 6 ? (char *)"" : BrStrGet(g_aBrRacePlaceMsg[k]);
            c->f1000 = 5.0f;
        }
        g_brRaceNFinished++;
    }
}

/* one racing frame: the first player's actions into the history; the race ends on the
   sequence's last press */
static void finish_cheat(void)
{
    host_pad p;
    int has = host_pad_read_n(0, &p), i;
    unsigned m = rm_player_actions(0, g_plat_dik, has ? &p : NULL);
    if (getenv("BR_FINISH_ACTS")) {      /* (a check: BR_FINISH_ACTS=F:mask,... the actions at race frame F) */
        const char *e = getenv("BR_FINISH_ACTS");
        int f;
        unsigned v;
        m = 0;
        while (e && sscanf(e, "%d:%x", &f, &v) == 2) {
            if (s_frames >= f)
                m = v;
            e = strchr(e, ',');
            e = e ? e + 1 : NULL;
        }
    }
    if (m == s_hist[0])
        return;
    memmove(s_hist + 1, s_hist, sizeof s_hist - sizeof s_hist[0]);
    s_hist[0] = m;
    for (i = 0; i < NFINISH && s_hist[i] == k_finish[i]; i++)
        ;
    if (i == NFINISH) {
        fprintf(stderr, "FINISH!\n");
        memset(s_hist, 0, sizeof s_hist);
        finish_now();
    }
}

static void result_of(int k, HostRaceResult *r)
{
    BrDriverCar *car = g_aBrRaceDriver[k].pCar;
    int i, n;
    memset(r, 0, sizeof *r);
    r->outcome = HOST_RACE_FINISHED;
    if (!car)
        return;
    r->place = car->fFF8;
    r->laps = car->lap;
    r->total = car->tFinal;
    r->best = car->tBest;
    r->best_lap = car->lapBest;
    n = car->lap < 12 ? car->lap : 12;
    for (i = 0; i < n; i++)
        r->lap[i] = car->aLapTime[i];
}

/* the players' outcomes (the second's when two raced: the second driver, the
   second human car) */
static void result(void)
{
    result_of(0, &s_res);
    if (g_brRaceNEntrant > 1)
        result_of(1, &s_res2);
    else
        memset(&s_res2, 0, sizeof s_res2);
}

static int s_raced_track, s_raced_weather, s_raced_mode;

static void finish(void)
{
    if (s_test) {
        int i;
        fprintf(stderr, "handoff: %d frames, mode %d track %d weather %d: %s, place %d, laps %d, total %.2f, best %.2f (lap %d):",
                s_frames, s_raced_mode, s_raced_track, s_raced_weather,
                s_res.outcome == HOST_RACE_FINISHED ? "finished" : "left", s_res.place + 1, s_res.laps,
                s_res.total, s_res.best, s_res.best_lap + 1);
        for (i = 0; i < s_res.laps && i < 12; i++)
            fprintf(stderr, " %.2f", s_res.lap[i]);
        if (s_req.players == 2)
            fprintf(stderr, "; player 2: place %d, laps %d, total %.2f", s_res2.place + 1, s_res2.laps, s_res2.total);
        fprintf(stderr, "\n");
        if (!*s_test_next)
            exit(0);
        return;
    }
    host_mutex_lock(s_m);
    s_done = 1;
    s_pending = 0;
    host_cond_broadcast(s_c);
    host_mutex_unlock(s_m);
}

/* 1 while the player drives: the race on, not its replay, not paused, not yet
 * finished (a touch host steers then; elsewhere a touch is a menu's) */
int br_race_driving(void)
{
    /* (a caller's race, or one the game's own menus started while they are the app's) */
    return (s_phase == RACE || (s_phase == IDLE && s_menus && g_brRaceRules.mode != 4)) &&
           g_pfnStep == (BrGameStepFn)BrRaceStep && DAT_105ccb68[8] == 0 &&
           g_BrX06909B4 == 0 && g_brRaceLights < 5;
}

/* a key pressed for a moment, for the race's own menus (the pause menu reads
 * the keyboard): BR_KEY_* */
static const struct { int vk, dik; } k_keys[] = {
    { 0x26, 0xC8 }, { 0x28, 0xD0 }, { 0x25, 0xCB }, { 0x27, 0xCD },   /* the arrows */
    { 0x0D, 0x1C }, { 0x1B, 0x01 },                                    /* Return, Escape */
};
static volatile int s_key = -1, s_key_frames;

void br_race_key(int key)
{
    if (key >= 0 && key < (int)(sizeof k_keys / sizeof k_keys[0]))
        s_key = key;
}

/* a tap at nx, ny (0..1 across the game's screen), from another thread: taken
 * by the next frame (taps) */
static volatile int s_tap, s_tap_menu;
static volatile float s_tap_x, s_tap_y;

void br_race_tap(float nx, float ny)
{
    s_tap_x = nx;
    s_tap_y = ny;
    s_tap_menu = 0;
    s_tap = 1;
}

/* a mouse click: the same on the pause menu, nothing elsewhere */
void br_race_click(float nx, float ny)
{
    s_tap_x = nx;
    s_tap_y = ny;
    s_tap_menu = 1;
    s_tap = 1;
}

/* a touch on the game's own menus while they are the app's: the pointer
 * there and a click, as a mouse would (the button down the frame after the
 * pointer moved, up two frames later) */
void plat_mouse_abs(int x, int y);      /* dx.c */
void plat_mouse_button(int down);
static int s_menu_click;                 /* frames into the click */

static void menu_click(void)
{
    if (s_menu_click <= 0)
        return;
    if (s_menu_click == 3)
        plat_mouse_button(1);
    else if (s_menu_click == 1)
        plat_mouse_button(0);
    s_menu_click--;
}

/* the pause menu's rows and the Remaster Options page take it
 * (pause_options.c); on the game's own menus while they are the app's a
 * touch clicks where it lands; anywhere else in a race it is Return, as before */
static void taps(void)
{
    int press = 0;
    menu_click();
    if (!s_tap)
        return;
    s_tap = 0;
    if (s_menus && s_phase == IDLE && DAT_105ccb68[21] == 4 && g_brPAA29B8 != 0) {
        if (!s_tap_menu) {           /* (a mouse's click is the game's own already) */
            static const float all[4] = { 0, 0, 640, 480 };
            float gx, gy;
            plat_tap_to_game(s_tap_x, s_tap_y, all, &gx, &gy);
            plat_mouse_abs((int)gx, (int)gy);
            s_menu_click = 4;
        }
        return;
    }
    if (g_BrX06909B4 == 1) {
        if (rm_pause_tap(s_tap_x, s_tap_y, g_scrW4 / 2, g_brRaceCueBase, &press) && press)
            s_key = BR_KEY_ENTER;
        return;
    }
    if (!s_tap_menu)
        s_key = BR_KEY_ENTER;
}

static void keys(void)
{
    static int down = -1;
    if (s_phase == RACE && PLAYBACK)
        return;                      /* (read as a skip, plat_race_frame) */
    if (down >= 0 && --s_key_frames <= 0) {
        plat_script_key(k_keys[down].vk, k_keys[down].dik, 0);
        down = -1;
    }
    if (down < 0 && s_key >= 0) {
        down = s_key;
        s_key = -1;
        s_key_frames = 3;
        plat_script_key(k_keys[down].vk, k_keys[down].dik, 1);
    }
}

/* BR_HANDOFF_TAPS: each tap at its frame after the pause menu opened */
static void check_taps(void)
{
    static int opened = -1;
    static const char *next;
    int f;
    float x, y;
    if (!getenv("BR_HANDOFF_TAPS"))
        return;
    if (g_BrX06909B4 == 0) {
        opened = -1;
        return;
    }
    if (opened < 0) {
        opened = s_frames;
        if (!next)
            next = getenv("BR_HANDOFF_TAPS");
    }
    if (next && *next && sscanf(next, "%d:%f,%f", &f, &x, &y) == 3 && s_frames - opened == f) {
        fprintf(stderr, "handoff: tap %.4f %.4f\n", x, y);
        br_race_tap(x, y);
        next = strchr(next, ';') ? strchr(next, ';') + 1 : "";
    }
}

/* the second player's car, for the race step's set-up (race_pads.h) */
int br_race_car2(int car)
{
    return s_phase == RACE && s_req.players == 2 && s_req.car2 >= 0 && s_req.car2 < 16 ? s_req.car2 : car;
}

void br_race_setup2(int *handling, int *automatic, int *tires, int *suspension)
{
    if (s_phase != RACE || s_req.players != 2)
        return;
    *handling = s_req.handling;
    *automatic = s_req.automatic2;
    *tires = s_req.tires2;
    *suspension = s_req.suspension2;
    if (getenv("RM_LOG"))
        fprintf(stderr, "handoff: player 2 set-up: handling %d automatic %d tires %d suspension %d\n", *handling,
                *automatic, *tires, *suspension);
}

/* a race the game's own menus started, while they are the app's: the players
   drive with the app's controls, as in the caller's races (race_pads.c; its
   recorded demos play themselves).  The pause menu's "quit game" closes the
   app, as the game's quit does (BrExt_10038F30); on the iPhone, where an app
   does not close itself, it ends the race */
static void own_race(int racing)
{
    static int on;
    int drive = racing && g_brRaceRules.mode != 4;
    if (drive != on) {
        br_race_pads_on(drive);
        on = drive;
    }
    if (drive) {
        static int n;
        BrDriverCar *c = g_aBrRaceDriver[0].pCar;
        controls();
        if (c && ++n % 300 == 0 && getenv("RM_LOG"))   /* (a check: the player's car under the app's controls) */
            fprintf(stderr, "handoff: own race frame %d lights %d lap %d along %.0f speed %.1f\n", n,
                    g_brRaceLights, c->lap, c->fFF4, hypotf(c->f1024.x, c->f1024.y));
#if defined(__APPLE__) && TARGET_OS_IPHONE
        if (DAT_105ccb68[12] == 2)
            DAT_105ccb68[12] = 1;
#endif
    }
}

/* the iPhone's menus (RM_PHONE=1: a check of them anywhere): no Quit, no
 * Multiplayer (BrUiRootEnter_100425E0.cpp), no Quit Game on the pause menu */
int rm_phone(void)
{
#if defined(__APPLE__) && TARGET_OS_IPHONE
    return 1;
#else
    static int e = -1;
    if (e < 0)
        e = getenv("RM_PHONE") != NULL;
    return e;
#endif
}

int rm_pause_rows(void) { return rm_phone() ? 6 : 7; }

/* Boss Rally's strings that name a key, naming instead what the first player has
 * that control on (controls.h): the replay's "Press ESC to exit" (244) is the
 * Pause / Start control, which leaves it (pad_keys, below); the key, or the pad
 * button while a pad is plugged in; on the iPhone a tap.  NULL: the game's own
 * string. */
int rm_bind(int player, int act, int kind);
const char *rm_bind_label(int kind, int code, char *buf, int n);
const char *rm_br_string(int id)
{
    static char buf[64];
    char name[32];
    host_pad p;
    int pad, code;
    const char *e = getenv("RM_PROMPT");        /* (a check: key, pad or touch) */
    if (id != 244)
        return NULL;
#if defined(__APPLE__) && TARGET_OS_IPHONE
    if (!e)
        e = "touch";
#endif
    if (e && !strcmp(e, "touch"))
        return "%wwTap to exit";             /* (a tap is Return: taps) */
    pad = e ? !strcmp(e, "pad") : host_pad_read(&p);
    code = rm_bind(0, 10, pad);                /* RM_ACT_START: its key (0) or pad button (1) */
    if (pad ? code < 0 : code <= 0) {
        pad = !pad;
        code = rm_bind(0, 10, pad);
    }
    rm_bind_label(pad, code, name, sizeof name);
    snprintf(buf, sizeof buf, "%%wwPress %s to exit",
             !strcmp(name, "ESCAPE") ? "ESC" : !strncmp(name, "PAD ", 4) ? name + 4 : name);
    return buf;
}

/* the first player's controls (controls.h) on Boss Rally's own screens, which
 * read keys: on its menus while they are the app's, Up, Down and the steering
 * are the arrow keys and Accelerate / Select Return (the menus have no key for
 * Back: the page's Back row is chosen as any other); in a race Pause / Start is
 * the game's pause key, Escape; on the pause menu the arrows and Return as on
 * the menus (straight into its presses, rm_pause_note), and Brake / Back or
 * Pause / Start close it (Back on the Remaster Options page: that page); in a demo Pause /
 * Start is Escape, which leaves it (the menu has no key
 * for that: its Continue row, or a network player's resume, the same steps,
 * DAT_105ccb68[6]).  Each
 * key is held as long as its control is (the menus repeat a held arrow
 * themselves); a control already held when the screen comes up counts only
 * once it has been let go (the accelerator held into the pause menu). */
enum { PK_NONE, PK_MENUS, PK_RACE, PK_PAUSED, PK_DEMO };
void rm_player_menu(int player, const unsigned char *dik, const host_pad *pad, unsigned short *buttons,
                    int *sx, int *sy);   /* the app's (controls.h) */

static void pad_keys(int ctx)
{
    static const struct { int vk, dik; } k[] = {
        { 0x26, 0xC8 }, { 0x28, 0xD0 }, { 0x25, 0xCB }, { 0x27, 0xCD }, { 0x0D, 0x1C }, { 0x1B, 0x01 },
    };
    static int was[7], frame, prev = PK_NONE;
    static unsigned block;
    int want[7] = { 0, 0, 0, 0, 0, 0, 0 }, i, has, x, y;
    unsigned short b;
    unsigned now = 0;
    host_pad p;
    const char *e = getenv("RM_MENUPAD");
    has = host_pad_read(&p);
    frame++;
    if (e) {        /* (a check: RM_MENUPAD=F:buttons[;...], the pad's buttons from frame F on) */
        unsigned v0 = 0;
        for (; *e; e = strchr(e, ';') ? strchr(e, ';') + 1 : e + strlen(e)) {
            int f = 0;
            unsigned v = 0;
            if (sscanf(e, "%d:%x", &f, &v) == 2 && f <= frame)
                v0 = v;
        }
        memset(&p, 0, sizeof p);
        p.buttons = v0;
        has = 1;
    }
    rm_player_menu(0, g_plat_dik, has ? &p : NULL, &b, &x, &y);
    if (ctx == PK_MENUS || ctx == PK_PAUSED) {
        want[0] = y > 40;
        want[1] = y < -40;
        want[2] = x < -40;
        want[3] = x > 40;
        want[4] = (b & 0x8000) != 0;                      /* A */
    }
    if (ctx == PK_PAUSED)
        want[6] = (b & 0x4000) || (b & 0x1000);           /* B, Start: resume */
    if (ctx == PK_RACE || ctx == PK_DEMO)
        want[5] = (b & 0x1000) != 0;                      /* Start (a demo: leaves it, as Escape) */
    for (i = 0; i < 7; i++)
        now |= (unsigned)want[i] << i;
    if (ctx != prev) {
        block = now;
        prev = ctx;
    }
    block &= now;
    now &= ~block;
    for (i = 0; i < 7; i++) {
        int on = (now >> i) & 1;
        if (on != was[i] && ctx == PK_PAUSED) {
            /* the pause menu: its presses straight in (up, down, left, right, Return,
               and Back: the Remaster Options page's close, else the race's resume) */
            static const int k_bits[5] = { 0x1000, 0x2000, 1, 2, 0x8000 };
            if (on && i < 5)
                rm_pause_note(k_bits[i]);
            else if (on && i == 6) {
                if (rm_pause_active() && (b & 0x4000))
                    rm_pause_note(0x4000);
                else
                    DAT_105ccb68[6] = 1;
            }
            was[i] = on;
            continue;
        }
        if (on != was[i]) {
            if (i == 5 && ctx == PK_DEMO) {
                if (on)
                    br_race_key(5);       /* (a demo's skip: the host's keys come this way) */
            } else if (i < 6)
                plat_script_key(k[i].vk, k[i].dik, on);
            else if (on)
                DAT_105ccb68[6] = 1;
            was[i] = on;
        }
    }
}

/* The races' tick rate: Top Gear Rally Remastered runs Boss Rally's simulation
 * at 60 ticks a second where the game ran 30 (RM_TICK=30: the game's own, its
 * values exactly).  What the game made a constant of its 30 (the physics' step,
 * the tick clock's period, the frame clock's fixed step, the blend between two
 * ticks' snapshots) asks here instead, a network race's too (its clock, the
 * ticks its packets carry, counts at the rate: rm_ms_to_ticks). */
static int s_tick_hz;

int rm_tick_hz(void)
{
    if (!s_tick_hz) {
        const char *e = getenv("RM_TICK");
        s_tick_hz = e && atoi(e) == 30 ? 30 : 60;
    }
    return s_tick_hz;
}

/* the step, seconds: 1/30 the game's own float */
float rm_tick_dt(void) { return rm_tick_hz() == 30 ? 0.03333333507180214f : 0.016666667535901070f; }

/* a fraction taken every tick, the same a second at this rate (rm_tick.h) */
float rm_per_tick(float k)
{
    if (rm_tick_hz() == 30)
        return k;
    return (float)(1.0 - pow(1.0 - (double)k, 30.0 / rm_tick_hz()));
}

/* an amount added every tick, the same a second at this rate (rm_tick.h) */
float rm_step(float x)
{
    if (rm_tick_hz() == 30)
        return x;
    return (float)((double)x * 30.0 / rm_tick_hz());
}

/* a factor applied every tick, the same a second at this rate (rm_tick.h) */
float rm_per_tick_mul(float s)
{
    if (rm_tick_hz() == 30 || !(s > 0.0f))
        return s;
    return (float)pow((double)s, 30.0 / rm_tick_hz());
}

/* milliseconds as ticks of the rate, and back: the game's own sums at 30
   (3 a tenth of a second, a tick 33 ms; BrTicks30FromMs) */
unsigned rm_ms_to_ticks(unsigned ms)
{
    if (rm_tick_hz() == 30)
        return 3u * (ms / 100u) + (ms % 100u) / 33u;
    return (unsigned)((unsigned long long)ms * (unsigned)rm_tick_hz() / 1000u);
}

int rm_ticks_to_ms(int t)
{
    if (rm_tick_hz() == 30)
        return (t % 3) * 0x21 + (t / 3) * 100;
    return (int)((long long)t * 1000 / rm_tick_hz());
}

/* the race ticks counted (plat_race_frame), for rm_tick_whole */
static unsigned s_ticks;

int rm_tick_whole(void) { return s_ticks % (unsigned)(rm_tick_hz() / 30) == 0; }

/* the tick clock's period, milliseconds, k = 0, 1, 2 in turn (the game's 33, 33, 34) */
int rm_tick_ms(int k)
{
    static const int t30[3] = { 33, 33, 34 }, t60[3] = { 17, 17, 16 };
    return rm_tick_hz() == 30 ? t30[k] : t60[k];
}

/* BR_TICKLOG=1 (a check): on stderr, each race tick from the racing light on, the race
 * clock and the first cars' place on the track, position and speed; two runs'
 * files compare tick for tick (the same race at another tick rate: by clock) */
static void ticklog(void)
{
    static int on = -1;
    FILE *f = stderr;
    int k, n;
    if (on < 0)
        on = getenv("BR_TICKLOG") != NULL;
    if (!on || g_pfnStep != (BrGameStepFn)BrRaceStep || g_brRaceLights < 4 || DAT_105ccb68[8] != 0)
        return;
    fprintf(f, "TICK %u %.4f", (*(unsigned int *)&DAT_106ec768), g_brRaceFlyStep);
    n = g_BrCarCount < 4 ? g_BrCarCount : 4;
    for (k = 0; k < n; k++) {
        BrDriverCar *c = &g_aBrRaceCar[k];
        fprintf(f, " | %d %.3f %.3f %.3f %.3f %.3f %.3f %.3f", c->lap, c->fFF4, c->pos.x, c->pos.y, c->pos.z,
                c->f1024.x, c->f1024.y, c->f1024.z);
    }
    if (getenv("BR_TICKLOG_CAR")) {             /* the first car's drive: gear, revs, wheels down, upright */
        BrDriverCar *c = &g_aBrRaceCar[0];
        fprintf(f, " || g%d rpm %.0f w %d %d %d %d up %.2f rs %d", c->fE70, c->f0E24,
                *(int *)&c->aBody[0].rb.child[0]->f1B4, *(int *)&c->aBody[0].rb.child[1]->f1B4,
                *(int *)&c->aBody[0].rb.child[2]->f1B4, *(int *)&c->aBody[0].rb.child[3]->f1B4,
                c->aBody[0].rb.m.m[2][2], *(int *)&c->aBody[0].f01F8);
        fprintf(f, " spin %.2f %.2f %.2f %.2f tq %.1f %.1f fe68 %.1f", c->aBody[0].rb.child[0]->f1C4,
                c->aBody[0].rb.child[2]->f1C4, c->aBody[0].rb.child[0]->f1CC, c->aBody[0].rb.child[2]->f1CC,
                c->aBody[0].rb.child[0]->f1CC, c->aBody[0].rb.child[2]->f1CC, c->fE68);
        fprintf(f, " steer %.4f wa %.4f %.4f yaw %.3f", c->f0E20, c->aBody[0].rb.child[0]->f1C0,
                c->aBody[0].rb.child[2]->f1C0, c->aBody[0].rb.st.angVel.z);
    }
    fputc('\n', f);
}

/* BR_HANDOFF_SHOTS=dir:F,...: the race's own frame at race frame F (a check) */
static void race_shots(void)
{
    char dir[512], path[600];
    const char *e = getenv("BR_HANDOFF_SHOTS"), *c;
    int brr_shot(const char *path);
    if (!e || !(c = strchr(e, ':')) || c - e >= (int)sizeof dir)
        return;
    memcpy(dir, e, (size_t)(c - e));
    dir[c - e] = 0;
    for (c++; *c; c = strchr(c, ',') ? strchr(c, ',') + 1 : c + strlen(c))
        if (atoi(c) == s_frames) {
            snprintf(path, sizeof path, "%s/race%05d.png", dir, s_frames);
            brr_shot(path);
        }
}

/* BrAppFrame's entry */
void plat_race_frame(void)
{
    if (g_pfnStep == (BrGameStepFn)BrRaceStep)
        s_ticks++;
    ticklog();
    rm_rumble_frame();               /* the Rumble Pak's view of the race (rm_rumble.c) */
    int state = DAT_105ccb68[21];
    int racing = g_pfnStep == (BrGameStepFn)BrRaceStep;
    init();
    taps();
    keys();
    if (s_phase == RACE)
        controls();
    if (s_phase == IDLE && s_menus)
        own_race(racing);
    if (getenv("RM_LOG") && s_phase == RACE) {   /* (a check: the pause menu's coming and going) */
        static int was_paused;
        if ((g_BrX06909B4 != 0) != was_paused)
            fprintf(stderr, "handoff: race frame %d %s\n", s_frames, g_BrX06909B4 ? "paused" : "resumed");
        was_paused = g_BrX06909B4 != 0;
    }
    pad_keys(s_menus && s_phase == IDLE && state == 4 && g_brPAA29B8 != 0 ? PK_MENUS
             : s_phase == RACE && racing && PLAYBACK ? PK_DEMO
             : s_phase == RACE && racing ? (g_BrX06909B4 != 0 ? PK_PAUSED : PK_RACE) : PK_NONE);
    switch (s_phase) {
    case IDLE:
        /* the menus are up (state 4, a phase built) */
        if (state != 4 || g_brPAA29B8 == 0)
            return;
        if (!test_request()) {
            host_mutex_lock(s_m);
            if (!s_serving) {
                host_mutex_unlock(s_m);
                return;
            }
            while (!s_pending && !s_menus)
                host_cond_wait(s_c, s_m, 0xFFFFFFFFu);
            if (!s_pending) {         /* the app's menus: they run */
                host_mutex_unlock(s_m);
                rm_menuopts_refresh();
                return;
            }
            host_mutex_unlock(s_m);
        }
        configure(&s_req);
        ((BrPhase_ *)g_brPAA29B8)->f68 = 0;
        BrPhaseShutdown_10048B20(0);
        configure(&s_req);           /* (after the shutdown, which may write the mode) */
        s_got = 0;
        s_underway = 0;
        s_phase = START;
        return;
    case START:
        if (!racing)
            return;
        /* before the step's first frame loads the track: a championship round
           took its track and weather from Boss Rally's own season schedule
           (BrSelLookup, in BrGlRaceStart) */
        if (!PLAYBACK) {             /* (the demos' recordings and the ceremony set their own) */
            g_Br0B380C = s_req.track + (s_req.mirrored ? 6 : 0);
            g_226e80 = s_req.weather;
            DAT_104b15e8 = s_req.weather;
            g_CBE8 = s_req.laps > 0 ? s_req.laps : 3;
        }
        s_skip = 0;
        s_key = -1;
        s_frames = 0;
        s_raced_track = g_Br0B380C;
        s_raced_weather = DAT_104b15e8;
        s_raced_mode = g_brRaceRules.mode;
        g_brRaceNEntrant = s_req.players == 2 && s_req.mode <= HOST_RACE_PRACTICE ? 2 : 1;   /* split screen */
        br_race_pads_on(!PLAYBACK);  /* the players drive with the app's controls (race_pads.c) */
        g_brRaceTick = 1;            /* no waiting for the start key */
        s_phase = RACE;
        return;
    case RACE:
        /* the ceremony over: Boss Rally goes on to its own ending (mode 4,
           the ending's recording: BrRaceEnterOutro) or its next season
           (BrSetMode5); the caller has its own, so to the menus instead,
           as the race step goes after a demo (its BrInit220B20 step) */
        if (s_req.mode == HOST_RACE_SEASON_END &&
            ((g_brRaceRules.mode == 4 && g_5bc760 == 2) || g_pfnStep == (BrGameStepFn)BrSetMode5)) {
            g_brRaceRules.mode = 1;
            g_5bc760 = 0;
            BrGameStepSet(BrInit220B20);
        }
        if (racing && PLAYBACK) {
            g_brRaceTick = 1;
            s_frames++;
            race_shots();
            if (s_test && getenv("BR_HANDOFF_SKIP_AT") && s_frames == atoi(getenv("BR_HANDOFF_SKIP_AT")))
                s_key = BR_KEY_ENTER;
            if (s_key >= 0 && !s_skip && BrFadeIsClosing() == 0) {
                /* skipped: what the race step does on the pause key in mode 4/5 */
                s_key = -1;
                s_skip = 1;
                g_brRaceBeginMovieDone = 1;
                DAT_105ccb68[12] = 1;
                DAT_105ccb68[9] = 0;
                BrFadeSetTargetA(0, 0.2f);
                BrFadeSetTarget(0, 0.2f);
            }
            return;
        }
        if (racing) {
            g_brRaceTick = 1;
            s_frames++;
            check_taps();
            race_shots();
            if (DAT_105ccb68[12] == 2)
                DAT_105ccb68[12] = 1;   /* the pause menu's "quit game" ends the app's race, not the app */
            if ((AUTOPILOT || s_test) && s_underway && DAT_105ccb68[8] == 0 && g_brRaceLights < 5 &&
                g_BrX06909B4 != 0) {
                if (!s_ap_paused && AUTOPILOT)
                    plat_script_autopilot_off();   /* paused: its keys let go, the menu's own */
                s_ap_paused = 1;
            } else if ((AUTOPILOT || s_test) && s_underway && DAT_105ccb68[8] == 0 && g_brRaceLights < 5) {
                s_ap_paused = 0;
                static int n;
                BrDriverCar *c = g_aBrRaceDriver[0].pCar;
                int rev = getenv("BR_HANDOFF_REVERSE_AT") ? atoi(getenv("BR_HANDOFF_REVERSE_AT")) : 0;
                if (AUTOPILOT && !(rev && n >= rev))
                    plat_script_autopilot();
                if (rev && n == rev) {                /* the reverse action held from here (a check) */
                    plat_script_autopilot_off();
                    plat_script_key(0x28, 0xD0, 1);
                }
                if (rev && n >= rev && n < rev + 600 && n % 30 == 0 && c)
                    fprintf(stderr, "handoff: reverse held %d: along %.0f speed %.1f\n", n - rev, c->fFF4,
                            hypotf(c->f1024.x, c->f1024.y));
                if (c && ++n % 300 == 0)
                    fprintf(stderr, "handoff: frame %d lights %d lap %d along %.0f speed %.1f\n", n,
                            g_brRaceLights, c->lap, c->fFF4, hypotf(c->f1024.x, c->f1024.y));
            }
            if (DAT_105ccb68[8] == 0 && g_brRaceLights == 4) {
                s_underway = 1;
                if (g_BrX06909B4 == 0)       /* (not paused) */
                    finish_cheat();
            }
            if (!s_got && s_underway && DAT_105ccb68[8] == 0 && g_brRaceLights >= 5) {
                result();
                s_got = 1;
                /* the replay that follows is one view, the first player's: its set-up must
                   not look for the second car's equipment (the PC game never replayed two) */
                g_brRaceNEntrant = 1;
                if (AUTOPILOT)
                    plat_script_autopilot_off();
            }
            if (s_got && DAT_105ccb68[8] != 0)
                DAT_10226a50 = 1;    /* the replay: Escape leaves it */
            /* the result is the caller's: its championship round is not one of Boss
               Rally's own season.  The finish's BrCarStateSave flag would send the
               race's end through BrRaceSelFromMenu, which counts the round in Boss
               Rally's standings and, at its season's last, has the front end start
               Boss Rally's end-of-season ceremony (BrSeasonApply: mode 5) before the
               caller has the result; without it the end goes to the menus as an
               arcade race's does (BrInit220B20) */
            if (s_got)
                DAT_105ccb60 = 0;
            /* nor Boss Rally's replay: the race's end goes on to the menus, as the
               caller's way back (the replay would restore the race from snapshots the
               port never takes: BrWrap_10067940, the per-car slots; a car without its
               equipment, and the race's set-up reads it).  The caller replays nothing */
            if (s_got)
                DAT_105ccb68[9] = 0;
            return;
        }
        if (PLAYBACK) {
            memset(&s_res, 0, sizeof s_res);
            s_res.outcome = s_skip ? HOST_RACE_LEFT : HOST_RACE_FINISHED;
        } else if (!s_got) {         /* left from the pause menu */
            memset(&s_res, 0, sizeof s_res);
            s_res.outcome = HOST_RACE_LEFT;
            s_res2 = s_res;
        }
        s_phase = BACK;
        br_race_pads_on(0);
        return;
    case BACK:
        if (state != 4 || g_brPAA29B8 == 0)
            return;                  /* the session restarts, the menus are built */
        s_phase = IDLE;
        finish();
        return;
    }
}
