/* particles.c -- the particle pool: 256 records of 0x20 bytes on index
 * linked lists (index 0 ends a list), one free and one live.
 */
#include "tgr/common.h"
#include "tgr/car.h"
#include "tgr/gbi.h"

/* -- declarations -- */
/* A particle (0x20 bytes). */
typedef struct BrParticle {
    float pos[3];               /* 0x00 */
    float vel[3];               /* 0x0C */
    float size;                 /* 0x18 */
    unsigned short next;        /* 0x1C  index of the next on its list */
    unsigned char x1e;          /* 0x1E */
    unsigned char x1f;          /* 0x1F */
} BrParticle;
extern BrParticle D_80366A80[256];      /* indexed from 1 */
extern short D_80368A7C;
extern unsigned short D_8028C830;       /* head of the free list */
extern unsigned short D_8028C834;       /* head of the live list */
extern unsigned short D_8028C838;
extern unsigned short D_8028C83C;
extern int D_8028B7F4;                  /* cars in the race */
extern float D_8028AAD8;                /* seconds this frame */
extern float D_803634D0[3];             /* the wind: every particle drifts with it */
extern int D_8028AA80;                  /* race-kind flags (BrRaceSetKind) */
extern int D_8028AA84;
extern int D_8028AA8C;
typedef struct BrWeatherObj {           /* 0x78 bytes */
    char pad00[0x60];
    int x60;                            /* 0x60  its emitter, 0 for none */
    char pad64[0x78 - 0x64];
} BrWeatherObj;
extern BrWeatherObj D_803239A0[];
extern int D_8028B7F0;                  /* entries in D_803239A0 */
void func_8023B178(int e);
void func_8023B418(int e);
void func_8023C800(int e);
void func_8023CD60(void);
int func_8023BB50();
void func_8023D134(unsigned int param_1,int param_2,unsigned int param_3,unsigned int param_4);
extern Gfx *D_8028A858;
extern int D_8028A898;
extern int D_8028A89C;
extern int D_8028A8A0;
extern int D_802A3790;
int BrRandStep(void);
void BrVec3Scale(BrVec3 *pOut, BrVec3 *pV, float s);
void BrVec3Sub(BrVec3 *pOut, BrVec3 *pA, BrVec3 *pB);
void BrVec3MulAdd(BrVec3 *pOut, BrVec3 *pA, BrVec3 *pB, float s);
float BrVec3Length(BrVec3 *pV);
/* -- end declarations -- */

/* WHAT IT DOES: A car's exhaust smoke for one frame: its emit timer counts
 * up at a rate that grows with 0xDF4 plus a random part, and past a
 * quarter second a particle comes off the free list onto the live list,
 * blown backwards along the car, started between the last spot and one
 * behind the rear wheel (a random square's way), sized by the same rate
 * and made fainter the faster the car goes.  The PC twin is BrCarSub9020.
 * The rate is written out in the timer sum and named inside the branch
 * (IDO shares the product); the constants are literals (read-only, so the
 * product survives the stores between). */
/* @implements 0x8023B178 tgr BrCarSmokeEmit */
void BrCarSmokeEmit(BrCar *car)
{
  BrVec3 v;
  float g;
  int n;
  BrParticle *p;
  float f;
  float t;
  int r;

  r = BrRandStep();
  car->x1010 = car->x1010 + D_8028AAD8 * (1.0f + car->xdf4 * 0.001f + (float)(r & 0x1fff) * (1.0f / 65536.0f));
  if (car->x1010 > 0.25f && (n = D_8028C830) != 0) {
    f = car->xdf4 * 0.001f;
    car->x1010 = 0.0f;
    p = &D_80366A80[n];
    D_8028C830 = p->next;
    p->next = D_8028C834;
    D_8028C834 = n;
    BrVec3Scale((BrVec3 *)p->vel, (BrVec3 *)car, -1.5f - f);
    BrVec3Sub(&v, (BrVec3 *)car->wheelMtx[2][3], (BrVec3 *)car);
    BrVec3MulAdd(&v, &v, (BrVec3 *)car->mtx0[2], 0.2f);
    BrVec3MulAdd(&v, &v, (BrVec3 *)car->mtx0[1], 0.2f);
    g = (float)(BrRandStep() & 0xffff) * 1.5259021893143654e-05f;
    BrVec3Sub((BrVec3 *)p->pos, &car->smokeAt, &v);
    BrVec3MulAdd((BrVec3 *)p->pos, &v, (BrVec3 *)p->pos, g * g);
    car->smokeAt.x = v.x;
    car->smokeAt.y = v.y;
    car->smokeAt.z = v.z;
    f *= 0.1f;
    t = 1.0f + f;
    p->size = t * 0.15f;
    p->x1e = 1.0f / (BrVec3Length(&car->velfd8) + t) * 255.0f;
    p->x1f = 0xff;
  }
}

