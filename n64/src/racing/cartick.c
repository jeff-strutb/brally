/* cartick.c -- a car's per-frame clocks and message timers
 */
#include "tgr/common.h"
#include "tgr/car.h"

/* -- declarations -- */
extern int D_8026FF18;
extern float D_8028AAD8;
typedef struct BrPathPt {       /* a point on a path segment (0x28 bytes) */
  BrVec3 left;                  /* 0x00  the track's left edge */
  BrVec3 pos;                   /* 0x0C  its centre */
  BrVec3 right;                 /* 0x18  its right edge */
  float dist;                   /* 0x24  distance along the track */
} BrPathPt;
typedef struct BrPathSeg {      /* a path segment */
  struct BrPathSeg *next;
  struct BrPathSeg *alt;
  char pad08[0x10 - 0x08];
  unsigned char x0, y0, x1, y1; /* 0x10  the grid cells it covers */
  unsigned short count;         /* 0x14  points */
  unsigned short flags;         /* 0x16  bit 0: closed */
  char pad18[0x40 - 0x18];
  BrPathPt pt[1];               /* 0x40 */
} BrPathSeg;
typedef struct BrTrackTri {     /* a collision triangle's record (8 bytes) */
  short x0;
  short x2;
  short x4;
  unsigned short surface;       /* 0x06  its surface id */
} BrTrackTri;
typedef struct BrTrackPaths {   /* the loaded track's header */
  char pad00[0x0C];
  BrTrackTri *tris;             /* 0x0C */
  char pad10[0x28 - 0x10];
  float x28;                    /* 0x28 */
  float x2c;                    /* 0x2C */
  char pad30[0x38 - 0x30];
  float x38;                    /* 0x38 */
  float x3c;                    /* 0x3C */
  char pad40[0x70 - 0x40];
  BrPathSeg *path;              /* 0x70  the path's first segment */
  char pad74[0x78 - 0x74];
  BrPathSeg **segs;             /* 0x78  every segment */
  int nSegs;                    /* 0x7C */
  char pad80[0x8C - 0x80];
  unsigned short *triggers;     /* 0x8C  zero-terminated trigger id lists */
  unsigned short *triTrigger;   /* 0x90  each triangle's list in triggers */
} BrTrackPaths;
typedef struct BrCollPlane {    /* a collision triangle's plane (0x20 bytes) */
  BrVec3 n;                     /* its normal */
  float d;
  BrVec3 *v0;                   /* 0x10  its corners */
  BrVec3 *v1;
  BrVec3 *v2;
  short tri;                    /* 0x1C  its triangle */
  short x1e;
} BrCollPlane;
extern BrCollPlane D_80379F80[][150];   /* each grid cell's collision planes */
extern unsigned short D_8037EA88[];     /* and how many */
extern BrVec3 D_8028B318;               /* the default normals */
extern BrVec3 D_8028B324;
int func_8025F18C(float x, float y);
void BrVec3Negate(BrVec3 *out, BrVec3 *v);
void BrVec3MulAdd(BrVec3 *out, BrVec3 *a, BrVec3 *b, float s);
int BrTriContainsPoint(BrVec3 *pPt, BrVec3 *pA, BrVec3 *pB, BrVec3 *pC, BrVec3 *pRef);
extern BrTrackPaths D_80025C00;
extern BrPathSeg *D_80025C70;           /* the track's path (0 before a track loads) */
int BrFloatToInt(float f);
int BrCarTrackLocate(BrCar *car);
int BrSeg2SideTest(float *a, float *b, float *c, float *d);
float BrVec3Dist(BrVec3 *a, BrVec3 *b);
float BrVec3Dot(BrVec3 *a, BrVec3 *b);
void BrVec3Normalise(BrVec3 *v);
extern int D_8028B940;
extern int D_8026FF08;
extern int D_8026FF10;
typedef struct BrCarEnt {       /* a car's entity record (0x78 bytes) */
  char pad00[0x60];
  BrCar *car;                   /* 0x60 */
} BrCarEnt;
void BrHudArrowDraw(BrCar *car, BrVec3 *at, short kind);
void BrWrongWayCheck(BrCar *car);
void func_8022A0E0(BrCarEnt *e);
void BrVec3Sub(BrVec3 *out, BrVec3 *a, BrVec3 *b);
void BrVec3ScaleBy(BrVec3 *v, float s);
float BrAtan2(float x, float y);
typedef struct BrCarWheel {     /* a wheel's rigid body */
  char pad000[0x1C0];
  float x1c0;                   /* 0x1C0 */
  char pad1c4[0x1CC - 0x1C4];
  float x1cc;                   /* 0x1CC */
  float x1d0;                   /* 0x1D0 */
} BrCarWheel;
typedef struct BrViewRect { int x; int y; int w; int h; int car; } BrViewRect;
extern BrViewRect D_8031B2C8[2];        /* the players' views */
extern int D_8028AB0C;                  /* number of views */
int BrGroundRay(BrVec3 *pPosOut, BrVec3 *pNormOut, BrVec3 *pEye, unsigned short *pNearIds, int *pGotHit,
                unsigned short *pFarIds, int *pFarCount, float *pDistOut, int *pFaceOut);
