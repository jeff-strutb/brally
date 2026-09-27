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
void guRotateF(float mf[4][4], float a, float x, float y, float z);
float sinf(float x);
float cosf(float x);
extern int D_8028C360;
extern Gfx D_8028C368[];
extern Vtx D_80351A20[31];
extern Mtx D_803519E0;
/* -- end declarations -- */

/* WHAT IT DOES: Begin the front end's 3-D backdrop: the first time
 * through, build its fan of 31 vertices (a centre and a 256-unit circle in
 * 12-degree steps, white) and its matrix (scaled down, turned -45 degrees);
 * then load the projection and call the backdrop's display list. */
/* @implements 0x80233494 tgr BrMenuBackdropBegin */
void BrMenuBackdropBegin(int unused)
{
  int i;
  float a;

  if (D_8028C360 == 0) {
    for (i = 0; i < 31; i++) {
      a = i * 0.20943952f;
      D_80351A20[i].v.ob[0] = cosf(a) * 256.0f;
      D_80351A20[i].v.ob[1] = sinf(a) * 256.0f;
      D_80351A20[i].v.ob[2] = 0;
      D_80351A20[i].v.cn[0] = 0xff;
      D_80351A20[i].v.cn[1] = 0xff;
      D_80351A20[i].v.cn[2] = 0xff;
      D_80351A20[i].v.cn[3] = 0xff;
    }
    D_80351A20[0].v.ob[0] = 0;
    D_80351A20[0].v.ob[1] = 0;
    D_80351A20[0].v.ob[2] = 0;
    guScaleF(D_8031AB10, 0.009765625f, 0.009765625f, 0.009765625f);
    guRotateF(D_8031AB50, -45.0f, 0.0f, 0.0f, 1.0f);
    guMtxCatF(D_8031AB10, D_8031AB50, D_8031AB50);
    guMtxF2L(D_8031AB50, &D_803519E0);
  }
  gSPMatrix(D_8028A858++, D_8028A878, G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
  gSPPerspNormalize(D_8028A858++, &D_8028A874);
  gSPDisplayList(D_8028A858++, D_8028C368);
}

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
