/* tyre.c -- tyre grip, skids and surface effects
 */
#include "tgr/common.h"

/* -- declarations -- */
float sqrtf(float param_1);
extern int D_8025E8FC;
extern int D_8025E900;
extern int D_8028C800;
typedef struct BrTyreLoad {     /* one wheel's contact record */
  struct BrTyreLoad *next;      /* 0x00 */
  int x4;
  float x8;                     /* 0x08 */
  float xc;                     /* 0x0C */
  float load;                   /* 0x10  the wheel's load */
} BrTyreLoad;
typedef struct BrRbBody {       /* a rigid body with four attached wheels */
  int x0;
  struct BrRbBody *sub[4];      /* 0x04 */
  char pad14[0x18 - 0x14];
  BrTyreLoad *loads;            /* 0x18  one per wheel, linked */
  char pad1c[0x1b4 - 0x1c];
  int x1b4;                     /* 0x1B4  on a wheel: it is on the ground */
  char pad1b8[0x1bc - 0x1b8];
  float x1bc;                   /* 0x1BC  load per unit of upward speed */
} BrRbBody;
void BrRbVelAtFlatPoint(float out[3], BrRbBody *b, BrRbBody *at);
/* -- end declarations -- */

/* WHAT IT DOES: Decide whether the car's tyres are skidding: from the
 * body's sideways speed and each wheel's surface, and start or stop the
 * skid sound and marks. */
/* @t4-pass 0x8025E820 1 2026-09-26 compiles 17 best 93 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x8025E820 2 2026-09-26 compiles 17 best 93 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x8025E820 3 2026-09-26 compiles 17 best 93 moved 0  (n64/tools/n64permute.py) */
/* @implements 0x8025E820 tgr BrTyreSkidCheck */
void BrTyreSkidCheck(int param_1,int param_2)
{
  char cVar1;
  char cVar2;
  char cVar3;
  char cVar4;
  float fVar5;
  float fVar6;
  
  *(float *)(param_2 + 8) = *(float *)(param_1 + 0x84) * -110.0f;
  *(float *)(param_2 + 0xc) = *(float *)(param_1 + 0x88) * -110.0f;
  *(float *)(param_2 + 0x10) = *(float *)(param_1 + 0x8c) * -110.0f;
  cVar1 = *(char *)(*(int *)(param_1 + 4) + 0x1a0);
  cVar2 = *(char *)(*(int *)(param_1 + 8) + 0x1a0);
  cVar3 = *(char *)(*(int *)(param_1 + 0xc) + 0x1a0);
  cVar4 = *(char *)(*(int *)(param_1 + 0x10) + 0x1a0);
  fVar5 = (float)sqrtf(*(float *)(param_1 + 0x8c) * *(float *)(param_1 + 0x8c) +
                              *(float *)(param_1 + 0x84) * *(float *)(param_1 + 0x84) +
                              *(float *)(param_1 + 0x88) * *(float *)(param_1 + 0x88));
  if (((4.0f < fVar5) && (D_8028C800 != 3)) &&
     ((cVar1 == '\x04' || (((cVar2 == '\x04' || (cVar3 == '\x04')) || (cVar4 == '\x04')))))) {
    fVar6 = *(float *)(param_1 + 0x88);
    fVar5 = *(float *)(param_1 + 0x8c);
    *(float *)(param_2 + 8) = *(float *)(param_2 + 8) + *(float *)(param_1 + 0x84) * -220.0f;
    *(float *)(param_2 + 0xc) = *(float *)(param_2 + 0xc) + fVar6 * -220.0f;
    *(float *)(param_2 + 0x10) = *(float *)(param_2 + 0x10) + fVar5 * -220.0f;
  }
}

/* WHAT IT DOES: Work out each wheel's load for this step: for wheel i the
 * body's velocity at that wheel is taken, and a wheel on the ground whose
 * point is not moving down gets load z * x1bc; otherwise none. */
/* @implements 0x8025EF70 tgr BrTyreLoads */
void BrTyreLoads(BrRbBody *b)
{
  BrTyreLoad *w;
  int i;
  BrRbBody *s;
  float spare[3];                 /* unused: it only holds a stack slot */
  float v[3];
  float load;

  w = b->loads;
  for (i = 0; i < 4; i++) {
    w->x8 = w->xc = 0.0f;
    switch (i) {
    case 0:
      BrRbVelAtFlatPoint(v, b, b->sub[0]);
      s = b->sub[0];
      break;
    case 1:
      BrRbVelAtFlatPoint(v, b, b->sub[1]);
      s = b->sub[1];
      break;
    case 2:
      BrRbVelAtFlatPoint(v, b, b->sub[2]);
      s = b->sub[2];
      break;
    default:
      BrRbVelAtFlatPoint(v, b, b->sub[3]);
      s = b->sub[3];
      break;
    }
    if (s->x1b4 == 0) {
      load = 0.0f;
    } else if (v[2] < 0.0f) {
      load = 0.0f;
    } else {
      load = v[2] * b->x1bc;
    }
    w->load = load;
    w = w->next;
  }
}
