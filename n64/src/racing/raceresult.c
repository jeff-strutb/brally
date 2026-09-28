/* raceresult.c -- keeping the players' race results for the results screen
 */
#include "tgr/common.h"
#include "tgr/car.h"

/* -- declarations -- */
/* The human players' results, copied out of their car records when a race
 * ends; separate arrays, one entry per player. */
extern int D_80315D60[2];               /* car xfac */
extern float D_80315D68[2];             /* the lap clock */
extern float D_80315D70[2];             /* car xf98 */
extern float D_80315D78[2][5];          /* each lap's time */
extern int D_80315DA0[2];               /* car xf9c */
extern int D_80315DA8[2];               /* laps completed */
extern int D_8026FF08;                  /* human players */
extern int D_8028B304;                  /* laps in the race */
extern int D_80270790;                  /* results are ready */
/* -- end declarations -- */

/* WHAT IT DOES: Copy each human player's race numbers (laps done, lap
 * times and a few more) out of their car into the results record, then
 * flag the results ready. */
/* @implements 0x80205FDC tgr BrRaceResultSave */
void BrRaceResultSave(void)
{
  int i;
  int j;

  for (i = 0; i < D_8026FF08; i++) {
    D_80315D60[i] = D_8031B760[i].xfac;
    D_80315D68[i] = D_8031B760[i].lapTime;
    D_80315D70[i] = D_8031B760[i].xf98;
    D_80315DA0[i] = D_8031B760[i].xf9c;
    D_80315DA8[i] = D_8031B760[i].laps;
    for (j = 0; j < D_8028B304; j++) {
      D_80315D78[i][j] = D_8031B760[i].lapTimes[j];
    }
  }
  D_80270790 = 1;
}