/* WHAT IT DOES: Step every live particle one frame: it grows, drifts with
 * the wind plus its own velocity scaled by its strength (x1f * x1e / 65280,
 * with a rise of 0.8 on z), and x1e becomes 5.7375 / size^2.  A particle
 * whose strength falls below 1/32 goes back on the free list.
 * RESIDUE (40): the ROM reads the next index before the position update and
 * keeps a copy of it for the loop (an extra register, p in a1); ours reads
 * it after.  Reading it first, index types and loop shapes score worse. */
/* @t4-pass 0x8023CBFC 1 2026-09-29 compiles 25 best 40 moved 4  (n64/tools/n64permute.py) */
/* @t4-pass 0x8023CBFC 2 2026-09-29 compiles 20 best 40 moved 0  (n64/tools/n64permute.py) */
/* @t3 0x8023CBFC */
/* @implements 0x8023CBFC tgr BrParticleStep */
void BrParticleStep(void)
{
  unsigned short *link;
  int n;
  int cur;
  BrParticle *p;
  float f;
  float grow;

  grow = 0.3f * D_8028AAD8;
  link = &D_8028C834;
  n = *link;
  while (n != 0) {
    cur = n;
    p = &D_80366A80[cur];
    p->size += grow;
    f = (float)(int)(p->x1f * p->x1e) * (1.0f / 65280.0f);
    p->pos[0] = p->pos[0] + (D_803634D0[0] + p->vel[0] * f * D_8028AAD8);
    p->pos[1] = (D_803634D0[1] + p->vel[1] * f * D_8028AAD8) + p->pos[1];
    p->pos[2] += (D_803634D0[2] + (p->vel[2] * f + 0.8f) * D_8028AAD8);
    p->x1e = (int)(5.7375f / (p->size * p->size));
    n = p->next;
    if (f < 0.03125f) {
      *link = n;
      p->next = D_8028C830;
      D_8028C830 = cur;
    } else {
      link = &p->next;
    }
  }
}

/* WHAT IT DOES: Step the two falling-particle lists one frame: as
 * BrParticleStep, but the particles also fall (z velocity loses
 * 19.62 * dt), x1e becomes 102 / size, and a particle is freed when its
 * strength falls below 1/32 or it falls faster than 30.
 * RESIDUE (72): the ROM re-reads p->next for the unlink and chooses the
 * list head per branch; ours reuses the early read (one more live register,
 * s0 saved).  Read order, index types, alias-breaking spellings and 294
 * permuter compiles leave it. */
/* @t4-pass 0x8023CD60 1 2026-09-29 compiles 26 best 72 moved 9  (n64/tools/n64permute.py) */
/* @t4-pass 0x8023CD60 2 2026-09-29 compiles 26 best 72 moved 0  (n64/tools/n64permute.py) */
/* @t3 0x8023CD60 */
/* @implements 0x8023CD60 tgr BrParticleFallStep */
void BrParticleFallStep(void)
{
  unsigned short *link;
  int n;
  int next;
  int k;
  BrParticle *p;
  float f;
  float grow;

  grow = 0.7f * D_8028AAD8;
  for (k = 0; k < 2; k++) {
    if (k != 0) {
      link = &D_8028C838;
    } else {
      link = &D_8028C83C;
    }
    n = *link;
    while (n != 0) {
      next = D_80366A80[n].next;
      p = &D_80366A80[n];
      p->size += grow;
      f = (float)(int)(p->x1f * p->x1e) * (1.0f / 65280.0f);
      p->pos[0] = p->pos[0] + (D_803634D0[0] + p->vel[0] * f * D_8028AAD8);
      p->pos[1] = (D_803634D0[1] + p->vel[1] * f * D_8028AAD8) + p->pos[1];
      p->pos[2] = p->pos[2] + (D_803634D0[2] + (p->vel[2] * f + 0.8f) * D_8028AAD8);
      p->vel[2] = p->vel[2] - D_8028AAD8 * 19.62f;
      p->x1e = (int)(102.0f / p->size);
      if (f < 0.03125f || p->vel[2] < -30.0f) {
        *link = p->next;
        p->next = D_8028C830;
        D_8028C830 = n;
      } else {
        link = &p->next;
      }
      n = next;
    }
  }
}

/* WHAT IT DOES: Empty the particle pool: chain every record onto the free
 * list (1 -> 2 -> ... -> 256), leave the live list empty, and clear each
 * racing car's 0x1010 field. */
