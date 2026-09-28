/* cartick.c -- a car's per-frame clocks and message timers
 */
#include "tgr/common.h"
#include "tgr/car.h"

/* -- declarations -- */
extern int D_8026FF18;
extern float D_8028AAD8;
extern void *D_80025C70;                /* the track grid (0 before a track loads) */
int BrFloatToInt(float f);
void func_8021EB50(BrCar *car);
/* -- end declarations -- */

/* WHAT IT DOES: Advance a car's clocks by one frame while it is still in
 * the race: the race clock always, the lap clock except in mode 3, and in
 * mode 1 run a countdown down to zero.  The PC twin is BrCarTickClocks. */
/* @implements 0x8021F0A8 tgr BrCarTickClocks */
void BrCarTickClocks(BrCar *car)
{
  if ((car->link->flags & 3) == 0) {
    if (D_8026FF18 == 3) {
      car->raceTime += D_8028AAD8;
      return;
    }
    car->lapTime += D_8028AAD8;
    car->raceTime += D_8028AAD8;
    if (D_8026FF18 == 1) {
      car->xfa4 = car->xfa4 - D_8028AAD8;
      if (car->xfa4 < 0.0f) {
        car->xfa4 = 0.0f;
      }
    }
  }
}

/* WHAT IT DOES: Count down a car's on-screen message timers by one frame:
 * the first message while it runs, else the second; a timer that runs out
 * clears its message.  The PC twin is BrCarTickMessages.  The running
 * tests compare with an integer 0, the rest with 0.0f: two constants, so
 * IDO materialises the zero again inside each branch as the ROM does. */
/* @implements 0x8021F158 tgr BrCarTickMessages */
void BrCarTickMessages(BrCar *car)
{
  if (car->msgATime != 0) {
    car->msgATime = car->msgATime - D_8028AAD8;
    if (car->msgATime <= 0.0f) {
      car->msgATime = 0.0f;
      car->msgA = 0;
    }
  } else if (car->msgBTime != 0) {
    car->msgBTime = car->msgBTime - D_8028AAD8;
    if (car->msgBTime <= 0.0f) {
      car->msgBTime = 0.0f;
      car->msgB = 0;
    }
  }
}

/* WHAT IT DOES: Put the car in its 32-unit cell of the 64 by 64 track grid
 * (held to the grid's edges) and update the grid's record of it. */
/* @implements 0x8021F2D8 tgr BrCarGridCell */
void BrCarGridCell(BrCar *car)
{
  if (D_80025C70 != 0) {
    car->cellX = BrFloatToInt(car->mtx0[3][0] / 32);
    car->cellY = BrFloatToInt(car->mtx0[3][1] / 32);
    if (car->cellX < 0) {
      car->cellX = 0;
    } else if (car->cellX >= 0x40) {
      car->cellX = 0x3f;
    }
    if (car->cellY < 0) {
      car->cellY = 0;
    } else if (car->cellY >= 0x40) {
      car->cellY = 0x3f;
    }
    func_8021EB50(car);
  }
}
