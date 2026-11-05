/* rmprogress.c (port): this library's side of the app's one save
 * (ports/remastered/platform/progress.h).  Each frame what the two seasons
 * have opened joins it and they take the rest; and the seasons themselves go
 * across as they change, so the championship is the one in the other Top Gear
 * Rally's menus and Boss Rally's. */
#include "tgr/common.h"
#include "tgr/car.h"
#include "tgr/season.h"

/* -- declarations -- */
void rm_progress_tgr(unsigned short *cars, unsigned short *tracks, unsigned short *weather);
int  rm_progress_gen(void);
int  rm_progress_season_ver(void);
int  rm_progress_season_put_tgr(const void *seasons);
void rm_progress_season_get_tgr(void *seasons);
void BrCarReset(BrCar *car);
/* -- end declarations -- */

void rm_progress_tick(void)
{
  static BrSeason last[2];
  static int started, gen, ver;
  BrSeason *s = D_80324300;
  int i, g;

  if (D_8031B760[0].season == 0 || D_8031B760[1].season == 0) {
    return;                             /* (the boot: no seasons yet) */
  }
  g = rm_progress_gen();
  if (started && g != gen) {            /* erased (here, or by the other library) */
    BrCarReset(&D_8031B760[0]);
    BrCarReset(&D_8031B760[1]);
    memcpy(last, s, sizeof last);
    ver = rm_progress_season_ver();
  }
  gen = g;
  if (started && memcmp(s, last, sizeof last) != 0) {
    ver = rm_progress_season_put_tgr(s);
  } else if (rm_progress_season_ver() != ver) {
    ver = rm_progress_season_ver();
    rm_progress_season_get_tgr(s);
    tgr_print("one save: season %d round %d race %d, %d points\n", s[0].state, s[0].round, s[0].race, s[0].points[s[0].round]);   /* (TGR_LOG) */
  }
  started = 1;
  for (i = 0; i < 2; i++) {
    rm_progress_tgr(&s[i].unlocked, &s[i].xce, &s[i].xd0);
  }
  memcpy(last, s, sizeof last);
}