/* @implements 0x8023CF30 tgr BrParticleReset */
void BrParticleReset(void)
{
  int i;
  BrParticle *p;

  for (i = 1, p = &D_80366A80[1]; i < 256; i++, p++) {
    p->next = i + 1;
  }
  D_80368A7C = 0;
  D_8028C830 = 1;
  for (i = 0; i < D_8028B7F4; i++) {
    D_8031B760[i].x1010 = 0.0f;
  }
  D_8028C834 = 0;
  D_8028C838 = 0;
  D_8028C83C = 0;
}

/* WHAT IT DOES: The particle frame: the pool is emptied on the first call;
 * then, by race kind, either the live particles are stepped and every
 * object's emitter runs (0x8023B178), or the other effect steps (0x8023CD60)
 * and each emitter runs 0x8023C800 -- in both cases followed by 0x8023B418
 * -- or, for the remaining kinds, only 0x8023B418 runs. */
/* @implements 0x8023CFC4 tgr BrParticleFrame */
void BrParticleFrame(void)
{
  static int D_8028C840 = 0;
  int i;
  BrWeatherObj *o;

  if (D_8028C840 == 0) {
    BrParticleReset();
    D_8028C840 = 1;
  }
  if (D_8028AA84 != 0) {
    BrParticleStep();
    for (i = 0, o = D_803239A0; i < D_8028B7F0; i++, o++) {
      if (o->x60 != 0) {
        func_8023B178(o->x60);
        func_8023B418(o->x60);
      }
    }
  } else if (D_8028AA80 == 0 && D_8028AA8C == 0) {
    func_8023CD60();
    for (i = 0, o = D_803239A0; i < D_8028B7F0; i++, o++) {
      if (o->x60 != 0) {
        func_8023C800(o->x60);
        func_8023B418(o->x60);
      }
    }
  } else {
    for (i = 0, o = D_803239A0; i < D_8028B7F0; i++, o++) {
      if (o->x60 != 0) {
        func_8023B418(o->x60);
      }
    }
  }
}

/* WHAT IT DOES: Draw the particles: one-cycle mode with the 64x64 I8
 * particle sprite (0x802A3790) loaded as a texture block, texture filter from
 * its setting, no LUT or perspective, primitive depth, no dither; for the
 * race kind that steps the particle pool the live list is drawn in pale blue
 * (0xE0, 0xE0, 0xFF) through 0x8023D134.  Then perspective, pixel depth, the
 * two dither settings and the alpha-compare threshold are restored and
 * 0x8023BB50 runs.  The texture-load commands are written as multi-line
 * blocks: on one line IDO schedules their two words the other way round. */
/* @implements 0x8023DBC0 tgr BrParticleDraw */
void BrParticleDraw(void)
{
  gDPPipeSync(D_8028A858++);
  gDPSetCycleType(D_8028A858++, 0);
  {
    Gfx *_g = D_8028A858++;

    _g->words.w0 = 0xbb000001;
    _g->words.w1 = 0xffffffff;
  }
  gSPSetOtherMode(D_8028A858++, G_SETOTHERMODE_H, 12, 2, D_8028A898);
  gDPSetCombine(D_8028A858++, 0xff97ff, 0xff2dfeff);
  {
    Gfx *_g = D_8028A858++;

    _g->words.w0 = 0xfd900000;
    _g->words.w1 = (unsigned int)&D_802A3790;
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
  gSPSetOtherMode(D_8028A858++, G_SETOTHERMODE_H, 14, 2, 0);
  gSPSetOtherMode(D_8028A858++, G_SETOTHERMODE_H, 19, 1, 0);
  gSPSetOtherMode(D_8028A858++, G_SETOTHERMODE_L, 2, 1, 4);
  gSPSetOtherMode(D_8028A858++, G_SETOTHERMODE_H, 6, 2, 0xc0);
  if (D_8028AA84 != 0) {
    gSPSetOtherMode(D_8028A858++, G_SETOTHERMODE_H, 4, 2, 0x80);
    gSPSetOtherMode(D_8028A858++, G_SETOTHERMODE_L, 3, 29, 0x504b50);
    func_8023D134(D_8028C834, 0xe0, 0xe0, 0xff);
  }
  gDPPipeSync(D_8028A858++);
  gSPSetOtherMode(D_8028A858++, G_SETOTHERMODE_H, 19, 1, 0x80000);
  gSPSetOtherMode(D_8028A858++, G_SETOTHERMODE_L, 2, 1, 0);
  gSPSetOtherMode(D_8028A858++, G_SETOTHERMODE_H, 6, 2, D_8028A8A0);
  gSPSetOtherMode(D_8028A858++, G_SETOTHERMODE_H, 4, 2, D_8028A89C);
  gSPSetOtherMode(D_8028A858++, G_SETOTHERMODE_L, 0, 2, 1);
  func_8023BB50();
}
