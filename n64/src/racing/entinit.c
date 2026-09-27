/* entinit.c -- resetting car entities
 */
#include "tgr/common.h"
#include "tgr/car.h"

/* -- declarations -- */
void BrCarReset(BrCar *car);
extern int D_8028AE04;
void BrCarPickKind(BrCar *car);
extern int D_8026FF08;
extern int D_8026FF18;
void BrPadInit(unsigned int *param_1);
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
    BrCarReset(&D_8031B760[i]);
    BrPadInit(D_8036A8E0[i]);
  }
}

/* WHAT IT DOES: Point a car record (0x2090 bytes) at the pad record of the
 * same slot and reset the car's matrix at +0x1D88 to identity. */
/* @implements 0x802207A4 tgr BrCarBindPad */
void BrCarBindPad(BrCar *car)
{
  car->pad = D_8036A8E0[car - D_8031B760];
  guMtxIdent(car->mtx);
}

/* WHAT IT DOES: Choose a car's kind (+0x205C) for the coming race from the
 * round table (the round is player 1's season round). Player slots take the
 * round's first kind in a championship and are left alone otherwise; in
 * arcade mode the other cars copy player 1; in a championship the first
 * computer car takes the round's first kind and the rest the lowest kind
 * allowed by the round's mask (rounds past 3 use round 3's mask; no bit set
 * means kind 5). */
/* @implements 0x80225F88 tgr BrCarPickKind */
void BrCarPickKind(BrCar *car)
{
  int i;
  int r;
  short mask;

  r = D_8031B760[0].season->round;
  if (r > 3) {
    r = 3;
  }
  if (car->slot >= D_8026FF08) {
    if (D_8026FF18 == 1) {
      car->kind = D_8031B760[0].kind;
      return;
    }
    mask = D_8028B944[r].kindMask;
    for (i = 0; i < 16; i++) {
      if (mask & 1) {
        break;
      }
      mask >>= 1;
    }
    if (i == 16) {
      i = 5;
    }
    if (car->slot > D_8026FF08) {
      car->kind = i;
    } else {
      car->kind = D_8028B944[D_8031B760[0].season->round].kinds[car->slot - D_8026FF08];
    }
  } else {
    if (D_8026FF18 == 0) {
      car->kind = D_8028B944[D_8031B760[0].season->round].kinds[0];
    }
  }
}

/* WHAT IT DOES: Reset one car record: bind it to its pad, store its slot
 * index, and for the two player slots start a fresh season record (car 1 and
 * car 8 unlocked, per-car values cleared); other slots get no season. */
/* @implements 0x80226100 tgr BrCarReset */
void BrCarReset(BrCar *car)
{
  int i;

  BrCarBindPad(car);
  car->slot = car - D_8031B760;
  if (car->slot < 2) {
    car->season = &D_80324300[car->slot];
    car->season->state = 0;
    car->season->round = 0;
    car->season->points[0] = 0;
    car->season->race = 0;
    car->season->unlocked = 0x102;
    car->season->xce = 4;
    car->season->xd0 = 1;
    car->season->xd8 = 1;
    car->season->xd4 = 0;
    car->season->xe0 = 1;
    car->season->xdc = 2;
    car->season->xe4 = 0;
    for (i = 0; i < D_8028AE04; i++) {
      car->season->x8c[i] = 0.0f;
      car->season->xe8[i] = 0.0f;
    }
  } else {
    car->season = 0;
  }
  BrCarPickKind(car);
  car->xe58 = 0;
}

/* WHAT IT DOES: Put a car at (x, y, z): the four position vectors it keeps
 * are all set to the same point. */
/* @implements 0x802201C8 tgr BrCarSetPos */
void BrCarSetPos(BrCar *car, float x, float y, float z)
{
  BrVec3 *v;

  car->pos1cc.x = x;
  car->pos1cc.y = y;
  car->pos1cc.z = z;
  car->pos2ac.x = x;
  car->pos2ac.y = y;
  car->pos2ac.z = z;
  car->pos268.x = x;
  car->pos268.y = y;
  car->pos268.z = z;
  v = &car->posfd8;
  v->x = x;
  v->y = y;
  v->z = z;
}
