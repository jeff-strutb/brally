/* main.c: Top Gear Rally Remastered, the process.  Top Gear Rally runs from
 * launch on this thread (its window loop, os/main.c) and owns the menus;
 * with Boss Rally's menus chosen (the Remaster Options' Menus) its game
 * thread waits while Boss Rally's are in front (menus_engine);
 * each race the player starts is handed to Boss Rally's engine
 * (host_race.h), which boots on a thread of its own once the window is open
 * (mux.c: its front end's music is the menus' when the PC's is chosen)
 * and waits at its menus between races (race_handoff.c).  The game that runs the race is put in front for it
 * (mux.c).  On the iPhone this runs as rm_main on a thread of the app's
 * (ios/Sources/App.swift). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <pthread.h>
#endif

#include "host.h"
#include "host_race.h"
#include "mux.h"

/* the Top Gear Rally object (ports/tgrally/link.sh, HOST=lib) */
int  tgr_main(int argc, char **argv);
void tgr_race_engine(host_race_fn fn);
void tgr_menus_engine(void (*fn)(void));
void tgr_other_engine(void (*fn)(int menus));
/* the Top Gear Rally object from Japan's cartridge (link.sh: rm_tgrp.h names) */
int  tgrp_main(int argc, char **argv);
void tgrp_race_engine(host_race_fn fn);
void tgrp_menus_engine(void (*fn)(void));
void tgrp_other_engine(void (*fn)(int menus));
void tgrp_boot_to_menu(int on);
/* the Boss Rally object (ports/brally/link64.sh, HOST=lib) */
int  br_main(int argc, char **argv);
void br_race_serve(void);
int  br_race_run(const HostRace *race, HostRaceResult *res);
void br_menus_run(void);

#define BR_STACK (16u << 20)            /* its main thread's, as the iPhone app gives it */

#ifdef _WIN32
static DWORD WINAPI br_thread(LPVOID p)
#else
static void *br_thread(void *p)
#endif
{
    char name[] = "brally";
    char *argv[] = { name, NULL };
    (void)p;
    br_main(1, argv);
    return 0;
}

/* Boss Rally's process, told first that races will be asked of it */
void rm_br_start(void)
{
    static int started;
    if (started)
        return;
    started = 1;
    br_race_serve();
#ifdef _WIN32
    CloseHandle(CreateThread(NULL, BR_STACK, br_thread, NULL, STACK_SIZE_PARAM_IS_A_RESERVATION, NULL));
#else
    {
        pthread_t t;
        pthread_attr_t a;
        pthread_attr_init(&a);
        pthread_attr_setstacksize(&a, BR_STACK);
        pthread_create(&t, &a, br_thread, NULL);
        pthread_attr_destroy(&a);
        pthread_detach(t);
    }
#endif
}

/* Top Gear Rally's race engine: called on its game thread, which waits here
 * for the race's outcome */
static int race_engine(const HostRace *race, HostRaceResult *res)
{
    int r;
    if (getenv("RM_LOG"))
        fprintf(stderr, "remastered: race mode %d track %d mirrored %d weather %d laps %d car %d handling %d auto %d tires %d susp %d piece %d players %d car2 %d\n",
                race->mode, race->track, race->mirrored, race->weather, race->laps, race->car, race->handling,
                race->automatic, race->tires, race->suspension, race->piece, race->players, race->car2);
    rm_br_start();
    mux_front(MUX_BR);
    r = br_race_run(race, res);
    mux_front(mux_tgr_active_game());   /* the Top Gear Rally that asked */
    if (getenv("RM_LOG")) {             /* each race handed over and its outcome */
        int i;
        fprintf(stderr, "remastered: mode %d track %d mirrored %d weather %d laps %d car %d: %s, place %d, laps %d, total %.2f, best %.2f (lap %d):",
                race->mode, race->track, race->mirrored, race->weather, race->laps, race->car,
                res->outcome == HOST_RACE_FINISHED ? "finished" : "left", res->place + 1, res->laps,
                res->total, res->best, res->best_lap + 1);
        for (i = 0; i < res->laps && i < 12; i++)
            fprintf(stderr, " %.2f", res->lap[i]);
        if (race->players == 2)
            fprintf(stderr, "; player 2: place %d, laps %d, total %.2f", res[1].place + 1, res[1].laps, res[1].total);
        fprintf(stderr, "\n");
    }
    return r;
}