void func_80222050(BrCar *car);
void func_80221940(BrCar *car);
void func_80221170(BrCar *car);
void BrVec3Scale(BrVec3 *out, BrVec3 *v, float s);
void BrVec3AddTo(BrVec3 *a, BrVec3 *b);
void BrVec3Cross(BrVec3 *out, BrVec3 *a, BrVec3 *b);
void BrCarSetPos(BrCar *car, float x, float y, float z);
void BrCarSetVel(BrCar *car, float x, float y, float z);
void BrCarSetAngVel(BrCar *car, float x, float y, float z);
void BrCarRotate(BrCar *car, float x, float y, float z);
void BrPadConsume(void *pad, int button);
void BrCarBuildMatrices(BrCar *car);
float sqrtf(float x);
/* -- end declarations -- */

/* WHAT IT DOES: Find the path point under a car: keep its last one while
 * the car has not crossed either of that span's edges and is within 64 of
 * it; otherwise search every segment whose grid box holds the car's cell
 * (players' cars and mode 2 also the closed ones), within 1000 of its race
 * distance, for the nearest point it is ahead of.  The race distance moves
 * by the car's progress along that point (a jump of 1000 or more is
 * ignored); the segment, point and the point's across direction are kept.
 * Returns 0 when no point is found.  One corner of the tracks 3 and 8 grid
 * is searched as a single cell with no distance check.  Ported from the PC
 * twin (BrCarTrackLocate).
 * RESIDUE (231): our IDO hoists the 1000.0f of the segment distance test
 * out of the search loop (the ROM rematerialises it at each test), which
 * shifts the loop body; x/z take f26/f30 the other way round and the car
 * position pointer s1 for s2.  Structure, frame and calls match. */
