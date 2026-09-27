/* carview.c -- drawing a car model on a front-end screen (car select, paint
 * shop): the 3-D projection, and the car scaled to its artwork's size
 */
#include "tgr/common.h"
#include "tgr/gbi.h"
#include "tgr/car.h"

/* -- declarations -- */
typedef struct { char raw[0xdf88]; } BrCarModelBuf;
extern BrCarModelBuf D_803C8000[];
extern int D_8031B238[];
extern Gfx *D_8028A858;
extern Mtx *D_8028A878;
extern unsigned short D_8028A874;
extern Gfx D_8028C4F0[];
extern Gfx D_8028C5D8[];
extern float D_8031AB10[4][4];
extern float D_8031AB50[4][4];
extern float D_802A9EF0;
Mtx *BrMtxAlloc(void);
void guScaleF(float mf[4][4], float x, float y, float z);
void guMtxCatF(float m[4][4], float n[4][4], float r[4][4]);
void guMtxF2L(float mf[4][4], Mtx *m);
void *memcpy(void *d, void *s, int n);
/* -- end declarations -- */

/* WHAT IT DOES: Begin drawing 3-D on a menu screen: load the projection
 * matrix, set the perspective normalisation (the original passes the
 * address of the value, not the value) and run the setup display list. */
/* @implements 0x80233668 tgr BrMenu3DBegin */
void BrMenu3DBegin(int unused)
{
  gSPMatrix(D_8028A858++, D_8028A878, G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
  gSPPerspNormalize(D_8028A858++, &D_8028A874);
  gSPDisplayList(D_8028A858++, D_8028C4F0);
}

/* WHAT IT DOES: Draw a car on a menu screen: projection as above, then the
 * car's own matrix scaled by its artwork's length and width (1/256 units)
 * and pulled back, pushed as the model view, and its display list. */
/* @implements 0x802336E0 tgr BrMenuCarDraw */
void BrMenuCarDraw(BrCar *car)
{
  Mtx *m;
  int k;

  gSPMatrix(D_8028A858++, D_8028A878, G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
  gSPPerspNormalize(D_8028A858++, &D_8028A874);
  m = BrMtxAlloc();
  k = D_8031B238[(BrCarModelBuf *)car->model - D_803C8000];
  guScaleF(D_8031AB10, D_8028AE0C[k].len / (double)256, D_8028AE0C[k].wid * 2.0 / 256, 1.0f / 256.0f);
  memcpy(D_8031AB50, car, 0x40);
  D_8031AB10[3][2] -= D_802A9EF0;
  guMtxCatF(D_8031AB10, D_8031AB50, D_8031AB50);
  guMtxF2L(D_8031AB50, m);
  gSPMatrix(D_8028A858++, m, G_MTX_MODELVIEW | G_MTX_MUL | G_MTX_PUSH);
  gSPDisplayList(D_8028A858++, D_8028C5D8);
}
