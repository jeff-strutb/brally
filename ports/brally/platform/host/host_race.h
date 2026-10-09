/* host_race.h: one game's race run by another game's engine, through the
 * host.  The iPhone app (ports/tgrally/ios) runs Top Gear Rally's front end
 * and, with the Remastered flag on, hands each race to Boss Rally's engine
 * (ports/brally/platform/common/race_handoff.c), then gives the outcome back
 * to Top Gear Rally's results, season and records
 * (ports/tgrally/src/racing/handoff.c).
 *
 * The two games share their tracks (Boss Rally's .trk files are the N64
 * track images): the courses are numbered alike, mirrored apart. Times are
 * seconds. */
#ifndef HOST_RACE_H
#define HOST_RACE_H

enum {
    HOST_RACE_CHAMPIONSHIP,         /* a season round: 20 drivers */
    HOST_RACE_ARCADE,               /* against one car, beating the clock */
    HOST_RACE_TIME_ATTACK,          /* alone, for the records */
    HOST_RACE_PRACTICE,             /* alone, never finishing: left from the pause menu */
    HOST_RACE_ATTRACT,              /* a recorded race played back: `piece` 0 the intro,
                                       1 the credits, 2 the ending (the last season won) */
    HOST_RACE_SEASON_END            /* the end-of-season ceremony (its own track) */
};

typedef struct HostRace {
    int mode;                       /* HOST_RACE_* */
    int track;                      /* 0 desert, 1 mountain, 2 coast, 3 strip mine, 4 jungle */
    int mirrored;
    int weather;                    /* 0 sunny, 1 fog, 2 rain, 3 snow, 4 night */
    int laps;
    int car;                        /* Boss Rally's car: 0 ce 1 es 2 ns 3 rs 4 sp 5 ps 6 m3 7 ip
                                       8 ld 9 hm 10 mt 11 cu 12 bb 13 pj 14 tr 15 mn */
    int handling;                   /* 0..2 */
    int automatic;                  /* 1: automatic transmission */
    int tires;                      /* 0..2: slippy, normal, grippy */
    int suspension;                 /* 0..2: softer, normal, harder */
    int piece;                      /* HOST_RACE_ATTRACT: which recording */
} HostRace;

enum {
    HOST_RACE_FINISHED,             /* the player took the flag */
    HOST_RACE_LEFT                  /* the player quit the race (or skipped the demo) */
};

typedef struct HostRaceResult {
    int outcome;                    /* HOST_RACE_FINISHED / _LEFT */
    int place;                      /* 0: first */
    int laps;                       /* laps completed */
    float total;                    /* the race's time */
    float best;                     /* the best lap's time */
    int best_lap;                   /* which lap, 0 first */
    float lap[12];                  /* each lap's time */
} HostRaceResult;

/* a key for the race engine's own menus (its pause menu) */
enum { BR_KEY_UP, BR_KEY_DOWN, BR_KEY_LEFT, BR_KEY_RIGHT, BR_KEY_ENTER, BR_KEY_ESCAPE };

/* the race engine's entry: runs the race, fills *res; 0 if it cannot */
typedef int (*host_race_fn)(const HostRace *race, HostRaceResult *res);

#endif