/* @t4-pass 0x8021EB50 1 2026-09-29 compiles 13 best 230 moved 1  (n64/tools/n64permute.py) */
/* @t4-pass 0x8021EB50 2 2026-09-29 compiles 13 best 230 moved 0  (n64/tools/n64permute.py) */
/* @t3 0x8021EB50 */
/* @implements 0x8021EB50 tgr BrCarTrackLocate */
int BrCarTrackLocate(BrCar *car)
{
  float x;
  float y;
  float z;
  unsigned char cx;
  unsigned char cy;
  int i;
  BrPathSeg *seg;
  int best;
  int bestSeg;
  int k;
  int j;
  int n;
  BrPathPt *p;
  float bestD;
  float d;
  int check;
  float dist;
  BrVec3 dir;
  BrVec3 rel;
  BrVec3 across;
  BrVec3 off;

  x = car->mtx0[3][0];
  y = car->mtx0[3][1];
  z = car->mtx0[3][2];
  check = 1;
  cx = car->cellX;
  cy = car->cellY;
  best = -1;
  bestSeg = 0;
  if ((D_8028B940 == 3 || D_8028B940 == 8) && cx >= 0x38 && cx < 0x3B && cy >= 0x17 && cy < 0x1C) {
    cx = 0x39;
    cy = 0x19;
    check = 0;
  }
  dist = car->xfa8 - D_80025C70->pt[0].dist * car->xf7c;
  if (check &&
      !BrSeg2SideTest(&((BrPathSeg *)car->xf5c)->pt[car->xf60].left.x, &((BrPathSeg *)car->xf5c)->pt[car->xf60].right.x,
                      &car->posPrev.x, car->mtx0[3]) &&
      !BrSeg2SideTest(&((BrPathSeg *)car->xf5c)->pt[car->xf60 + 1].left.x, &((BrPathSeg *)car->xf5c)->pt[car->xf60 + 1].right.x,
                      &car->posPrev.x, car->mtx0[3]) &&
      BrVec3Dist((BrVec3 *)car->mtx0[3], &((BrPathSeg *)car->xf5c)->pt[car->xf60].pos) < 64.0f) {
    seg = (BrPathSeg *)car->xf5c;
    i = car->xf60;
  } else {
    bestD = D_80025C00.x2c - D_80025C00.x28;
    bestD *= bestD;
    for (k = 0; k < D_80025C00.nSegs; k++) {
      if (car->slot < D_8026FF08 || D_8026FF18 == 2 || !(D_80025C00.segs[k]->flags & 1)) {
        seg = D_80025C00.segs[k];
        if (cx < seg->x0 || cx > seg->x1 || cy < seg->y0 || cy > seg->y1) {
          continue;
        }
        if (check && ((D_80025C70->pt[0].dist - seg->pt[0].dist) - dist > 1000.0f ||
                      dist - (D_80025C70->pt[0].dist - seg->pt[seg->count].dist) > 1000.0f)) {
          continue;
        }
        n = seg->count;
        for (j = 0, p = seg->pt; j < n; j++, p++) {
          d = (p->pos.x - x) * (p->pos.x - x) + (p->pos.y - y) * (p->pos.y - y) + (p->pos.z - z) * (p->pos.z - z);
          if (d < bestD) {
            dir.x = p->left.y - p->right.y;
            dir.y = p->right.x - p->left.x;
            dir.z = 0.0f;
            BrVec3Sub(&rel, (BrVec3 *)car->mtx0[3], &p->pos);
            if (BrVec3Dot(&dir, &rel) >= 0.0f) {
              bestD = d;
              best = j;
              bestSeg = k;
            }
          }
        }
      }
    }
    if (best == -1) {
      return 0;
    }
    seg = D_80025C00.segs[bestSeg];
    i = best;
  }
  p = &seg->pt[i];
  across.x = p->left.y - p->right.y;
  across.z = 0.0f;
  across.y = p->right.x - p->left.x;
  BrVec3Normalise(&across);
  BrVec3Sub(&off, (BrVec3 *)car->mtx0[3], &p->pos);
  d = BrVec3Dot(&across, &off) + (D_80025C70->pt[0].dist * (car->xf7c + 1) - p->dist) - car->xfa8;
  if (!check || (-1000.0f < d && d < 1000.0f)) {
    car->xfa8 += d;
  }
  car->xf60 = i;
  car->xf5c = (int)seg;
  car->xf64 = across.x;
  car->xf68 = across.y;
  car->xf6c = across.z;
  return 1;
}

/* WHAT IT DOES: Advance a car's clocks by one frame while it is still in
 * the race: the race clock always, the lap clock except in mode 3, and in
 * mode 1 run a countdown down to zero.  The PC twin is BrCarTickClocks. */
