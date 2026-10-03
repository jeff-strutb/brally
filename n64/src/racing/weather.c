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
 * RESIDUE (~320): the ROM's frame is 8 larger with the car pointer and view
 * index in stack homes and the view point re-read from its home; ours keeps
 * the point in saved FP registers, which moves every register after. */
/* @implements 0x8023A1C8 tgr BrWeatherStep */
void BrWeatherStep(void)
{
  BrCar *car;
  float v[3];
  int n;
  float cur[3];
  float dx;
  float dy;
  float dz;
  float *w;
  float jx;
  float jy;
  unsigned int cx;
  unsigned int cy;
  short sjx;
  short sjy;
  short sdx;
  short sdy;
  short sdz;
  short *p;
  int i;

  BrWindUpdate();
  if (D_8028AA8C != 0) {
    BrLightningStep();
  }
  D_8028C804 = 512 / D_8028AB0C;
  for (n = 0, car = D_8031B760; n < D_8028AB0C; n++, car++) {
    BrVec3MulAdd(cur, car->cam->mtx[3], car->cam->mtx[0], 3.0f);
    if (D_8028C824 == 0) {
      BrParticlesInit();
      D_803634B8[1][0] = D_803634B8[0][0] = cur[0];
      D_803634B8[1][1] = D_803634B8[0][1] = cur[1];
      D_803634B8[1][2] = D_803634B8[0][2] = cur[2];
      D_8028C824 = 1;
    }
    if (D_8028AA84 == 0 && D_8028AA8C == 0) {
      return;
    }
    dx = cur[0] - D_803634B8[n][0];
    dy = cur[1] - D_803634B8[n][1];
    dz = cur[2] - D_803634B8[n][2];
    D_8028C810 = sqrtf(dx * dx + dy * dy + dz * dz) / D_8028AAD8;
    if (D_8028C810 > 0.27777777f) {
      D_8028C814 = sqrtf(D_8028C810 * 3.6f) * 0.27777777f / D_8028C810;
      D_8028C810 = D_8028C810 * D_8028C814;
      dx = dx * D_8028C814;
      dy = dy * D_8028C814;
      dz = dz * D_8028C814;
    } else {
      D_8028C814 = 1.0f;
    }
    if (D_8028AA84 == 0) {
      w = D_803634E0[n];
      w[0] = cosf(D_8028C808) * (D_8028AAD8 * D_8028C80C);
      w[1] = sinf(D_8028C808) * (D_8028AAD8 * D_8028C80C);
      w[2] = D_8028AAD8 + D_8028AAD8;
      v[0] = dx;
      v[1] = dy;
      v[2] = dz;
      dx += w[0];
      dy += w[1];
      dz += w[2];
      BrVec3ScaleBy(v, D_8028C814 * 0.5f);
      BrVec3AddTo(w, v);
    } else {
      dz += D_8028AAD8 * 0.5f;
    }
    if (D_8028AA84 == 0) {
      jx = 0.0f;
      jy = 0.0f;
    } else {
      jx = ((unsigned short)BrRandStep() * 3.05180437862873e-05f - 1.0f) * D_8028AAD8 * 0.25f;
      jy = ((unsigned short)BrRandStep() * 3.05180437862873e-05f - 1.0f) * D_8028AAD8 * 0.25f;
    }
    D_803634B8[n][0] = cur[0];
    D_803634B8[n][1] = cur[1];
    D_803634B8[n][2] = cur[2];
    cx = BrRandStep() & 0xf;
    cy = BrRandStep() & 0xf;
    sjx = jx * 16383.5;
    sjy = jy * 16383.5;
    p = D_80361C40[n].p[0];
    sdx = dx * 16383.5;
    sdy = dy * 16383.5;
    sdz = dz * 16383.5;
    for (i = 0; i < D_8028C804; i++, p += 3) {
      if (cx == 0) {
        p[0] = p[0] + sdx + sjx;
        sjx = -sjx;
        cx = BrRandStep() & 0xf;
      } else {
        cx--;
        p[0] += sdx;
      }
      if (cy == 0) {
        p[1] = p[1] + sdy + sjy;
        sjy = -sjy;
        cy = BrRandStep() & 0xf;
      } else {
        cy--;
        p[1] += sdy;
      }
      p[2] += sdz;
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
 * RESIDUE (gap 145, 11 short): ours keeps the camera matrix's address in
 * s0 and 28.0 in f20 across the calls, where the ROM re-forms the address
 * at each use and holds only 1.0; the rain direction sits at sp+0x60 in
 * the ROM's frame. */
/* @implements 0x8023A784 tgr BrWeatherDraw */
void BrWeatherDraw(void)
{
  short (*pt)[3];
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
  float fz;
  float w;
  float tx;
  float ty;
  int hw;
  int hh;
  int px;
  int py;
  unsigned char c;
  float d[3];

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
  gRaw(D_8028A858++, 0xbb000001, 0xffffffff);
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
  gRaw(D_8028A858++, 0xf5900000, 0x07018060);
  gDPLoadSync(D_8028A858++);
  gRaw(D_8028A858++, 0xf3000000, 0x077ff100);
  gDPPipeSync(D_8028A858++);
  gRaw(D_8028A858++, 0xf5881000, 0x00018060);
  gRaw(D_8028A858++, 0xf2000000, 0x000fc0fc);
  gRaw(D_8028A858++, 0xba000e02, 0);
  gRaw(D_8028A858++, 0xba001301, 0);
  if (D_8028AA84 != 0) {
    gRaw(D_8028A858++, 0xfa00ffff, 0xe0e0ffff);
  } else {
    gRaw(D_8028A858++, 0xfa00ffff, 0x788088ff);
  }
  BrMat4Inverse(D_8031AB10, D_8028AAF4->mtx0);
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
    BrFill64(D_80364A80[D_8028AAEC], 0x1000, 0);
    BrVec3Project(d, D_803634E0[D_8028AAEC], D_8031AB10);
    BrVec3Normalise(d);
    if (D_8028A8A8 != D_8028A8AC) {
      d[0] = -d[0];
    }
    x = 32.0f - d[0] * 28.0f;
    y = d[1] * 28.0f + 32.0f;
    for (k = 0; k < 16; k++) {
      y += -d[1] * 3.5f;
      x += d[0] * 3.5f;
      iy = (int)y;
      if (iy > 1 && iy < 62) {
        ix = (int)x;
        if (ix > 1 && ix < 62) {
          c = (k << 4) | k;
          D_80364A80[D_8028AAEC][iy - 2][ix - 1] = c;
          D_80364A80[D_8028AAEC][iy - 2][ix] = c;
          D_80364A80[D_8028AAEC][iy - 2][ix + 1] = c;
          D_80364A80[D_8028AAEC][iy - 1][ix - 2] = c;
          D_80364A80[D_8028AAEC][iy - 1][ix - 1] = c;
          D_80364A80[D_8028AAEC][iy - 1][ix] = c;
          D_80364A80[D_8028AAEC][iy - 1][ix + 1] = c;
          D_80364A80[D_8028AAEC][iy - 1][ix + 2] = c;
          D_80364A80[D_8028AAEC][iy][ix - 2] = c;
          D_80364A80[D_8028AAEC][iy][ix - 1] = c;
          D_80364A80[D_8028AAEC][iy][ix] = c;
          D_80364A80[D_8028AAEC][iy][ix + 1] = c;
          D_80364A80[D_8028AAEC][iy][ix + 2] = c;
          D_80364A80[D_8028AAEC][iy + 1][ix - 2] = c;
          D_80364A80[D_8028AAEC][iy + 1][ix - 1] = c;
          D_80364A80[D_8028AAEC][iy + 1][ix] = c;
          D_80364A80[D_8028AAEC][iy + 1][ix + 1] = c;
          D_80364A80[D_8028AAEC][iy + 1][ix + 2] = c;
          D_80364A80[D_8028AAEC][iy + 2][ix - 1] = c;
          D_80364A80[D_8028AAEC][iy + 2][ix] = c;
          D_80364A80[D_8028AAEC][iy + 2][ix + 1] = c;
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
    sx = sy = 128.0f;
  }
  if (D_8028A8A8 != D_8028A8AC) {
    mirror = -640.0f;
  } else {
    mirror = 640.0f;
  }
  for (i = 0; i < D_8028C804; i++) {
    fx = pt[i][0];
    fy = pt[i][1];
    fz = pt[i][2];
    tx = D_8031AB10[3][0] + (fx * D_8031AB10[0][0] + fy * D_8031AB10[1][0] + fz * D_8031AB10[2][0]);
    ty = D_8031AB10[3][1] + (fx * D_8031AB10[0][1] + fy * D_8031AB10[1][1] + fz * D_8031AB10[2][1]);
    w = D_8031AB10[3][3] + (fx * D_8031AB10[0][3] + fy * D_8031AB10[1][3] + fz * D_8031AB10[2][3]);
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
    hw = sx * w;
    if (hw < 2) {
      continue;
    }
    hh = sy * w;
    if (hh < 2) {
      continue;
    }
    px = tx * mirror;
    py = ty * 480.0f;
    gSPTextureRectangle(D_8028A858++, px + 0x280, py + 0x1e0, px + hw + 0x280, py + hh + 0x1e0, 0, 0, 0x7e0,
                        0x3f000 / hw, -0x3f000 / hh);
  }
  gDPPipeSync(D_8028A858++);
  gRaw(D_8028A858++, 0xba001301, 0x80000);
}
