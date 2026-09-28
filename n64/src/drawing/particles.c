/* particles.c -- the particle pool: 256 records of 0x20 bytes on index
 * linked lists (index 0 ends a list), one free and one live.
 */
#include "tgr/common.h"
#include "tgr/car.h"

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
/* -- end declarations -- */

/* WHAT IT DOES: Step every live particle one frame: it grows, drifts with
 * the wind plus its own velocity scaled by its strength (x1f * x1e / 65280,
 * with a rise of 0.8 on z), and x1e becomes 5.7375 / size^2.  A particle
 * whose strength falls below 1/32 goes back on the free list.
 * RESIDUE (44): the ROM reads the next index before the position update and
 * keeps a copy of it for the loop (an extra register, p in a1); ours reads
 * it after.  Reading it first, index types and loop shapes score worse. */
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
    p->pos[1] = p->pos[1] + (D_803634D0[1] + p->vel[1] * f * D_8028AAD8);
    p->pos[2] = p->pos[2] + (D_803634D0[2] + (p->vel[2] * f + 0.8f) * D_8028AAD8);
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
