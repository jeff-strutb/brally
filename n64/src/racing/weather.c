/* weather.c -- rain, snow and fog particles
 */
#include "tgr/common.h"
#include "tgr/car.h"

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
