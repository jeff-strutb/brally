/* handoff.c (port): a race run by another engine (host_race.h).
 *
 * With TGR_FLAG_BR_RACES on and a host that carries Boss Rally's engine (the
 * iPhone app sets it with tgr_race_engine), a race the player starts from
 * the menus is not run here: at BrRaceTick's first call, before any of its
 * set-up, the race the menus chose (track, mirror, weather, laps, car and
 * its set-up) is handed over and this thread waits.  The two games share
 * their tracks and number their cars, weather, tires and suspension alike.
 *
 * The music is this game's: the track's own, started as the race's set-up
 * does (BrMusicLoadTrack), at full level at once (the fade is stepped by the
 * game loop, which waits; the results screen fades in again after).  The other
 * engine runs on a host thread while this one waits on a message queue, an
 * OS wait like any other, so the game's other threads (the audio) and its
 * clock run on and the music plays under the other engine's race.
 *
 * The outcome comes back as the race's own end would leave it: the player's
 * car record (place, total, laps, each lap, the best lap) and the lap and
 * course records (as BrRaceGateStep sets them, rank.c), then
 * BrRaceResultSave, and from there the post-race flow of racetick.c: the
 * results screen (BrRaceResultRestore adds a championship round's points and
 * place), or the main menu when the player quit.  What only this engine's
 * race could make is left out: no instant replay (it replays this engine's
 * physics) and no Time Attack ghost (an input recording for them).
 *
 * The attract demos and the season-end ceremony go over too, to Boss Rally's
 * own (demo A its intro, B its credits, C its ending; the ceremony on its
 * end-of-season track), and what follows each is this game's, as after its
 * own (racetick.c's end: the next season's save prompt, the options, the
 * intro, or the main menu when the player skipped the demo).
 *
 * Not handed over: two players (Boss Rally races one locally: where the
 * races are handed over the 1P/2P screen offers one, tgr_race_handoff_on),
 * and the replay. */
#include "tgr/common.h"
#include "tgr/car.h"
#include "tgr/season.h"
#include "tgr_view.h"
#include "host_race.h"

/* -- declarations -- */
extern int D_802707C8;                  /* the race is set up */
extern int D_80270788;                  /* this is the instant replay */
extern int D_8027078C;                  /* play the replay after */
extern int D_802707A8;                  /* a new Time Attack record (its ghost to save) */
extern int D_80270790;                  /* results are ready */
extern int D_8026FF18;                  /* the game mode */
extern int D_8026FF08;                  /* human players */
extern int D_8028B940;                  /* the track: 0-4, 5-9 mirrored */
extern int D_8028C800;                  /* the weather */
extern int D_8028B304;                  /* laps in the race */
extern int D_8028A8AC;                  /* the track is mirrored */
extern BrCar D_8031B760[];
extern int D_8026FF1C;                  /* which demo */
extern int D_80315E70;                  /* the demo was skipped */
extern int D_802724F4;                  /* the save screen's kind (1: the season) */
void BrScreenFlush2Layout1(void);
void BrSeasonPickRace(void);
void BrLoadSaveScreen(void);
void BrOptionsScreen(void);
void BrIntroScreen(void);
void BrDemoRaceStartC(void);
void BrRaceResultSave(void);
void BrRaceResultRestore(void);
void BrModeSet(void (*mode)(void));
void BrResultsRun(void);
void BrMainMenu(void);
void BrMusicLoadTrack(void);
extern float D_8028B760;                /* the music fade's target */
extern float D_8028B768;                /* and its level */
extern unsigned char D_802A49C8;        /* the level the mixer reads (BrFadeStep writes it) */
typedef struct host_thread host_thread;  /* the host's (host.h) */
host_thread *host_thread_start(void *(*fn)(void *), void *arg);
void tgr_os_lock(void);                  /* the platform's (os/plat.h) */
void tgr_os_unlock(void);
unsigned long long tgr_count(void);
void tgr_post_mesg_at(unsigned long long when, OSMesgQueue *mq, OSMesg msg);
char *getenv(const char *name);         /* the host's (a headless check's switch) */
/* -- end declarations -- */

static host_race_fn s_engine;

/* the race, run on a host thread: its end is a message to the waiting game thread */
static struct {
  HostRace race;
  HostRaceResult res;
  int ok;
} s_job;
static OSMesgQueue s_done;
static OSMesg s_done_buf[1];

static void *race_thread(void *arg)
{
  (void)arg;
  s_job.ok = s_engine(&s_job.race, &s_job.res);
  tgr_os_lock();
  tgr_post_mesg_at(tgr_count(), &s_done, 0);
  tgr_os_unlock();
  return 0;
}

/* TGR_HANDOFF_FAKE=place,total,best[,seconds]: a stand-in engine for headless
 * checks of this side: every race "finishes" with that place and times, the
 * laps even, after that many seconds of wall time */
void host_sleep_ms(unsigned int ms);
static int fake_engine(const HostRace *r, HostRaceResult *res)
{
  int place = 0, i, secs = 0;
  float total = 0, best = 0;
  sscanf(getenv("TGR_HANDOFF_FAKE"), "%d,%f,%f,%d", &place, &total, &best, &secs);
  host_sleep_ms((unsigned int)secs * 1000u);
  memset(res, 0, sizeof *res);
  res->outcome = r->mode == HOST_RACE_PRACTICE ? HOST_RACE_LEFT : HOST_RACE_FINISHED;
  res->place = place;
  res->laps = r->laps;
  res->total = total;
  res->best = best;
  res->best_lap = r->laps - 1;
  for (i = 0; i < r->laps && i < 12; i++) {
    res->lap[i] = i == r->laps - 1 ? best : (total - best) / (r->laps - 1);
  }
  osSyncPrintf("handoff: mode %d track %d mirrored %d weather %d laps %d car %d auto %d tires %d susp %d\n",
               r->mode, r->track, r->mirrored, r->weather, r->laps, r->car, r->automatic, r->tires, r->suspension);
  return 1;
}

