/* entinit.c -- resetting car entities
 */
#include "tgr/common.h"
#include "tgr/season.h"

/* -- declarations -- */
void BrCarReset(int *car);
extern int D_8028AE04;
void func_80225F88(void);
void BrPadInit(unsigned int *param_1);
extern int D_8031B760[4][0x824];
extern unsigned int D_8036A8E0[4][0x57];
void guMtxIdent(int *m);
/* -- end declarations -- */

/* WHAT IT DOES: Reset every car slot for a new session: runs the per-car
 * reset on each car record (0x2090 bytes apiece) and clears its matching
 * input record. */
/* @implements 0x80200154 tgr BrEntAllReset */
void BrEntAllReset(void)
{
  int i;

  for (i = 0; i < 4; i++) {
    BrCarReset(D_8031B760[i]);
    BrPadInit(D_8036A8E0[i]);
  }
}

/* WHAT IT DOES: Point a car record (0x2090 bytes) at the pad record of the
 * same slot and reset the car's matrix at +0x1D88 to identity. */
/* @implements 0x802207A4 tgr BrCarBindPad */
void BrCarBindPad(int *car)
{
  *(unsigned int **)(car + 0x81d) = D_8036A8E0[(int (*)[0x824])car - D_8031B760];
  guMtxIdent(car + 0x762);
}

/* WHAT IT DOES: Reset one car record: bind it to its pad, store its slot
 * index, and for the two player slots start a fresh season record (car 1 and
 * car 8 unlocked, per-car values cleared); other slots get no season. */
/* @implements 0x80226100 tgr BrCarReset */
void BrCarReset(int *car)
{
  int i;

  BrCarBindPad(car);
  car[0x50] = (int (*)[0x824])car - D_8031B760;
  if (car[0x50] < 2) {
    *(BrSeason **)(car + 0x397) = &D_80324300[car[0x50]];
    (*(BrSeason **)(car + 0x397))->state = 0;
    (*(BrSeason **)(car + 0x397))->round = 0;
    (*(BrSeason **)(car + 0x397))->points[0] = 0;
    (*(BrSeason **)(car + 0x397))->race = 0;
    (*(BrSeason **)(car + 0x397))->unlocked = 0x102;
    (*(BrSeason **)(car + 0x397))->xce = 4;
    (*(BrSeason **)(car + 0x397))->xd0 = 1;
    (*(BrSeason **)(car + 0x397))->xd8 = 1;
    (*(BrSeason **)(car + 0x397))->xd4 = 0;
    (*(BrSeason **)(car + 0x397))->xe0 = 1;
    (*(BrSeason **)(car + 0x397))->xdc = 2;
    (*(BrSeason **)(car + 0x397))->xe4 = 0;
    for (i = 0; i < D_8028AE04; i++) {
      (*(BrSeason **)(car + 0x397))->x8c[i] = 0.0f;
      (*(BrSeason **)(car + 0x397))->xe8[i] = 0.0f;
    }
  } else {
    *(BrSeason **)(car + 0x397) = 0;
  }
  func_80225F88();
  car[0x396] = 0;
}
