/* cardraw.c -- drawing the cars
 */
#include "tgr/common.h"
#include "tgr/car.h"
#include "tgr/gbi.h"

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
extern Gfx *D_8028A858;
extern int D_8028C300;                  /* the fog render-mode bits */
extern int D_80351D00;                  /* the ghost car's alpha */
extern int D_8028AA84;                  /* use the low-detail wheels */
extern float D_8031AB50[4][4];
extern float D_80351CC0[4][4];
extern float D_8031AA50[4][4];
extern float D_8031AB10[4][4];
void guScaleF(float mf[4][4], float x, float y, float z);
void guMtxCatF(float m[4][4], float n[4][4], float r[4][4]);
void guMtxF2L(float mf[4][4], void *m);
void *BrMtxAlloc(void);
void BrMat4FitRange(float m[4][4]);
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

/* WHAT IT DOES: Draw the current model's four wheels on a car: for each,
 * set the ghost (translucent, alpha from D_80351D00) or normal combine and
 * fog render mode, scale the wheel's matrix down from the model's 1/255
 * units, load it, load its product with the camera as the lighting matrix,
 * set texturing and tiles, and call the wheel display list (the low-detail
 * one when asked).
 * RESIDUE (20): saved-register naming only -- the ROM gives s3-s5 to the
 * two matrix buffers and the DL command word and s6/s7 to the loop; ours
 * the reverse.  Register-blind exact; loop forms and named locals do not
 * move it. */
/* @implements 0x8022FFB4 tgr BrCarDrawWheels */
void BrCarDrawWheels(BrCar *car)
{
  int i;
  unsigned int m;

  if (D_8028AB08->dl2[0][0] != 0) {
    for (i = 0; i < 4; i++) {
      gDPPipeSync(D_8028A858++);
      gRaw(D_8028A858++, 0xba001402, 0x100000);
      if (car->colour[3] == 2) {
        gRaw(D_8028A858++, 0xb900031d, D_8028C300 | 0x104a50);
        gRaw(D_8028A858++, 0xfb000000, D_80351D00 & 0xff);
        gRaw(D_8028A858++, 0xfc127fff, 0xff17f23f);
      } else {
        gRaw(D_8028A858++, 0xfc127fff, 0xfffff238);
        gRaw(D_8028A858++, 0xb900031d, D_8028C300 | 0x112230);
      }
      guScaleF(D_8031AB50, 0.003921569f, 0.003921569f, 0.003921569f);
      guMtxCatF(D_8031AB50, car->wheelMtx[i], D_80351CC0);
      m = (unsigned int)BrMtxAlloc();
      guMtxF2L(D_80351CC0, (void *)m);
      gRaw(D_8028A858++, 0x1060040, m);
      guMtxCatF(D_80351CC0, D_8031AA50, D_8031AB10);
      BrMat4FitRange(D_8031AB10);
      m = (unsigned int)BrMtxAlloc();
      guMtxF2L(D_8031AB10, (void *)m);
      gRaw(D_8028A858++, 0x39e0010, m + 0x80000000);
      gRaw(D_8028A858++, 0x3980010, m + 0x80000010);
      gRaw(D_8028A858++, 0x39a0010, m + 0x80000020);
      gRaw(D_8028A858++, 0x39c0010, m + 0x80000030);
      gRaw(D_8028A858++, 0xbb000001, 0xffffffff);
      gRaw(D_8028A858++, 0xb6000000, 0xc0000);
      gRaw(D_8028A858++, 0xe8000000, 0);
      gRaw(D_8028A858++, 0xf5100000, 0x7000000);
      gRaw(D_8028A858++, 0xf50001f0, 0x6000000);
      gRaw(D_8028A858++, 0xf5000100, 0x5000000);
      if (D_8028AA84 != 0) {
        if (D_8028AB08->dl2[0][2] != 0) {
          gRaw(D_8028A858++, 0x6000000, D_8028AB08->dl2[0][2]);
        }
      } else if (D_8028AB08->dl2[0][0] != 0) {
        gRaw(D_8028A858++, 0x6000000, D_8028AB08->dl2[0][0]);
      }
      gRaw(D_8028A858++, 0xbd000000, 0);
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