/* @implements 0x8021F0A8 tgr BrCarTickClocks */
void BrCarTickClocks(BrCar *car)
{
  if ((car->link->flags & 3) == 0) {
    if (D_8026FF18 == 3) {
      car->raceTime += D_8028AAD8;
      return;
    }
    car->lapTime += D_8028AAD8;
    car->raceTime += D_8028AAD8;
    if (D_8026FF18 == 1) {
      car->xfa4 = car->xfa4 - D_8028AAD8;
      if (car->xfa4 < 0.0f) {
        car->xfa4 = 0.0f;
      }
    }
  }
}

/* WHAT IT DOES: Count down a car's on-screen message timers by one frame:
 * the first message while it runs, else the second; a timer that runs out
 * clears its message.  The PC twin is BrCarTickMessages.  The running
 * tests compare with an integer 0, the rest with 0.0f: two constants, so
 * IDO materialises the zero again inside each branch as the ROM does. */
/* @implements 0x8021F158 tgr BrCarTickMessages */
void BrCarTickMessages(BrCar *car)
{
  if (car->msgATime != 0) {
    car->msgATime = car->msgATime - D_8028AAD8;
    if (car->msgATime <= 0.0f) {
      car->msgATime = 0.0f;
      car->msgA = 0;
    }
  } else if (car->msgBTime != 0) {
    car->msgBTime = car->msgBTime - D_8028AAD8;
    if (car->msgBTime <= 0.0f) {
      car->msgBTime = 0.0f;
      car->msgB = 0;
    }
  }
}

/* WHAT IT DOES: Cast a ray straight down through the collision grid cell
 * under a point: of the upward-facing triangles it crosses, the nearest at
 * or below 1.5 above the point gives the ground height, normal, triangle and
 * the distance down to it, and each one hit adds its surface to the near
 * list (and, within 5, its triggers to the far list); failing any, the
 * nearest just above (within 1) stands in.  Returns how many were hit.
 * Ported from the PC twin (the collision ray); the u locals are declared
 * and unused (the ROM frame keeps their slots).
 * RESIDUE (387): the loop end is a spilled temp at 0x9C in the ROM (ours sits
 * elsewhere), which with FP colouring shifts most rows; frame, named slots,
 * structure and calls match. */
