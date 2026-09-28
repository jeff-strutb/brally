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
/* -- end declarations -- */

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
