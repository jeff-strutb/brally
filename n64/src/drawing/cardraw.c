/* cardraw.c -- drawing the cars
 */
#include "tgr/common.h"
#include "tgr/car.h"

/* -- declarations -- */
void BrMat4TransformPoint4(float out[4], float v[3], float m[4][4]);
void *memcpy(void *dst, void *src, unsigned int n);
extern BrCarModel *D_8028AB08;
float BrVec3Dist(BrVec3 *a, BrVec3 *b);
float BrFogAmount(float v[3]);
int BrGridSpanHasPoint(float x, float z);
void BrVec3MulAdd(BrVec3 *pOut, BrVec3 *pA, BrVec3 *pB, float s);
extern BrCar *D_8028AAF0;             /* the player's car */
extern BrCar *D_8028AAF4;             /* the car the camera follows */
extern int D_8028AA70;
extern int D_8028AA80;
extern int D_8028AA8C;
extern int D_80351C70[];              /* per slot: draw the car body */
extern int D_80351C80[];              /* per slot: draw the car at all */
/* -- end declarations -- */

/* WHAT IT DOES: Place car n's four wheels: each wheel's matrix takes the
 * body's rotation and, as its translation, the model's wheel position
 * carried through the body matrix.  Makes the car's model the current one;
 * a model without wheels is left alone. */
/* @implements 0x8022FEF0 tgr BrCarPlaceWheels */
void BrCarPlaceWheels(int n)
{
  BrCar *car = &D_8031B760[n];
  int i;

  D_8028AB08 = (BrCarModel *)car->model;
  if (D_8028AB08->dl2[0][0] != 0) {
    for (i = 0; i < 4; i++) {
      memcpy(car->wheelMtx[i], car->mtx0, 0x30);
      BrMat4TransformPoint4(car->wheelMtx[i][3], D_8028AB08->wheel[i], car->mtx0);
    }
  }
}

/* WHAT IT DOES: Decide whether a car is drawn this frame and set its fog
 * amount: other cars only when their position (or, with either of two
 * flags set, a point 6 units ahead) lies on the track grid; the player's
 * car skips its body when seen from its in-car views; a car with colour
 * byte 3 == 2 never draws its body. */
/* @implements 0x802303D4 tgr BrCarVisibility */
void BrCarVisibility(BrCar *car)
{
  float dist;
  float v[3];

  dist = BrVec3Dist((BrVec3 *)car->mtx0[3], (BrVec3 *)D_8028AAF4->mtx0[3]);
  D_80351C70[car->slot] = 0;
  D_80351C80[car->slot] = 0;
  car->fog = BrFogAmount(car->mtx0[3]);
  if (car != D_8028AAF0) {
    if (D_8028AA80 != 0 || D_8028AA8C != 0) {
      BrVec3MulAdd((BrVec3 *)v, (BrVec3 *)car->mtx0[3], (BrVec3 *)car->mtx0[0], 6.0f);
      if (!BrGridSpanHasPoint(v[0], v[1]) && !BrGridSpanHasPoint(car->mtx0[3][0], car->mtx0[3][1])) {
        return;
      }
    } else if (!BrGridSpanHasPoint(car->mtx0[3][0], car->mtx0[3][1])) {
      return;
    }
  }
  if (car == D_8028AAF0 && (car->cam == &car->cams[0] || car->cam == &car->cam4) && D_8028AA70 == 0) {
    D_80351C80[car->slot] = 1;
    return;
  }
  if (car->colour[3] != 2) {
    D_80351C70[car->slot] = 1;
  }
  D_80351C80[car->slot] = 1;
}