/* @t4-pass 0x8021F380 1 2026-09-29 compiles 13 best 387 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x8021F380 2 2026-09-29 compiles 13 best 387 moved 0  (n64/tools/n64permute.py) */
/* @t3 0x8021F380 */
/* @implements 0x8021F380 tgr BrGroundRay */
int BrGroundRay(BrVec3 *pPosOut, BrVec3 *pNormOut, BrVec3 *pEye, unsigned short *pNearIds, int *pGotHit,
                unsigned short *pFarIds, int *pFarCount, float *pDistOut, int *pFaceOut)
{
  int hitCount;
  int nearCount;
  int farCount;
  float dt;
  BrVec3 dir;
  float bestFarHitZ;
  float bestNearHitZ;
  float bestNearDist;
  float bestFarDist;
  BrVec3 tmpV;
  float t;
  int u0;
  int u1;
  BrVec3 hitPt;
  BrVec3 origin;
  float dist;
  int u2;
  unsigned short farFaceVal;
  unsigned short farFaceIdx;
  BrCollPlane *pP;
  BrCollPlane *pEnd;
  int ci;
  int cell;
  int u3;
  int u4;
  BrVec3 bestFarNorm;
  BrVec3 bestNearNorm;
  int u6[8];

  bestFarNorm = D_8028B318;
  bestNearNorm = D_8028B324;
  dt = D_80025C00.x3c - D_80025C00.x38;
  bestNearDist = dt * dt + 1.0f;
  farFaceVal = 0;
  farFaceIdx = 0;
  bestNearHitZ = pEye->z;
  bestFarHitZ = pEye->z;
  dir.x = 0.0f;
  dir.y = 0.0f;
  dir.z = 1.0f;
  origin.y = pEye->y;
  origin.x = pEye->x;
  origin.z = 1.0f;
  hitCount = 0;
  *pFaceOut = 0;
  bestFarDist = bestNearDist;
  nearCount = 0;
  farCount = 0;
  cell = func_8025F18C(origin.x, origin.y);
  pP = D_80379F80[cell];
  pEnd = pP + D_8037EA88[cell];
  for (; pP != pEnd; pP++) {
    if (pP->n.z < 0.0f) {
      continue;
    }
    dist = BrVec3Dot(&dir, &pP->n);
    if (dist == 0) {
      continue;
    }
    BrVec3Sub(&tmpV, pP->v0, &origin);
    t = BrVec3Dot(&tmpV, &pP->n) / dist;
    BrVec3MulAdd(&hitPt, &origin, &dir, t);
    if (!BrTriContainsPoint(&hitPt, pP->v0, pP->v1, pP->v2, &pP->n)) {
      continue;
    }
    dist = pEye->z + 1.5f - hitPt.z;
    if (dist >= 0.0f) {
      hitCount++;
      if (dist < bestNearDist) {
        bestNearDist = dist;
        *pFaceOut = pP->tri;
        bestNearHitZ = hitPt.z;
        if (pP->n.z < 0.0f) {
          BrVec3Negate(&bestNearNorm, &pP->n);
        } else {
          bestNearNorm.x = pP->n.x;
          bestNearNorm.y = pP->n.y;
          bestNearNorm.z = pP->n.z;
        }
        if (nearCount < 32) {
          pNearIds[nearCount++] = pNearIds[0];
        }
        pNearIds[0] = D_80025C00.tris[pP->tri].surface + 1;
        if (dist < 5.0f) {
          ci = D_80025C00.triTrigger[pP->tri];
          if (D_80025C00.triggers[ci] != 0) {
            do {
              if (farCount < 32) {
                pFarIds[farCount++] = pFarIds[0];
              }
              pFarIds[0] = D_80025C00.triggers[ci];
              ci++;
            } while (D_80025C00.triggers[ci] != 0);
          }
        }
      } else if (nearCount < 32) {
        pNearIds[nearCount++] = D_80025C00.tris[pP->tri].surface + 1;
        if (dist < 5.0f) {
          ci = D_80025C00.triTrigger[pP->tri];
          if (D_80025C00.triggers[ci] != 0) {
            do {
              if (farCount < 32) {
                pFarIds[farCount++] = D_80025C00.triggers[ci];
              }
              ci++;
            } while (D_80025C00.triggers[ci] != 0);
          }
        }
      }
    }
    dist -= 1.5f;
    if (dist <= 0.0f && dist < bestFarDist) {
      farFaceVal = D_80025C00.tris[pP->tri].surface + 1;
      if (-1.0f < dist) {
        bestFarDist = dist;
        farFaceIdx = pP->tri;
        bestFarHitZ = hitPt.z;
        if (pP->n.z < 0.0f) {
          BrVec3Negate(&bestFarNorm, &pP->n);
        } else {
          bestFarNorm.x = pP->n.x;
          bestFarNorm.y = pP->n.y;
          bestFarNorm.z = pP->n.z;
        }
      }
    }
  }
  if (hitCount != 0) {
    if (pPosOut != 0) {
      pPosOut->z = bestNearHitZ;
      *pDistOut = bestNearDist - 1.5f;
    }
    if (pNormOut != 0) {
      pNormOut->x = bestNearNorm.x;
      pNormOut->y = bestNearNorm.y;
      pNormOut->z = bestNearNorm.z;
    }
  } else {
    ci = D_80025C00.triTrigger[farFaceIdx];
    while (D_80025C00.triggers[ci] != 0) {
      if (farCount < 32) {
        pFarIds[farCount++] = D_80025C00.triggers[ci];
      } else {
        pFarIds[31] = D_80025C00.triggers[ci];
        break;
      }
      ci++;
    }
    if (nearCount < 32) {
      pNearIds[nearCount] = farFaceVal;
    } else {
      pNearIds[31] = farFaceVal;
    }
    if (pPosOut != 0) {
      pPosOut->z = bestFarHitZ;
    }
    if (pNormOut != 0) {
      pNormOut->x = bestFarNorm.x;
      pNormOut->y = bestFarNorm.y;
      pNormOut->z = bestFarNorm.z;
    }
  }
  *pGotHit = 1;
  *pFarCount = farCount;
  return hitCount;
}

