/* weather.c -- rain, snow and fog particles: their step and their drawing
 */
#include "tgr/common.h"
#include "tgr/car.h"
#include "tgr/gbi.h"

/* -- declarations -- */
typedef struct BrParticleSet {  /* 0xC3C bytes */
  short p[512][3];              /* particle positions */
  char pad[0xC3C - 0xC00];
} BrParticleSet;
extern BrParticleSet D_80361C40[2];
extern int D_8028C804;                  /* particles per view */
extern int D_8028AB0C;                  /* views on screen */
extern int D_8028AA84;                  /* snow */
extern int D_8028AA8C;                  /* rain with lightning */
extern int D_8028C824;                  /* the particles have been placed */
extern float D_8028AAD8;                /* the frame's time step */
extern float D_8028C808;                /* the wind's heading */
extern float D_8028C80C;                /* and its strength */
extern float D_8028C810;                /* the view's move this frame, per unit time */
extern float D_8028C814;                /* and the scale it was held to */
extern float D_803634B8[2][3];          /* each view's point last frame */
extern float D_803634E0[2][3];          /* each view's rain drift */
void BrWindUpdate(void);
void BrLightningStep(void);
void BrParticlesInit(void);
int BrRandStep(void);
float sqrtf(float x);
float sinf(float x);
float cosf(float x);
void BrVec3MulAdd(float out[3], float a[3], float b[3], float s);
void BrVec3ScaleBy(float v[3], float s);
void BrVec3AddTo(float out[3], float v[3]);
typedef struct BrTrackObj {     /* a track object (0x54 bytes) */
  float m[4][4];
  int x40;
  unsigned int dl;
  unsigned short hide;
  unsigned short x4a;
  unsigned short flags;         /* 0x4C  0x10: a tunnel, no rain or snow */
  unsigned short tris;
  unsigned short vtxs;
  unsigned short x52;
} BrTrackObj;
extern BrTrackObj *D_80025C60;          /* the track's objects */
extern unsigned short D_8031B248[];     /* the objects around the camera */
extern int D_8028AB00;                  /* and how many */
extern int D_8028AAEC;                  /* the view being drawn */
extern BrCar *D_8028AAF4;               /* its camera */
extern Gfx *D_8028A858;
extern int D_8028A898;                  /* the texture filter mode */
extern int D_8028A8A8;                  /* mirror flags: they differ when the view is mirrored */
extern int D_8028A8AC;
extern int D_802A3790;                  /* the snowflake texture */
extern unsigned char D_80364A80[2][64][64];     /* each view's rain streak texture */
extern float D_8031AB10[4][4];
extern float D_8031AB50[4][4];
extern float D_8031AA90[4][4];
int BrMat4Inverse(float out[4][4], float in[4][4]);
void BrMat4Mul(float r[4][4], float a[4][4], float b[4][4]);
void BrVec3Project(float out[3], float v[3], float m[4][4]);
void BrVec3Normalise(float v[3]);
void BrPerfMark(int bar, int r, int g, int b, int a);
void BrFill64(void *p, int n, int v);
/* -- end declarations -- */

/* WHAT IT DOES: Does nothing. An empty function the retail build kept just
 * before the weather particle code. */
/* @implements 0x8023A1C0 tgr BrStub8023A1C0 */
void BrStub8023A1C0(void)
{
}

/* WHAT IT DOES: One frame of the rain or snow: step the wind (and the
 * lightning in a storm), share the 512 particles between the views, and
 * for each view take the point 3 ahead of its camera; the first time,
 * scatter the particles there.  The particles move against the view's
 * motion (held to a steady rate above 1/3.6 per unit time), rain also
 * drifting with the wind and falling twice the time step, snow falling
 * half of it and jittering left and right at random, as 16.16-scaled
 * shorts; each particle's jitter flips sign every 0-15 frames.
 * The view's camera is reached as D_8031B760[n] (uopt strength-reduces it
 * to a pointer kept in a spill slot) and the drift as D_803634E0[n][k], so
 * the stores to v do not hide the drift values from the adds.  The point
 * is copied into x, y and z where it is used.  The particle rows go
 * through pp, whose stores may alias the particle count, so the count is
 * reloaded after the last store; the rows and the short deltas are
 * computed after the loop's entry test.  u is never read; it holds its
 * frame slot. */
