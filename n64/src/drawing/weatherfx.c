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
