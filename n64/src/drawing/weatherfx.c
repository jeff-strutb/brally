/* weatherfx.c -- the rain and snow particles and the wind that drives them
 */
#include "tgr/common.h"

/* -- declarations -- */
typedef struct BrParticleSet {  /* 0xC3C bytes */
  short p[512][3];              /* particle positions */
  char pad[0xC3C - 0xC00];
} BrParticleSet;
int BrRandStep(void);
extern int D_8028C804;
extern BrParticleSet D_80361C40[2];
float sinf(float x);
float cosf(float x);
extern float D_8028AAD8;
extern float D_8028C808;
extern float D_8028C80C;
extern float D_803634D0[3];
extern int D_8028C818;                  /* lightning: flash frames left, -1 = none */
extern float D_8028C820;                /* how far the thunder has travelled */
typedef struct BrBoltPt { float x, y, z; } BrBoltPt;
extern BrBoltPt D_803634F8[];           /* the lightning bolt's points; [0] is where it struck */
extern int D_8028C81C;                  /* the bolt's subdivision depth, 4 per level */
extern float D_80025C38;                /* the track's sky height */
/* -- end declarations -- */

/* WHAT IT DOES: Scatter both particle sets: 512 particles each at random
 * positions. */
/* @implements 0x80239CF0 tgr BrParticlesInit */
void BrParticlesInit(void)
{
  int j;
  int i;

  D_8028C804 = 512;
  for (j = 0; j < 2; j++) {
    for (i = 0; i < 512; i++) {
      D_80361C40[j].p[i][0] = BrRandStep();
      D_80361C40[j].p[i][1] = BrRandStep();
      D_80361C40[j].p[i][2] = BrRandStep();
    }
  }
}

/* WHAT IT DOES: Let the wind wander: its heading and strength (0.5 to 1)
 * drift at random by up to a unit per second, and the per-frame wind
 * vector is set from them. */
/* @implements 0x80239D84 tgr BrWindUpdate */
void BrWindUpdate(void)
{
  D_8028C808 += ((int)(unsigned short)BrRandStep() * 3.0518044e-05f - 1.0f) * D_8028AAD8;
  if (D_8028C808 >= 6.2831855f) {
    D_8028C808 -= 6.2831855f;
  } else if (D_8028C808 < 0.0f) {
    D_8028C808 += 6.2831855f;
  }
  D_8028C80C += ((int)(unsigned short)BrRandStep() * 3.0518044e-05f - 1.0f) * D_8028AAD8;
  if (D_8028C80C > 1.0f) {
    D_8028C80C = 1.0f;
  } else if (D_8028C80C < 0.5f) {
    D_8028C80C = 0.5f;
  }
  D_803634D0[0] = cosf(D_8028C808) * (D_8028AAD8 * D_8028C80C);
  D_803634D0[1] = sinf(D_8028C808) * (D_8028AAD8 * D_8028C80C);
  D_803634D0[2] = 0.0f;
}

/* WHAT IT DOES: Build the lightning bolt by midpoint displacement: point
 * i becomes the average of points i - n and i + n, x and y each jittered by
 * a random amount within +-4n; then the two halves are split the same way
 * down to single steps (the depth counter up 4 per level meanwhile). */
/* @implements 0x80239F28 tgr BrBoltSplit */
void BrBoltSplit(int i, int n)
{
  int mask;
  int unused;                   /* declared, never used: its slot is in the frame */
  float half;

  mask = n * 8 - 1;
  half = mask * 0.5f;
  D_803634F8[i].x = (D_803634F8[i - n].x + D_803634F8[i + n].x) * 0.5f + (BrRandStep() & mask) - half;
  D_803634F8[i].y = (D_803634F8[i - n].y + D_803634F8[i + n].y) * 0.5f + (BrRandStep() & mask) - half;
  D_803634F8[i].z = (D_803634F8[i - n].z + D_803634F8[i + n].z) * 0.5f;
  n >>= 1;
  if (n != 0) {
    D_8028C81C += 4;
    BrBoltSplit(i - n, n);
    BrBoltSplit(i + n, n);
    D_8028C81C -= 4;
  }
}

/* WHAT IT DOES: Lightning: with none active, a 1-in-512 chance per frame
 * of a strike at a random point (x, y below 2048, at the sky height),
 * flashing for three frames; then the thunder front travels outward at
 * 343 units a second until it has gone 2048, when lightning may strike
 * again. */
/* @implements 0x8023A0BC tgr BrLightningStep */
void BrLightningStep(void)
{
  if (D_8028C818 >= 0) {
    D_8028C820 += 343.0f * D_8028AAD8;
    if (D_8028C818 > 0) {
      D_8028C818--;
    } else if (D_8028C820 > 2048.0) {
      D_8028C818 = -1;
    }
  } else if ((unsigned short)BrRandStep() < 0x80) {
    D_8028C818 = 3;
    D_8028C820 = 0.0f;
    D_803634F8[0].x = BrRandStep() & 0x7ff;
    D_803634F8[0].y = BrRandStep() & 0x7ff;
    D_803634F8[0].z = D_80025C38;
  }
}
