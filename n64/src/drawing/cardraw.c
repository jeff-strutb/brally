/* cardraw.c -- drawing the cars
 */
#include "tgr/common.h"
#include "tgr/car.h"

/* -- declarations -- */
void BrMat4TransformPoint4(float out[4], float v[3], float m[4][4]);
void *memcpy(void *dst, void *src, unsigned int n);
extern BrCarModel *D_8028AB08;
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