/* @implements 0x8023A1C8 tgr BrWeatherStep */
void BrWeatherStep(void)
{
  float x;
  float y;
  float z;
  float dx;
  float dy;
  float dz;
  float jx;
  float jy;
  unsigned int cx;
  unsigned int cy;
  int i;
  short (*pp)[3];
  float cur[3];
  int n;
  short sjx;
  short sjy;
  short sdx;
  short sdy;
  short sdz;
  int u[3];
  float v[3];

  BrWindUpdate();
  if (D_8028AA8C != 0) {
    BrLightningStep();
  }
  D_8028C804 = 512 / D_8028AB0C;
  for (n = 0; n < D_8028AB0C; n++) {
    BrVec3MulAdd(cur, D_8031B760[n].cam->mtx[3], D_8031B760[n].cam->mtx[0], 3.0f);
    if (D_8028C824 == 0) {
      BrParticlesInit();
      x = cur[0];
      y = cur[1];
      z = cur[2];
      D_803634B8[0][0] = x;
      D_803634B8[1][0] = x;
      D_803634B8[0][1] = y;
      D_803634B8[1][1] = y;
      D_803634B8[0][2] = z;
      D_803634B8[1][2] = z;
      D_8028C824 = 1;
    }
    x = cur[0];
    y = cur[1];
    z = cur[2];
    if (D_8028AA84 == 0 && D_8028AA8C == 0) {
      return;
    }
    dx = x - D_803634B8[n][0];
    dy = y - D_803634B8[n][1];
    dz = z - D_803634B8[n][2];
    D_8028C810 = sqrtf(dx * dx + dy * dy + dz * dz) / D_8028AAD8;
    if (D_8028C810 > 0.27777777f) {
      D_8028C814 = sqrtf(D_8028C810 * 3.6000001f) * 0.27777777f / D_8028C810;
      D_8028C810 = D_8028C810 * D_8028C814;
      dx = dx * D_8028C814;
      dy = dy * D_8028C814;
      dz = dz * D_8028C814;
    } else {
      D_8028C814 = 1.0f;
    }
    if (D_8028AA84 != 0) {
      dz += D_8028AAD8 * 0.5f;
    } else {
      D_803634E0[n][0] = cosf(D_8028C808) * (D_8028AAD8 * D_8028C80C);
      D_803634E0[n][1] = sinf(D_8028C808) * (D_8028AAD8 * D_8028C80C);
      D_803634E0[n][2] = D_8028AAD8 + D_8028AAD8;
      v[0] = dx;
      v[1] = dy;
      v[2] = dz;
      dx += D_803634E0[n][0];
      dy += D_803634E0[n][1];
      dz += D_803634E0[n][2];
      BrVec3ScaleBy(v, D_8028C814 * 0.5f);
      BrVec3AddTo(D_803634E0[n], v);
    }
    if (D_8028AA84 != 0) {
      jx = ((BrRandStep() & 0xffff) * 3.05180437862873e-05f - 1.0f) * D_8028AAD8 * 0.25f;
      jy = ((BrRandStep() & 0xffff) * 3.05180437862873e-05f - 1.0f) * D_8028AAD8 * 0.25f;
    } else {
      jx = jy = 0.0f;
    }
    x = cur[0];
    y = cur[1];
    z = cur[2];
    D_803634B8[n][0] = x;
    D_803634B8[n][1] = y;
    D_803634B8[n][2] = z;
    cx = BrRandStep() & 0xf;
    cy = BrRandStep() & 0xf;
    sjx = jx * 16383.5;
    sjy = jy * 16383.5;
    pp = D_80361C40[n].p;
    sdx = dx * 16383.5;
    sdy = dy * 16383.5;
    sdz = dz * 16383.5;
    for (i = 0; i < D_8028C804; i++) {
      if (cx != 0) {
        cx--;
        pp[i][0] += sdx;
      } else {
        pp[i][0] = pp[i][0] + sdx + sjx;
        sjx = -sjx;
        cx = BrRandStep() & 0xf;
      }
      if (cy != 0) {
        cy--;
        pp[i][1] += sdy;
      } else {
        pp[i][1] = pp[i][1] + sdy + sjy;
        sjy = -sjy;
        cy = BrRandStep() & 0xf;
      }
      pp[i][2] += sdz;
    }
  }
}