/* WHAT IT DOES: Put the car in its 32-unit cell of the 64 by 64 track grid
 * (held to the grid's edges) and update the grid's record of it. */
/* @implements 0x8021F2D8 tgr BrCarGridCell */
void BrCarGridCell(BrCar *car)
{
  if (D_80025C70 != 0) {
    car->cellX = BrFloatToInt(car->mtx0[3][0] / 32);
    car->cellY = BrFloatToInt(car->mtx0[3][1] / 32);
    if (car->cellX < 0) {
      car->cellX = 0;
    } else if (car->cellX >= 0x40) {
      car->cellX = 0x3f;
    }
    if (car->cellY < 0) {
      car->cellY = 0;
    } else if (car->cellY >= 0x40) {
      car->cellY = 0x3f;
    }
    BrCarTrackLocate(car);
  }
}

/* WHAT IT DOES: A car entity's per-frame housekeeping (not in replay): show
 * a pending HUD arrow, run the clocks, messages, wrong-way check, 0x8022A0E0
 * and grid cell, take the velocity from this frame's movement, the heading
 * from the camera's first row, and count 0xED4 down. */
/* @implements 0x8022CF88 tgr BrCarEntTick */
void BrCarEntTick(BrCarEnt *e)
{
  if (D_8026FF10 == 0 && e->car != 0) {
    if (e->car->x344 != 0) {
      BrHudArrowDraw(e->car, &e->car->x334, e->car->x344);
      e->car->x344 = 0;
    }
    BrCarTickClocks(e->car);
    BrCarTickMessages(e->car);
    BrWrongWayCheck(e->car);
    func_8022A0E0(e);
    BrCarGridCell(e->car);
    BrVec3Sub(&e->car->velfd8, (BrVec3 *)e->car->mtx0[3], &e->car->posPrev);
    BrVec3ScaleBy(&e->car->velfd8, 1.0f / D_8028AAD8);
    e->car->heading = BrAtan2(e->car->cam->mtx[0][0], e->car->cam->mtx[0][1]);
    if (e->car->xed4 != 0) {
      e->car->xed4--;
    }
  }
}

/* WHAT IT DOES: One physics frame for a car.  Free-flying (the debug fly
 * mode), it moves along its heading at its set speed, stops, and turns at
 * its set rates; pad bit 0x10 levels it.  Otherwise the track contact
 * update runs, the wheels' slip terms are reset and set from the throttle
 * and brake demands (doubled on the front pair for the car kinds that drive
 * it, the rear pair braked only while it is nearly still), and the body is
 * stepped.  Then the draw matrices, the speed for the gauges, and the view's
 * camera step for a car that has a view.  u0/u1 are declared and unused
 * (the ROM frame keeps their slots).
 * RESIDUE (48): FP register colouring only -- the ROM gives the float zero
 * f0 and the frame time f2 throughout; ours swaps them. */
