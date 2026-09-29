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

/* -- declarations: BrSkyDraw -- */
extern int D_8028AA5C;                  /* no sky this frame */
extern int D_8028AA78;                  /* the weather has fog */
extern int D_8028C818;                  /* the lightning timer */
extern int D_8028AAEC;                  /* the view being drawn */
extern unsigned char D_8028AB20, D_8028AB24, D_8028AB28, D_8028AB2C;   /* the fog colour */
extern unsigned int D_8028A898;         /* the texture filter */
extern int D_8028A8A8;
extern int D_8028A8AC;
extern Mtx *D_8028A878;                 /* the projection matrix */
extern Gfx *D_80025C50;                 /* the track's sky display list */
typedef struct BrSkyView { int x; int y; int w; int h; int car; } BrSkyView;
extern BrSkyView D_8031B2C8[2];
int BrRaceSplitScreen(void);
void guTranslateF(float mf[4][4], float x, float y, float z);
void BrGfxFillRect(int x, int y, int w, int h, int r, int g, int b);
/* -- end declarations -- */

/* WHAT IT DOES: Draw the sky: set the clip ratio, then, unless the frame
 * has no sky, either the track's sky display list around the camera
 * (lowered a little, fogged in fog), or in a split screen a fill of the
 * view in the fog colour -- brightened by a lightning flash.  The unused
 * array is the ROM frame's 0x40.
 * RESIDUE (116): the two cull-mode ternaries -- the ROM materialises the 0x1000
 * arm first and fills the branch's delay slot from the 0x2000 arm (4 bytes
 * longer); everything after is shifted by that. */
/* @implements 0x8022F968 tgr BrSkyDraw */
void BrSkyDraw(void)
{
  int u[4];
  Mtx *m;

  gSPClipRatio1(D_8028A858++);
  if (D_8028AA5C != 0) {
    return;
  }
  if (BrRaceSplitScreen()) {
    if (D_8028C818 > 0 && (D_8028C818 & 1)) {
      BrGfxFillRect(D_8031B2C8[D_8028AAEC].x, D_8031B2C8[D_8028AAEC].y, D_8031B2C8[D_8028AAEC].w,
                    D_8031B2C8[D_8028AAEC].h, (D_8028AB20 * 3 + 240) >> 2, (D_8028AB24 * 3 + 248) >> 2,
                    (D_8028AB28 * 3 + 255) >> 2);
    } else {
      BrGfxFillRect(D_8031B2C8[D_8028AAEC].x, D_8031B2C8[D_8028AAEC].y, D_8031B2C8[D_8028AAEC].w,
                    D_8031B2C8[D_8028AAEC].h, D_8028AB20, D_8028AB24, D_8028AB28);
    }
    return;
  }
  guTranslateF(D_8031AB10, D_8028AAF4->mtx0[3][0], D_8028AAF4->mtx0[3][1], D_8028AAF4->mtx0[3][2] * 0.99f);
  m = BrMtxAlloc();
  guMtxF2L(D_8031AB10, m);
  gSPMatrix(D_8028A858++, (char *)D_8028A878 + 0x80000000, G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
  gSPMatrix(D_8028A858++, (char *)m + 0x80000000, G_MTX_MODELVIEW | G_MTX_MUL | G_MTX_PUSH);
  gDPPipeSync(D_8028A858++);
  gDPSetCycleType(D_8028A858++, G_CYC_1CYCLE);
  if (D_8028AA78 != 0) {
    gDPSetCombine(D_8028A858++, 0x167E2C, 0x55FEF379);
    gDPSetEnvColor(D_8028A858++, D_8028AB20, D_8028AB24, D_8028AB28, D_8028AB2C);
  } else {
    gDPSetCombine(D_8028A858++, 0xFFFFFF, 0xFFFCF87C);
  }
  gDPSetRenderMode(D_8028A858++, 0x0F0A4200, 0);
  gDPSetTextureFilter(D_8028A858++, D_8028A898);
  gSPClearGeometryMode(D_8028A858++, 0xF0205);
  gSPSetGeometryMode(D_8028A858++, D_8028A8A8 == D_8028A8AC ? 0x2000 : 0x1000);
  gSPClearGeometryMode(D_8028A858++, D_8028A8A8 == D_8028A8AC ? 0x1000 : 0x2000);
  gDPSetTextureLOD(D_8028A858++, 0);
  gSPTexture(D_8028A858++, 0xFFFF, 0xFFFF, 0, 0, 1);
  gSPClearGeometryMode(D_8028A858++, 0xC0000);
  gDPTileSync(D_8028A858++);
  gRaw(D_8028A858++, 0xF5100000, 0x07000000);
  gRaw(D_8028A858++, 0xF50001F0, 0x06000000);
  gRaw(D_8028A858++, 0xF5000100, 0x05000000);
  gSPDisplayList(D_8028A858++, D_80025C50);
  gDPPipeSync(D_8028A858++);
  gDPSetCycleType(D_8028A858++, G_CYC_1CYCLE);
  gSPSetGeometryMode(D_8028A858++, 0x20205);
  gSPPopMatrix(D_8028A858++, 0);
}

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
