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
extern int D_8026FF10;
typedef struct BrCarEnt {       /* a car's entity record (0x78 bytes) */
  char pad00[0x60];
  BrCar *car;                   /* 0x60 */
} BrCarEnt;
void BrHudArrowDraw(BrCar *car, BrVec3 *at, short kind);
void BrWrongWayCheck(BrCar *car);
void func_8022A0E0(BrCarEnt *e);
void BrVec3Sub(BrVec3 *out, BrVec3 *a, BrVec3 *b);
void BrVec3ScaleBy(BrVec3 *v, float s);
float BrAtan2(float x, float y);
typedef struct BrCarWheel {     /* a wheel's rigid body */
  char pad000[0x1C0];
  float x1c0;                   /* 0x1C0 */
  char pad1c4[0x1CC - 0x1C4];
  float x1cc;                   /* 0x1CC */
  float x1d0;                   /* 0x1D0 */
} BrCarWheel;
typedef struct BrViewRect { int x; int y; int w; int h; int car; } BrViewRect;
extern BrViewRect D_8031B2C8[2];        /* the players' views */
extern int D_8028AB0C;                  /* number of views */
int func_8021F380();
void func_80222050(BrCar *car);
void func_80221940(BrCar *car);
void func_80221170(BrCar *car);
void BrVec3Scale(BrVec3 *out, BrVec3 *v, float s);
void BrVec3AddTo(BrVec3 *a, BrVec3 *b);
void BrVec3Cross(BrVec3 *out, BrVec3 *a, BrVec3 *b);
void BrCarSetPos(BrCar *car, float x, float y, float z);
void BrCarSetVel(BrCar *car, float x, float y, float z);
void BrCarSetAngVel(BrCar *car, float x, float y, float z);
void BrCarRotate(BrCar *car, float x, float y, float z);
void BrPadConsume(void *pad, int button);
void BrCarBuildMatrices(BrCar *car);
float sqrtf(float x);
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

/* WHAT IT DOES: A car entity's per-frame housekeeping (not in replay): show
 * a pending HUD arrow, run the clocks, messages, wrong-way check, 0x8022A0E0
 * and grid cell, take the velocity from this frame's movement, the heading
 * from the camera's first row, and count 0xED4 down. */
/* @implements 0x8022CF88 tgr BrCarEntTick */
void BrCarEntTick(BrCarEnt *e)
{
  if (D_8026FF10 == 0 && e->car != 0) {
    if (e->car->x344 != 0) {
      BrHudArrowDraw(e->car, &e->car->x334, e->car->x344);
      e->car->x344 = 0;
    }
    BrCarTickClocks(e->car);
    BrCarTickMessages(e->car);
    BrWrongWayCheck(e->car);
    func_8022A0E0(e);
    BrCarGridCell(e->car);
    BrVec3Sub(&e->car->velfd8, (BrVec3 *)e->car->mtx0[3], &e->car->posPrev);
    BrVec3ScaleBy(&e->car->velfd8, 1.0f / D_8028AAD8);
    e->car->heading = BrAtan2(e->car->cam->mtx[0][0], e->car->cam->mtx[0][1]);
    if (e->car->xed4 != 0) {
      e->car->xed4--;
    }
  }
}

/* WHAT IT DOES: One physics frame for a car.  Free-flying (the debug fly
 * mode), it moves along its heading at its set speed, stops, and turns at
 * its set rates; pad bit 0x10 levels it.  Otherwise the track contact
 * update runs, the wheels' slip terms are reset and set from the throttle
 * and brake demands (doubled on the front pair for the car kinds that drive
 * it, the rear pair braked only while it is nearly still), and the body is
 * stepped.  Then the draw matrices, the speed for the gauges, and the view's
 * camera step for a car that has a view.  u0/u1 are declared and unused
 * (the ROM frame keeps their slots).
 * RESIDUE (48): FP register colouring only -- the ROM gives the float zero
 * f0 and the frame time f2 throughout; ours swaps them. */