/* @t4-pass 0x8021F998 1 2026-09-29 compiles 12 best 48 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x8021F998 2 2026-09-29 compiles 12 best 48 moved 0  (n64/tools/n64permute.py) */
/* @t3 0x8021F998 */
/* @implements 0x8021F998 tgr BrCarPhysTick */
void BrCarPhysTick(BrCar *car)
{
  int u0[4];
  BrVec3 v;
  int u1[4];
  int i;

  if (car->xf4c != 0) {
    BrVec3Scale(&v, (BrVec3 *)car->mtx0[0], car->x1ddc * D_8028AAD8 * 100.0f);
    BrVec3AddTo((BrVec3 *)car->mtx0[3], &v);
    BrCarSetPos(car, car->mtx0[3][0], car->mtx0[3][1], car->mtx0[3][2]);
    BrCarSetVel(car, 0, 0, 0);
    BrCarSetAngVel(car, 0, 0, 0);
    car->xdf4 = 0.0f;
    BrCarRotate(car, car->x1dd4 * 3.1415927f * 0.4f * D_8028AAD8,
                car->x1dd8 * 3.1415927f * 0.4f * D_8028AAD8,
                car->x1de0 * 3.1415927f * 0.4f * D_8028AAD8);
    if (*car->pad & 0x10) {
      car->mtx0[2][0] = 0.0f;
      car->mtx0[2][1] = 0.0f;
      car->mtx0[2][2] = 1.0f;
      BrVec3Cross((BrVec3 *)car->mtx0[1], (BrVec3 *)car->mtx0[2], (BrVec3 *)car->mtx0[0]);
      BrVec3Cross((BrVec3 *)car->mtx0[2], (BrVec3 *)car->mtx0[0], (BrVec3 *)car->mtx0[1]);
      BrPadConsume(car->pad, 0x10);
    }
  } else {
    car->xfd4 = BrGroundRay((BrVec3 *)car->mtx0[3], (BrVec3 *)car->mtx0[2], (BrVec3 *)car->mtx0[3], car->x1fc0, &car->x2000,
                             (unsigned short *)car->x2004, &car->x2044, (float *)&car->x2048, &car->x204c);
    car->wheel[0]->x1c0 = car->wheel[1]->x1c0 = 0.0f;
    func_80222050(car);
    car->wheel[2]->x1c0 = car->wheel[3]->x1c0 = car->xdf0;
    car->wheel[0]->x1cc = 0.0f;
    car->wheel[1]->x1cc = 0.0f;
    car->wheel[2]->x1cc = 0.0f;
    car->wheel[3]->x1cc = 0.0f;
    car->wheel[0]->x1d0 = 0.0f;
    car->wheel[1]->x1d0 = 0.0f;
    car->wheel[2]->x1d0 = 0.0f;
    car->wheel[3]->x1d0 = 0.0f;
    if (car->xe38 > 0.0f) {
      if ((*car->pad & 0x20000) && car->xe40 == 1) {
        car->xe38 = -car->xe38;
      }
      if (car->xe28[1] != 0) {
        car->wheel[0]->x1cc = -car->xe38 * 2;
        car->wheel[1]->x1cc = -car->xe38 * 2;
        car->wheel[2]->x1cc = -car->xe38 * 0;
        car->wheel[3]->x1cc = -car->xe38 * 0;
      } else {
        car->wheel[0]->x1cc = -car->xe38;
        car->wheel[1]->x1cc = -car->xe38;
        car->wheel[2]->x1cc = -car->xe38;
        car->wheel[3]->x1cc = -car->xe38;
      }
    }
    if (car->xe3c < 0.0f) {
      car->wheel[0]->x1d0 = -car->xe3c;
      car->wheel[1]->x1d0 = -car->xe3c;
      car->wheel[0]->x1cc = 0.0f;
      car->wheel[1]->x1cc = 0.0f;
      if ((car->wheel[2]->x1cc < 0.0f ? -car->wheel[2]->x1cc : car->wheel[2]->x1cc) < 0.0001f) {
        car->wheel[2]->x1d0 = -car->xe3c;
        car->wheel[3]->x1d0 = -car->xe3c;
      }
    }
    func_80221940(car);
  }
  BrCarBuildMatrices(car);
  if (car->wheels[1].x13c != 0) {
    car->xfe4[0] = sqrtf(car->st.vel.z * car->st.vel.z + (car->st.vel.x * car->st.vel.x + car->st.vel.y * car->st.vel.y)) * 2.24f;
  }
  for (i = 0; i < D_8028AB0C; i++) {
    if (car->slot == D_8031B2C8[i].car) {
      func_80221170(car);
      return;
    }
  }
}