/* the host: the engine that runs the races (NULL: this one does) */
void tgr_race_engine(host_race_fn fn) { s_engine = fn; }

/* BrRaceTick's first call: 1 when the race was run by the other engine and
 * the game has moved on to what follows it */
int BrRaceHandoff(void)
{
  BrCar *car = &D_8031B760[0];
  BrSeason *season = TGR_PTR(BrSeason *, car->season);
  HostRace r;
  HostRaceResult res;
  int i, n;

  if (s_engine == 0 && getenv("TGR_HANDOFF_FAKE")) {
    s_engine = fake_engine;
  }
  if (s_engine == 0 || !tgr_flag(TGR_FLAG_BR_RACES) || D_802707C8 != 0 || D_80270788 != 0) {
    return 0;
  }
  if (D_8026FF18 > 5 || (D_8026FF18 <= 3 && (D_8026FF08 != 1 || D_8028B940 < 0 || D_8028B940 >= 10))) {
    return 0;
  }
  memset(&r, 0, sizeof r);
  r.mode = D_8026FF18;                 /* HOST_RACE_*: the same numbers */
  r.piece = D_8026FF1C;
  r.track = D_8028B940 % 5;
  r.mirrored = D_8028B940 >= 5;
  r.weather = D_8028C800;
  if (D_8026FF18 == 2) {
    r.weather = r.track == 3 ? 0 : 1;   /* Time Attack's, as the race set-up picks it */
  }
  r.laps = D_8026FF18 == 1 ? D_8028B304 : 3;
  r.car = car->kind;
  r.handling = 1;                      /* Boss Rally's own (it has no handling choice) */
  r.automatic = season->xd8;
  r.tires = season->xdc;
  r.suspension = season->xe0;
  if (D_8026FF18 != 4) {
    BrMusicLoadTrack();                /* (as the race's set-up: not for the demos) */
  }
  D_8028B760 = D_8028B768 = 1.0f;
  D_802A49C8 = 255;
  s_job.race = r;
  osCreateMesgQueue(&s_done, s_done_buf, 1);
  host_thread_start(race_thread, 0);
  osRecvMesg(&s_done, 0, OS_MESG_BLOCK);
  if (!s_job.ok) {
    return 0;
  }
  res = s_job.res;

  if (D_8026FF18 >= 4) {
    /* what follows a demo or the ceremony (racetick.c, at the race's end) */
    BrScreenFlush2Layout1();
    D_8028A8AC = 0;
    D_80270788 = 0;
    if (res.outcome == HOST_RACE_LEFT && D_8026FF18 == 4) {
      D_80315E70 = 1;
    }
    if ((D_8026FF18 == 5 && season->round != 0) || (D_8026FF18 == 4 && D_8026FF1C == 2)) {
      D_8026FF18 = 0;                  /* the next season: its save prompt, then track select */
      BrSeasonPickRace();
      D_802724F4 = 1;
      BrModeSet(BrLoadSaveScreen);
    } else if (D_8026FF18 == 5) {
      BrDemoRaceStartC();              /* the last season won: the ending */
    } else if (D_8026FF1C == 1) {
      BrModeSet(BrOptionsScreen);
    } else if (D_8026FF1C == 0 && D_80315E70 == 0) {
      BrModeSet(BrIntroScreen);
    } else {
      BrModeSet(BrMainMenu);
    }
    return 1;
  }

  D_8028B304 = r.laps;
  D_8028A8AC = 0;
  D_8027078C = 0;
  D_802707A8 = 0;
  D_80270790 = 0;
  if (res.outcome == HOST_RACE_FINISHED && D_8026FF18 <= 2) {
    /* the car record as the finish line leaves it, and the records */
    n = res.laps < 5 ? res.laps : 5;
    for (i = 0; i < 5; i++) {
      car->lapTimes[i] = i < n ? res.lap[i] : 0.0f;
    }
    car->laps = res.laps;
    car->lapTime = res.total;
    car->xf98 = res.best;
    car->xf9c = res.best_lap;
    car->xfac = res.place;
    if (res.best > 0.0f && (season->x8c[D_8028B940] == 0.0f || res.best < season->x8c[D_8028B940])) {
      season->x8c[D_8028B940] = res.best;
    }
    if (D_8028B304 == 3 && (season->xe8[D_8028B940] == 0.0f || res.total < season->xe8[D_8028B940])) {
      season->xe8[D_8028B940] = res.total;
    }
    BrRaceResultSave();
    BrScreenFlush2Layout1();
    BrRaceResultRestore();
    BrModeSet(BrResultsRun);
  } else {
    BrModeSet(BrMainMenu);
  }
  return 1;
}

/* the player's speed, km/h (negative backing up): what the speedometer reads
 * (BrHudDraw).  A touch host's brake pedal turns to reverse below a walk, as
 * Boss Rally's reverse does by itself (TGR reverses only from first gear,
 * with the stick pulled back and A) */
float tgr_race_speed(void)
{
  return D_8031B760[0].xfe4[0];
}

/* the races go to the other engine (the 1P/2P screen offers one player) */
int tgr_race_handoff_on(void)
{
  return s_engine != 0 && tgr_flag(TGR_FLAG_BR_RACES);
}