/* WHAT IT DOES: Draw the view's rain or snow, unless the camera is among
 * the track objects marked as covered.  Snow uses the snowflake texture;
 * rain draws a streak into the view's own 64x64 texture first, along the
 * rain's drift as the camera sees it (flipped in a mirrored view), 16 dots
 * of rising intensity.  Each particle is taken through the camera's
 * inverse and the projection; one in front of the camera, on screen and
 * at least 2 units across is drawn as a textured rectangle, 24x36 (rain)
 * or 128x128 (snow) over its distance.
 * Source facts: the particle loop reaches the camera matrix through the
 * pointer m, so each row element is a load of its own; it counts with k,
 * and the point's z goes into w.  The streak's direction d is declared in
 * the rain block, and its colour byte is written as (k << 4) | k at every
 * store.  Both mirror tests are D_8028A8A8 ^ D_8028A8AC.  The rectangle's
 * three packet pointers are cleared before the size tests: uopt numbers
 * them there, ahead of py, which gives them a0..a2 and py a3.  Their
 * stores share one source line, as the macro's would.  n, j, row, col, x0,
 * y0 and c are never read; they hold their frame slots. */
/* @implements 0x8023A784 tgr BrWeatherDraw */
void BrWeatherDraw(void)
{
  int n;
  short (*pt)[3];
  BrCar *car;
  float sx;
  float sy;
  float mirror;
  int i;
  int k;
  int ix;
  int iy;
  float x;
  float y;
  float fx;
  float fy;
  float w;
  float tx;
  float ty;
  int hw;
  int hh;
  int px;
  int py;
  unsigned char c;
  float (*m)[4];
  int j;
  int row;
  int col;

  if (D_8028AA84 == 0 && D_8028AA8C == 0) {
    return;
  }
  for (i = 0; i < D_8028AB00; i++) {
    if (D_80025C60[D_8031B248[i]].flags & 0x10) {
      return;
    }
  }
  pt = D_80361C40[D_8028AAEC].p;
  if (D_8028AA8C != 0) {
    BrStub8023A1C0();
  }
  gDPPipeSync(D_8028A858++);
  gRaw(D_8028A858++, 0xba001402, 0);
  {
    Gfx *_g = D_8028A858++;

    _g->words.w0 = 0xbb000001;
    _g->words.w1 = 0xffffffff;
  }
  gRaw(D_8028A858++, 0xba000c02, D_8028A898);
  gRaw(D_8028A858++, 0xfcffffff, 0xfffdf2f9);
  gRaw(D_8028A858++, 0xb900031d, 0x504240);
  {
    Gfx *_g = D_8028A858++;

    _g->words.w0 = 0xfd900000;
    if (D_8028AA8C != 0) {
      _g->words.w1 = (unsigned int)D_80364A80[D_8028AAEC];
    } else {
      _g->words.w1 = (unsigned int)&D_802A3790;
    }
  }
  {
    Gfx *_g = D_8028A858++;

    _g->words.w0 = 0xf5900000;
    _g->words.w1 = 0x07018060;
  }
  gDPLoadSync(D_8028A858++);
  {
    Gfx *_g = D_8028A858++;

    _g->words.w0 = 0xf3000000;
    _g->words.w1 = 0x077ff100;
  }
  gDPPipeSync(D_8028A858++);
  {
    Gfx *_g = D_8028A858++;

    _g->words.w0 = 0xf5881000;
    _g->words.w1 = 0x00018060;
  }
  {
    Gfx *_g = D_8028A858++;

    _g->words.w0 = 0xf2000000;
    _g->words.w1 = 0x000fc0fc;
  }
  gRaw(D_8028A858++, 0xba000e02, 0);
  gRaw(D_8028A858++, 0xba001301, 0);
  if (D_8028AA84 != 0) {
    gRaw(D_8028A858++, 0xfa00ffff, 0xe0e0ffff);
  } else {
    gRaw(D_8028A858++, 0xfa00ffff, 0x788088ff);
  }
  car = D_8028AAF4;
  BrMat4Inverse(D_8031AB10, car->mtx0);
  D_8031AB10[3][2] = 0.0f;
  D_8031AB50[0][2] = 6.1037019e-05f;
  D_8031AB50[1][0] = 6.1037019e-05f;
  D_8031AB50[2][1] = 6.1037019e-05f;
  D_8031AB50[0][0] = 0.0f;
  D_8031AB50[0][1] = 0.0f;
  D_8031AB50[0][3] = 0.0f;
  D_8031AB50[1][1] = 0.0f;
  D_8031AB50[1][2] = 0.0f;
  D_8031AB50[1][3] = 0.0f;
  D_8031AB50[2][0] = 0.0f;
  D_8031AB50[2][2] = 0.0f;
  D_8031AB50[2][3] = 0.0f;
  D_8031AB50[3][0] = 0.0f;
  D_8031AB50[3][1] = 0.0f;
  D_8031AB50[3][2] = 0.0f;
  D_8031AB10[3][0] = D_8031AB10[3][1] = D_8031AB10[3][2];
  D_8031AB50[3][3] = 1.0f;
  BrMat4Mul(D_8031AB10, D_8031AB10, D_8031AB50);
  if (D_8028AA8C != 0) {
    float d[3];

    BrFill64(D_80364A80[D_8028AAEC], 0x1000, 0);
    BrVec3Project(d, D_803634E0[D_8028AAEC], D_8031AB10);
    BrVec3Normalise(d);
    if (D_8028A8A8 ^ D_8028A8AC) {
      d[0] = -d[0];
    }
    x = 32.0f - d[0] * 28.0f;
    y = d[1] * 28.0f + 32.0f;
    for (k = 0; k < 16; k++) {
      x += d[0] * 3.5f;
      y += -d[1] * 3.5f;
      ix = (int)x;
      iy = (int)y;
      if (iy > 1 && iy < 62) {
        if (ix > 1 && ix < 62) {
          D_80364A80[D_8028AAEC][iy - 2][ix - 1] = (k << 4) | k;
          D_80364A80[D_8028AAEC][iy - 2][ix] = (k << 4) | k;
          D_80364A80[D_8028AAEC][iy - 2][ix + 1] = (k << 4) | k;
          D_80364A80[D_8028AAEC][iy - 1][ix - 2] = (k << 4) | k;
          D_80364A80[D_8028AAEC][iy - 1][ix - 1] = (k << 4) | k;
          D_80364A80[D_8028AAEC][iy - 1][ix] = (k << 4) | k;
          D_80364A80[D_8028AAEC][iy - 1][ix + 1] = (k << 4) | k;
          D_80364A80[D_8028AAEC][iy - 1][ix + 2] = (k << 4) | k;
          D_80364A80[D_8028AAEC][iy][ix - 2] = (k << 4) | k;
          D_80364A80[D_8028AAEC][iy][ix - 1] = (k << 4) | k;
          D_80364A80[D_8028AAEC][iy][ix] = (k << 4) | k;
          D_80364A80[D_8028AAEC][iy][ix + 1] = (k << 4) | k;
          D_80364A80[D_8028AAEC][iy][ix + 2] = (k << 4) | k;
          D_80364A80[D_8028AAEC][iy + 1][ix - 2] = (k << 4) | k;
          D_80364A80[D_8028AAEC][iy + 1][ix - 1] = (k << 4) | k;
          D_80364A80[D_8028AAEC][iy + 1][ix] = (k << 4) | k;
          D_80364A80[D_8028AAEC][iy + 1][ix + 1] = (k << 4) | k;
          D_80364A80[D_8028AAEC][iy + 1][ix + 2] = (k << 4) | k;
          D_80364A80[D_8028AAEC][iy + 2][ix - 1] = (k << 4) | k;
          D_80364A80[D_8028AAEC][iy + 2][ix] = (k << 4) | k;
          D_80364A80[D_8028AAEC][iy + 2][ix + 1] = (k << 4) | k;
        }
      }
    }
  }
  BrPerfMark(0, 0x80, 0xff, 0xff, 0xff);
  BrMat4Mul(D_8031AB10, D_8031AB10, D_8031AA90);
  if (D_8028AA84 != 0) {
    sx = 24.0f;
    sy = 36.0f;
  } else {
    sy = sx = 128.0f;
  }
  if (D_8028A8A8 ^ D_8028A8AC) {
    mirror = -640.0f;
  } else {
    mirror = 640.0f;
  }
  m = D_8031AB10;
  for (k = 0; k < D_8028C804; k++) {
    int x0;
    int y0;
    Gfx *g1;
    Gfx *g2;
    Gfx *g3;

    fx = pt[k][0];
    fy = pt[k][1];
    w = pt[k][2];
    tx = (fx * m[0][0] + fy * m[1][0] + w * m[2][0]) + m[3][0];
    ty = (fx * m[0][1] + fy * m[1][1] + w * m[2][1]) + m[3][1];
    w = (fx * m[0][3] + fy * m[1][3] + w * m[2][3]) + m[3][3];
    if (w < 0.1f) {
      continue;
    }
    w = 0.5f / w;
    ty *= w;
    if (ty < -1.0f || 1.0f < ty) {
      continue;
    }
    tx *= w;
    if (tx < -1.0f || 1.0f < tx) {
      continue;
    }
    px = tx * mirror;
    g1 = NULL;
    g2 = NULL;
    g3 = NULL;
    hw = sx * w;
    if (hw < 2) {
      continue;
    }
    hh = sy * w;
    if (hh < 2) {
      continue;
    }
    py = ty * 480.0f;
    g1 = D_8028A858++;                                                                      \
    g1->words.w0 = (_SHIFTL(G_TEXRECT, 24, 8) | _SHIFTL(px + hw + 0x280, 12, 12) |          \
                    _SHIFTL(py + hh + 0x1e0, 0, 12));                                       \
    g1->words.w1 = (_SHIFTL(0, 24, 3) | _SHIFTL(px + 0x280, 12, 12) | _SHIFTL(py + 0x1e0, 0, 12)); \
    g2 = D_8028A858++;                                                                      \
    g2->words.w0 = _SHIFTL(G_RDPHALF_1, 24, 8);                                             \
    g2->words.w1 = (unsigned int)(_SHIFTL(0, 16, 16) | _SHIFTL(0x7e0, 0, 16));              \
    g3 = D_8028A858++;                                                                      \
    g3->words.w0 = _SHIFTL(G_RDPHALF_2, 24, 8);                                             \
    g3->words.w1 = (unsigned int)(_SHIFTL(0x3f000 / hw, 16, 16) | _SHIFTL(-0x3f000 / hh, 0, 16));

  }
  gDPPipeSync(D_8028A858++);
  gRaw(D_8028A858++, 0xba001301, 0x80000);
}