/* @implements 0x8021F998 tgr BrCarPhysTick */
void BrCarPhysTick(BrCar *car)
{
  int u0[4];
  BrVec3 v;
  int u1[4];
  int i;

  if (car->xf4c != 0) {
    BrVec3Scale(&v, (BrVec3 *)car->mtx0[0], car->x1ddc * D_8028AAD8 * 100.0f);
    BrVec3AddTo((BrVec3 *)car->mtx0[3], &v);
    BrCarSetPos(car, car->mtx0[3][0], car->mtx0[3][1], car->mtx0[3][2]);
    BrCarSetVel(car, 0, 0, 0);
    BrCarSetAngVel(car, 0, 0, 0);
    car->xdf4 = 0.0f;
    BrCarRotate(car, car->x1dd4 * 3.1415927f * 0.4f * D_8028AAD8,
                car->x1dd8 * 3.1415927f * 0.4f * D_8028AAD8,
                car->x1de0 * 3.1415927f * 0.4f * D_8028AAD8);
    if (*car->pad & 0x10) {
      car->mtx0[2][0] = 0.0f;
      car->mtx0[2][1] = 0.0f;
      car->mtx0[2][2] = 1.0f;
      BrVec3Cross((BrVec3 *)car->mtx0[1], (BrVec3 *)car->mtx0[2], (BrVec3 *)car->mtx0[0]);
      BrVec3Cross((BrVec3 *)car->mtx0[2], (BrVec3 *)car->mtx0[0], (BrVec3 *)car->mtx0[1]);
      BrPadConsume(car->pad, 0x10);
    }
  } else {
    car->xfd4 = func_8021F380(car->mtx0[3], car->mtx0[2], car->mtx0[3], car->x1fc0, &car->x2000,
                              car->x2004, &car->x2044, &car->x2048, &car->x204c);
    car->wheel[0]->x1c0 = car->wheel[1]->x1c0 = 0.0f;
    func_80222050(car);
    car->wheel[2]->x1c0 = car->wheel[3]->x1c0 = car->xdf0;
    car->wheel[0]->x1cc = 0.0f;
    car->wheel[1]->x1cc = 0.0f;
    car->wheel[2]->x1cc = 0.0f;
    car->wheel[3]->x1cc = 0.0f;
    car->wheel[0]->x1d0 = 0.0f;
    car->wheel[1]->x1d0 = 0.0f;
    car->wheel[2]->x1d0 = 0.0f;
    car->wheel[3]->x1d0 = 0.0f;
    if (car->xe38 > 0.0f) {
      if ((*car->pad & 0x20000) && car->xe40 == 1) {
        car->xe38 = -car->xe38;
      }
      if (car->xe28[1] != 0) {
        car->wheel[0]->x1cc = -car->xe38 * 2;
        car->wheel[1]->x1cc = -car->xe38 * 2;
        car->wheel[2]->x1cc = -car->xe38 * 0;
        car->wheel[3]->x1cc = -car->xe38 * 0;
      } else {
        car->wheel[0]->x1cc = -car->xe38;
        car->wheel[1]->x1cc = -car->xe38;
        car->wheel[2]->x1cc = -car->xe38;
        car->wheel[3]->x1cc = -car->xe38;
      }
    }
    if (car->xe3c < 0.0f) {
      car->wheel[0]->x1d0 = -car->xe3c;
      car->wheel[1]->x1d0 = -car->xe3c;
      car->wheel[0]->x1cc = 0.0f;
      car->wheel[1]->x1cc = 0.0f;
      if ((car->wheel[2]->x1cc < 0.0f ? -car->wheel[2]->x1cc : car->wheel[2]->x1cc) < 0.0001f) {
        car->wheel[2]->x1d0 = -car->xe3c;
        car->wheel[3]->x1d0 = -car->xe3c;
      }
    }
    func_80221940(car);
  }
  BrCarBuildMatrices(car);
  if (car->wheels[1].x13c != 0) {
    car->xfe4[0] = sqrtf(car->st.vel.z * car->st.vel.z + (car->st.vel.x * car->st.vel.x + car->st.vel.y * car->st.vel.y)) * 2.24f;
  }
  for (i = 0; i < D_8028AB0C; i++) {
    if (car->slot == D_8031B2C8[i].car) {
      func_80221170(car);
      return;
    }
  }
}
