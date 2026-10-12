/* main.c: Top Gear Rally Remastered, the process.  Top Gear Rally runs from
 * launch on this thread (its window loop, os/main.c) and owns the menus;
 * with its Remastered profile's TGR_FLAG_BR_RACES on, each race the player
 * starts is handed to Boss Rally's engine (host_race.h), which boots on a
 * thread of its own at the first one and waits at its menus between races
 * (race_handoff.c).  The game that runs the race is put in front for it
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
/* the Boss Rally object (ports/brally/link64.sh, HOST=lib) */
int  br_main(int argc, char **argv);
void br_race_serve(void);
int  br_race_run(const HostRace *race, HostRaceResult *res);

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
static void br_start(void)
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
        fprintf(stderr, "remastered: race mode %d track %d mirrored %d weather %d laps %d car %d handling %d auto %d tires %d susp %d piece %d\n",
                race->mode, race->track, race->mirrored, race->weather, race->laps, race->car, race->handling,
                race->automatic, race->tires, race->suspension, race->piece);
    br_start();
    mux_front(MUX_BR);
    r = br_race_run(race, res);
    mux_front(MUX_TGR);
    if (getenv("RM_LOG")) {             /* each race handed over and its outcome */
        int i;
        fprintf(stderr, "remastered: mode %d track %d mirrored %d weather %d laps %d car %d: %s, place %d, laps %d, total %.2f, best %.2f (lap %d):",
                race->mode, race->track, race->mirrored, race->weather, race->laps, race->car,
                res->outcome == HOST_RACE_FINISHED ? "finished" : "left", res->place + 1, res->laps,
                res->total, res->best, res->best_lap + 1);
        for (i = 0; i < res->laps && i < 12; i++)
            fprintf(stderr, " %.2f", res->lap[i]);
        fprintf(stderr, "\n");
    }
    return r;
}

int main(int argc, char **argv)
{
    host_set_app_name("Top Gear Rally Remastered", "Top Gear Rally Remastered");
    tgr_race_engine(race_engine);
    return tgr_main(argc, argv);
}