/* Top Gear Rally's menus engine: called on a thread of its own while its
 * game thread waits (src/racing/handoff.c), Boss Rally's menus in front
 * until the player chooses Top Gear Rally's on Boss Rally's Remaster Options
 * page (race_handoff.c) */
static void menus_engine(void)
{
    if (getenv("RM_LOG"))
        fprintf(stderr, "remastered: menus: Boss Rally\n");
    rm_br_start();
    mux_front(MUX_BR);
    br_menus_run();
    mux_front(mux_tgr_active_game());
    if (getenv("RM_LOG"))
        fprintf(stderr, "remastered: menus: Top Gear Rally\n");
}

/* The two Top Gear Rallies (Menus TGR (USA), TGR (PAL/Japan)): one at a time,
 * the other's game thread waiting in its other engine (src/racing/handoff.c)
 * until the turn is its again.  The first is the app's from launch; the
 * second starts on a thread of its own the first time it is asked for: at
 * its intro if the app launched with its menus kept, else at its main menu. */
#define RM_MENUS_AT_LAUNCH 0x100        /* (handoff.c) */
static host_mutex *s_om;
static host_cond *s_oc;
static volatile int s_turn = MUX_TGR;

#ifdef _WIN32
static DWORD WINAPI tgrp_thread(LPVOID p)
#else
static void *tgrp_thread(void *p)
#endif
{
    static char name[] = "tgrally-jpn", args[2048];
    char *argv[64] = { name, NULL }, *e = getenv("RM_TGRP_ARGS"), *w;
    int argc = 1;
    (void)p;
    if (e) {                            /* (checks: RM_TGRP_ARGS="--script F --shots D ...", its own run's) */
        snprintf(args, sizeof args, "%s", e);
        for (w = strtok(args, " "); w && argc < 63; w = strtok(NULL, " "))
            argv[argc++] = w;
        argv[argc] = NULL;
    }
    tgrp_main(argc, argv);
    return 0;
}

static void tgrp_start(int at_launch)
{
    tgrp_boot_to_menu(!at_launch);
#ifdef _WIN32
    CloseHandle(CreateThread(NULL, BR_STACK, tgrp_thread, NULL, STACK_SIZE_PARAM_IS_A_RESERVATION, NULL));
#else
    {
        pthread_t t;
        pthread_attr_t a;
        pthread_attr_init(&a);
        pthread_attr_setstacksize(&a, BR_STACK);
        pthread_create(&t, &a, tgrp_thread, NULL);
        pthread_attr_destroy(&a);
        pthread_detach(t);
    }
#endif
}

/* hand the turn to `to` and wait until it is `me`'s again */
static void turn_to(int me, int to)
{
    host_mutex_lock(s_om);
    s_turn = to;
    host_cond_broadcast(s_oc);
    host_mutex_unlock(s_om);
    mux_tgr_active(to);
    mux_front(to);
    host_mutex_lock(s_om);
    while (s_turn != me)
        host_cond_wait(s_oc, s_om, 100);
    host_mutex_unlock(s_om);
    mux_tgr_active(me);
    mux_front(me);
}

/* the USA cartridge's game, from its options (or at launch): Japan's menus */
static void other_from_usa(int menus)
{
    static int started;
    if (getenv("RM_LOG"))
        fprintf(stderr, "remastered: menus: Top Gear Rally (PAL/Japan)\n");
    if (!started) {
        started = 1;
        host_mutex_lock(s_om);
        s_turn = MUX_TGRP;
        host_mutex_unlock(s_om);
        tgrp_start((menus & RM_MENUS_AT_LAUNCH) != 0);
    }
    turn_to(MUX_TGR, MUX_TGRP);
    if (getenv("RM_LOG"))
        fprintf(stderr, "remastered: menus: Top Gear Rally (USA)\n");
}

/* Japan's cartridge's game, from its options: the USA's menus */
static void other_from_jpn(int menus)
{
    (void)menus;
    turn_to(MUX_TGRP, MUX_TGR);
}

int main(int argc, char **argv)
{
    s_om = host_mutex_new();
    s_oc = host_cond_new();
    tgr_other_engine(other_from_usa);
    tgrp_other_engine(other_from_jpn);
    tgrp_race_engine(race_engine);
    tgrp_menus_engine(menus_engine);
    host_set_app_name("Top Gear Rally Remastered", "Top Gear Rally Remastered");
    tgr_race_engine(race_engine);
    tgr_menus_engine(menus_engine);
    return tgr_main(argc, argv);
}
