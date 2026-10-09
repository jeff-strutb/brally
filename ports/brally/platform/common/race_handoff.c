/* race_handoff.c: Boss Rally's races, run for another game's front end
 * (host_race.h: the iPhone app runs Top Gear Rally's menus and hands each
 * race here).
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
 * check of the whole round trip).
 * Built with the core's flags, for its types. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "br_addr32.h"
#include "br_coretypes.h"
#include "slice3_41.h"     /* BrDriverCar, BrDriver */
#include "br_phase.h"
#include "br_race.h"         /* g_brRaceRules */
#include "br_gamestep.h"     /* g_pfnStep */
#include "slice2_25.h"       /* g_brPAA29B8 */
#include "../host/host_race.h"

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

enum { IDLE, START, RACE, BACK };

static host_mutex *s_m;
static host_cond *s_c;
static int s_serving;               /* a caller asked for races: wait for them at the menus */
static int s_pending, s_done;
static HostRace s_req;
static HostRaceResult s_res;
static int s_phase = IDLE, s_got, s_test;
static int s_skip;                   /* the demo or ceremony was skipped */
static int s_frames;                 /* frames the race step ran (a check's report) */
#define PLAYBACK (s_req.mode == HOST_RACE_ATTRACT || s_req.mode == HOST_RACE_SEASON_END)
#define AUTOPILOT ((s_test || getenv("BR_HANDOFF_AUTOPILOT")) && !getenv("BR_PADFAKE"))
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
    s_done = 0;
    host_mutex_unlock(s_m);
    return 1;
}

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
    if (sscanf(e, "%d,%d,%d,%d,%d,%d,%d", &r.mode, &r.track, &r.mirrored, &r.weather, &r.laps, &r.car,
               &r.piece) < 6)
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
    /* touch reaches the game as the joystick (dx.c), in the gamepad profile (2:
       the stick's x steers, button 0 accelerates, button 1 brakes); the
       headless check drives the keys (profile 0) */
    g_BrCtrlCfg.active = AUTOPILOT ? 0 : 2;
    g_BrPadModeBytes = (const unsigned char *)&g_BrCtrlCfg.profile[g_BrCtrlCfg.active];
}

static void result(void)
{
    BrDriverCar *car = g_aBrRaceDriver[0].pCar;
    int i, n;
    memset(&s_res, 0, sizeof s_res);
    s_res.outcome = HOST_RACE_FINISHED;
    if (!car)
        return;
    s_res.place = car->fFF8;
    s_res.laps = car->lap;
    s_res.total = car->tFinal;
    s_res.best = car->tBest;
    s_res.best_lap = car->lapBest;
    n = car->lap < 12 ? car->lap : 12;
    for (i = 0; i < n; i++)
        s_res.lap[i] = car->aLapTime[i];
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
    return s_phase == RACE && g_pfnStep == (BrGameStepFn)BrRaceStep && DAT_105ccb68[8] == 0 &&
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

/* BrAppFrame's entry */
void plat_race_frame(void)
{
    int state = DAT_105ccb68[21];
    int racing = g_pfnStep == (BrGameStepFn)BrRaceStep;
    init();
    keys();
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
            while (!s_pending)
                host_cond_wait(s_c, s_m, 0xFFFFFFFFu);
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
            if (DAT_105ccb68[12] == 2)
                DAT_105ccb68[12] = 1;   /* the pause menu's "quit game" ends the app's race, not the app */
            if ((AUTOPILOT || s_test) && s_underway && DAT_105ccb68[8] == 0 && g_brRaceLights < 5) {
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
            if (DAT_105ccb68[8] == 0 && g_brRaceLights == 4)
                s_underway = 1;
            if (!s_got && s_underway && DAT_105ccb68[8] == 0 && g_brRaceLights >= 5) {
                result();
                s_got = 1;
                if (AUTOPILOT)
                    plat_script_autopilot_off();
            }
            if (s_got && DAT_105ccb68[8] != 0)
                DAT_10226a50 = 1;    /* the replay: Escape leaves it */
            return;
        }
        if (PLAYBACK) {
            memset(&s_res, 0, sizeof s_res);
            s_res.outcome = s_skip ? HOST_RACE_LEFT : HOST_RACE_FINISHED;
        } else if (!s_got) {         /* left from the pause menu */
            memset(&s_res, 0, sizeof s_res);
            s_res.outcome = HOST_RACE_LEFT;
        }
        s_phase = BACK;
        return;
    case BACK:
        if (state != 4 || g_brPAA29B8 == 0)
            return;                  /* the session restarts, the menus are built */
        s_phase = IDLE;
        finish();
        return;
    }
}
