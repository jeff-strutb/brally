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
  char pad1c[0x78 - 0x1c];
  float mount[2];               /* 0x78  on a wheel: where it hangs off the body */
  float x80;                    /* 0x80  on a wheel: x1d8 held to [-0.4, 0] */
  float vel[3];                 /* 0x84 */
  char pad90[0xbc - 0x90];
  float m[4][4];                /* 0xBC  the body's matrix */
  char padfc[0x19c - 0xfc];
  void *hit;                    /* 0x19C  on a wheel: the ground plane under it */
  unsigned char surface;        /* 0x1A0  on a wheel: what it is on */
  char pad1a1[0x1a4 - 0x1a1];
  float hitN[4];                /* 0x1A4  that plane's normal and constant */
  int x1b4;                     /* 0x1B4  on a wheel: it is on the ground */
  float x1b8;                   /* 0x1B8  spring load per unit of compression squared */
  float x1bc;                   /* 0x1BC  load per unit of upward speed */
  char pad1c0[0x1d8 - 0x1c0];
  float x1d8;                   /* 0x1D8  on a wheel: minus 0x8025E96C's answer */
  char pad1dc[0x203 - 0x1dc];
  unsigned char x203;           /* 0x203  0x80: a wheel has just landed */
} BrRbBody;
#define SIGN(x) ((x) == 0.0f ? 0.0 : ((x) > 0 ? 1.0 : -1.0))
float BrWheelGroundProbe(BrRbBody *b, BrRbBody *w);
typedef struct { float v[3]; } BrTyreVec;
extern BrTyreVec D_802A4B94;            /* (0, 0, -1): the body's down axis */
extern unsigned short D_8037EA88[];     /* each grid cell's plane count */
extern float D_80379F80[][150][8];      /* and its planes (normal, d, corners...) */
int func_8025F18C(float x, float y);
void BrMat3MulVecRows(float out[3], float m[4][4], float v[3]);
void BrMat4RotateVecT(float out[3], float m[4][4], float v[3]);
float BrCrPlaneDist(float *n, float d, float *p);
int func_8025B3B0(float *plane, float *p);
typedef struct BrRbForce {      /* a force applied to a body */
  struct BrRbForce *next;       /* 0x00 */
  int frame;                    /* 0x04 */
  float f[3];                   /* 0x08 */
} BrRbForce;
void BrRbVelAtFlatPoint(float out[3], BrRbBody *b, BrRbBody *at);
/* -- end declarations -- */

/* WHAT IT DOES: The body's drag: a force of -110 times its velocity, and
 * -220 times it more while it moves faster than 4 with any wheel on
 * surface 4, unless the race mode is 3. */
/* @implements 0x8025E820 tgr BrTyreSkidCheck */
void BrTyreSkidCheck(BrRbBody *b, BrRbForce *f)
{
  int s0;
  int s1;
  int s2;
  int s3;
  float d[3];

  f->f[0] = b->vel[0] * -110.0f;
  f->f[1] = b->vel[1] * -110.0f;
  f->f[2] = b->vel[2] * -110.0f;
  s0 = b->sub[0]->surface;
  s1 = b->sub[1]->surface;
  s2 = b->sub[2]->surface;
  s3 = b->sub[3]->surface;
  if (sqrtf(b->vel[0] * b->vel[0] + b->vel[1] * b->vel[1] + b->vel[2] * b->vel[2]) > 4.0f
      && D_8028C800 != 3 && (s0 == 4 || s1 == 4 || s2 == 4 || s3 == 4)) {
    d[0] = b->vel[0] * -220.0f;
    d[1] = b->vel[1] * -220.0f;
    d[2] = b->vel[2] * -220.0f;
    f->f[0] += d[0];
    f->f[1] += d[1];
    f->f[2] += d[2];
  }
}

/* WHAT IT DOES: How far one wheel can drop before it meets the ground: its
 * mount point (height ignored) into the world by the body's matrix and the
 * body's down axis rotated the same way, then every plane of the grid cell
 * under that point within 2 of it is hit along that axis; the nearest hit
 * within 2 on an upward-facing plane (normal z above 0.2) whose triangle
 * contains it is recorded on the wheel (plane, surface byte, normal and
 * constant).  Returns the drop, or 100.  The PC twin is BrWheelGroundProbe.
 * RESIDUE (65): FP register choice from the ray dot product on -- the ROM
 * also loads world x and y ahead of the |t| test and spills them; ours loads
 * them after.  Frame and control flow match. */
/* @t4-pass 0x8025E96C 1 2026-09-29 compiles 26 best 65 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x8025E96C 2 2026-09-29 compiles 26 best 65 moved 0  (n64/tools/n64permute.py) */
/* @t3 0x8025E96C */
/* @implements 0x8025E96C tgr BrWheelGroundProbe */
float BrWheelGroundProbe(BrRbBody *b, BrRbBody *w)
{
  float mount[3];
  float world[3];
  float *pPl;
  float best;
  float t;
  float h;
  float d;
  int cell;
  int i;
  int n;
  float dir[3];
  int x;                        /* x, y, pad: declared, never used; */
  int y;                        /* the frame holds them */
  BrTyreVec down;
  char pad[24];

  down = D_802A4B94;
  best = 100.0f;
  mount[0] = w->mount[0];
  mount[1] = w->mount[1];
  mount[2] = 0.0f;
  BrMat3MulVecRows(world, b->m, mount);
  BrMat4RotateVecT(dir, b->m, down.v);
  w->hit = 0;
  cell = func_8025F18C(world[0], world[1]);
  pPl = D_80379F80[cell][0];
  n = D_8037EA88[cell];
  for (i = 0; i < n; i++, pPl += 8) {
    d = BrCrPlaneDist(pPl, pPl[3], world);
    if (d > -2.0 && d < 2.0) {
      t = pPl[0] * dir[0] + pPl[1] * dir[1] + dir[2] * pPl[2];
      if ((t < 0.0f ? -t : t) > 0.001) {
        h = -(pPl[3] + (pPl[0] * world[0] + pPl[1] * world[1] + world[2] * pPl[2])) / t;
        mount[0] = dir[0] * h;
        mount[1] = dir[1] * h;
        mount[2] = dir[2] * h;
        mount[0] = mount[0] + world[0];
        mount[1] = mount[1] + world[1];
        mount[2] += world[2];
        if (h > -2.0 && h < 2.0 && h < best && pPl[2] > 0.2 && func_8025B3B0(pPl, mount) != 0) {
          w->hit = pPl;
          w->surface = ((unsigned char *)pPl)[0x1e];
          best = h;
          w->hitN[0] = pPl[0];
          w->hitN[1] = pPl[1];
          w->hitN[2] = pPl[2];
          w->hitN[3] = pPl[3];
        }
      }
    }
  }
  return best;
}

/* WHAT IT DOES: The four wheels' spring loads: each wheel counts frames on
 * the ground (to 100); pushed in past 0.3999 it is taken as bottomed out
 * (count reset, depth -0.3); the compression past -0.3, never negative, is
 * squared (keeping its sign) and scaled by the body's spring rate.  A wheel
 * back on the ground after a frame off it flags a landing.  The bottom-out
 * test is -0.4 + 0.0001 (BrTyreDepthAll's clamp; the folded double differs
 * from a literal -0.3999 in its last bit).
 * RESIDUE (53, same 109 instructions): the hoisted constants' FP registers
 * (the ROM keeps 1.0/-1.0 in f26/f28 and 0.0 in f20) and literal pool order
 * (the ROM pools -0.3 double before -0.3f). */
/* @t4-pass 0x8025EDBC 1 2026-09-29 compiles 26 best 53 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x8025EDBC 2 2026-09-29 compiles 26 best 53 moved 0  (n64/tools/n64permute.py) */
/* @t3 0x8025EDBC */
/* @implements 0x8025EDBC tgr BrTyreSprings */
void BrTyreSprings(BrRbBody *b)
{
  BrTyreLoad *w;
  int i;
  BrRbBody *s;
  short prev;
  float d;
  float t;

  w = b->loads;
  for (i = 0; i < 4; i++) {
    w->x8 = w->xc = 0.0f;
    switch (i) {
    case 0:
      s = b->sub[0];
      break;
    case 1:
      s = b->sub[1];
      break;
    case 2:
      s = b->sub[2];
      break;
    default:
      s = b->sub[3];
      break;
    }
    prev = s->x1b4;
    d = s->x1d8;
    if (s->x1b4 < 100) {
      s->x1b4++;
    }
    if (d <= -0.4 + 0.0001) {
      s->x1b4 = 0;
      d = -0.3;
    }
    if (0.0 < d) {
      d = 0.0f;
    }
    t = d - -0.3;
    if (t < 0.0f) {
      t = 0.0f;
    }
    t = SIGN(t) * t * t;
    w->load = t * b->x1b8;
    w = w->next;
    if (s->x1b4 != 0 && prev == 0) {
      b->x203 = 0x80;
    }
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

/* WHAT IT DOES: For each of the body's four wheels, take the negated
 * result of 0x8025E96C (asked with the body and that wheel), keep it at the
 * wheel's 0x1D8, and keep it held to [-0.4, 0] at the wheel's 0x80.  The
 * limits are double locals, set once before the loop. */
/* @implements 0x8025EC7C tgr BrTyreDepthAll */
void BrTyreDepthAll(BrRbBody *b)
{
  int i;
  double hi;
  BrRbBody *w;
  double lo;
  float f;

  hi = 0.0;
  lo = -0.4;
  for (i = 0; i < 4; i++) {
    switch (i) {
    case 0:
      w = b->sub[0];
      break;
    case 1:
      w = b->sub[1];
      break;
    case 2:
      w = b->sub[2];
      break;
    case 3:
      w = b->sub[3];
      break;
    }
    f = -BrWheelGroundProbe(b, w);
    w->x1d8 = f;
    if (f > hi) {
      f = 0.0f;
    }
    if (f < lo) {
      w->x80 = -0.4f;
    } else {
      w->x80 = f;
    }
  }
}
