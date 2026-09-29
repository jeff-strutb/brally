/* collresp.c -- collision response: contact planes and the list they are kept on
 */
#include "tgr/common.h"

/* -- declarations -- */
typedef struct BrCrNode {       /* a contact-list node */
  void *plane;
  struct BrCrNode *next;
} BrCrNode;
extern BrCrNode *D_802A4A20;          /* the contact list */
extern BrCrNode *D_802A4A24;          /* next free node */
void osSyncPrintf(char *fmt, ...);
typedef struct BrTipBody {
  int x0;
  struct BrTipBody *child[4];   /* 0x004 */
  char pad14[0x78 - 0x14];
  float f78[3];                 /* 0x078  wheel point */
  char pad84[0xbc - 0x84];
  float m[4][4];                /* 0x0BC */
  char padfc[0x114 - 0xfc];
  float state[0x28 / 4];        /* 0x114  saved rigid-body state */
  float angVel[3];              /* 0x13C */
  char pad148[0x158 - 0x148];
  float cur[0x44 / 4];          /* 0x158  the state being stepped */
  char pad19c[0x1a4 - 0x19c];
  float n[3];                   /* 0x1A4  ground plane normal */
  float d;                      /* 0x1B0 */
  int f1B4;                     /* 0x1B4  contact count */
  char pad1b8[0x1dc - 0x1b8];
  float f1DC;                   /* 0x1DC */
  float f1E0;                   /* 0x1E0 */
  float f1E4;                   /* 0x1E4 */
  float f1E8;                   /* 0x1E8 */
  char pad1ec[0x1f8 - 0x1ec];
  int stuck;                    /* 0x1F8  frames left before it counts as stuck */
} BrTipBody;
void BrQuatToMat(float m[4][4], void *st);
void BrMat3MulVecRows(float out[3], float m[4][4], float v[3]);
void BrRbVelAtPoint(float out[3], BrTipBody *b, float *pt);
void BrMat4RotateVecT(float out[3], float m[4][4], float v[3]);
void BrRbQuatDerivative(void *st);

extern int D_802A4A28;
extern void *D_802A4A2C;
extern float D_8037EAA8[4];           /* the shared contact plane: normal and w */
extern float D_8037EAC8[3];           /* the push-out it gives */
extern BrCrNode D_80379940[];         /* the contact-list nodes */
void BrPerfMark(int a, int r, int g, int b, int al);
void BrMat4InvertScaled(float m[4][4], float out[4][4], float s[3]);
int BrCollRespBroadPhase(BrTipBody *b, float m[4][4]);
void func_8025FDE4(void);
void BrRbStateStep(void *out, void *in, float dt);
int func_8025DFCC(BrTipBody *b, float m[4][4]);
void *memcpy(void *d, const void *s, unsigned int n);
int BrCollRespTipKick(BrTipBody *b);

typedef struct BrCrPlane {      /* a collision triangle's plane (0x20 bytes) */
  float n[3];
  float d;
  float *v0;                    /* 0x10  its corners */
  float *v1;
  float *v2;
  int x1c;
} BrCrPlane;
extern BrCrPlane D_80379F80[][150];   /* each grid cell's collision planes */
extern unsigned short D_8037EA88[];   /* and how many */
extern int D_802A4A30;                /* walk the cell backwards (flips every frame) */
int func_8025F18C(float x, float y);
int BrTriCubeTest(float *tri, float *norm);
void BrCrListPush(void *plane);

#define ABS(x) ((x) < 0.0f ? -(x) : (x))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define SIGN(x) ((x) == 0.0f ? 0.0 : ((x) > 0 ? 1.0 : -1.0))
/* -- end declarations -- */

/* WHAT IT DOES: Signed distance of a point from a plane: n . p + d. */
/* @implements 0x8025B704 tgr BrCrPlaneDist */
float BrCrPlaneDist(float n[3], float d, float p[3])
{
  return n[0] * p[0] + n[1] * p[1] + n[2] * p[2] + d;
}

/* WHAT IT DOES: Push a contact plane on the front of the contact list,
 * taking the next node from the pool.  The PC twin (br_collresp.c) is the
 * same source. */
/* @implements 0x8025C24C tgr BrCrListPush */
void BrCrListPush(void *plane)
{
  BrCrNode *n = D_802A4A24++;

  n->plane = plane;
  n->next = D_802A4A20;
  D_802A4A20 = n;
}

/* WHAT IT DOES: Debug print of how many contacts are on the list (the
 * print itself is compiled out in the retail osSyncPrintf). */
/* @implements 0x8025C27C tgr BrCrListPrintCount */
void BrCrListPrintCount(void)
{
  BrCrNode *n;
  short count;

  n = D_802A4A20;
  if (n != 0) {
    count = 0;
    for (; n != 0; n = n->next) {
      count++;
    }
    osSyncPrintf("----%d-------------------------------\n", count);
  }
}

/* WHAT IT DOES: The broad phase of car-versus-track collision: take the
 * grid cell under the body, put each of its triangles into the body's box
 * space and keep, on this frame's contact list, each one that touches the
 * box; returns how many.  The cell is walked backwards on alternate
 * frames.  The PC twin is BrCollRespBroadPhase; its two arms are one
 * macro, but here each arm is written out line by line (a one-line macro
 * expansion schedules the edge loads differently). */
/* @implements 0x8025D060 tgr BrCollRespBroadPhase */
int BrCollRespBroadPhase(BrTipBody *b, float m[4][4])
{
  float v[9];
  float nrm[3];
  float e1[3];
  float e2[3];
  BrCrPlane *pP;
  int cell;
  int count;
  int i;
  int n;

  n = 0;
  cell = func_8025F18C(b->m[3][0], b->m[3][1]);
  count = D_8037EA88[cell];
  if (D_802A4A30 != 0) {
    pP = &D_80379F80[cell][count - 1];
    for (i = count - 1; i >= 0; i--, pP--) {
      BrMat3MulVecRows(&v[0], m, pP->v0);
      BrMat3MulVecRows(&v[3], m, pP->v1);
      BrMat3MulVecRows(&v[6], m, pP->v2);
      e1[0] = v[3] - v[0];
      e1[1] = v[4] - v[1];
      e1[2] = v[5] - v[2];
      e2[0] = v[6] - v[0];
      e2[1] = v[7] - v[1];
      e2[2] = v[8] - v[2];
      nrm[0] = e1[1] * e2[2] - e1[2] * e2[1];
      nrm[1] = e1[2] * e2[0] - e1[0] * e2[2];
      nrm[2] = e1[0] * e2[1] - e1[1] * e2[0];
      if (BrTriCubeTest(v, nrm) != 0) {
        BrCrListPush(pP);
        n++;
      }
    }
  } else {
    pP = D_80379F80[cell];
    for (i = 0; i < count; i++, pP++) {
      BrMat3MulVecRows(&v[0], m, pP->v0);
      BrMat3MulVecRows(&v[3], m, pP->v1);
      BrMat3MulVecRows(&v[6], m, pP->v2);
      e1[0] = v[3] - v[0];
      e1[1] = v[4] - v[1];
      e1[2] = v[5] - v[2];
      e2[0] = v[6] - v[0];
      e2[1] = v[7] - v[1];
      e2[2] = v[8] - v[2];
      nrm[0] = e1[1] * e2[2] - e1[2] * e2[1];
      nrm[1] = e1[2] * e2[0] - e1[0] * e2[2];
      nrm[2] = e1[0] * e2[1] - e1[1] * e2[0];
      if (BrTriCubeTest(v, nrm) != 0) {
        BrCrListPush(pP);
        n++;
      }
    }
  }
  return n;
}

/* WHAT IT DOES: The PC's BrCollRespTipKick: rebuild the body matrix from its
 * saved state, then for each of the four wheels with a ground contact place
 * the body's box corner on that wheel's side (half extents signed by the
 * wheel's point, z lowered by half the z extent), transform it and keep the
 * smallest distance from that wheel's ground plane, remembering the last
 * contacting wheel.  With one or two wheels down, the corner within 0.6 of
 * the ground and its velocity along the last wheel's normal under 0.1, kick
 * the angular velocity by twice a +-0.1 pitch vector (sign from the chassis
 * plane) and refresh the quaternion derivative; returns 1 when it kicked.
 * The three "-- Stand" traces print on the way.  One float, t, carries the
 * wheel distance and then the two dot products (its frame slot shows it). */
/* @implements 0x8025D368 tgr BrCollRespTipKick */
int BrCollRespTipKick(BrTipBody *b)
{
  int count;
  BrTipBody *pW;
  float *pN;                    /* unused: holds its frame slot */
  float p[3];
  float w[3];
  float t;
  float best;

  BrQuatToMat(b->m, b->state);
  best = 100.0f;
  count = 0;
  if (b->child[0]->f1B4 != 0) {
    count++;
    pW = b->child[0];
    p[0] = SIGN(b->child[0]->f78[0]) * (b->f1DC * 0.5f);
    p[1] = SIGN(pW->f78[1]) * (b->f1E0 * 0.5f);
    p[2] = -b->f1E4 * 0.5f + b->f1E8;
    BrMat3MulVecRows(w, b->m, p);
    t = ABS(BrCrPlaneDist(pW->n, pW->d, w));
    best = MIN(best, t);
  }
  if (b->child[1]->f1B4 != 0) {
    count++;
    pW = b->child[1];
    p[0] = SIGN(b->child[1]->f78[0]) * (b->f1DC * 0.5f);
    p[1] = SIGN(pW->f78[1]) * (b->f1E0 * 0.5f);
    p[2] = -b->f1E4 * 0.5f + b->f1E8;
    BrMat3MulVecRows(w, b->m, p);
    t = ABS(BrCrPlaneDist(pW->n, pW->d, w));
    best = MIN(best, t);
  }
  if (b->child[2]->f1B4 != 0) {
    count++;
    pW = b->child[2];
    p[0] = SIGN(b->child[2]->f78[0]) * (b->f1DC * 0.5f);
    p[1] = SIGN(pW->f78[1]) * (b->f1E0 * 0.5f);
    p[2] = -b->f1E4 * 0.5f + b->f1E8;
    BrMat3MulVecRows(w, b->m, p);
    t = ABS(BrCrPlaneDist(pW->n, pW->d, w));
    best = MIN(best, t);
  }
  if (b->child[3]->f1B4 != 0) {
    count++;
    pW = b->child[3];
    p[0] = SIGN(b->child[3]->f78[0]) * (b->f1DC * 0.5f);
    p[1] = SIGN(pW->f78[1]) * (b->f1E0 * 0.5f);
    p[2] = -b->f1E4 * 0.5f + b->f1E8;
    BrMat3MulVecRows(w, b->m, p);
    t = ABS(BrCrPlaneDist(pW->n, pW->d, w));
    best = MIN(best, t);
  }
  if (count > 2 || count < 1)
    return 0;
  osSyncPrintf("-- Stand Dist = %f\n", best);
  if (best > 0.6)
    return 0;
  BrRbVelAtPoint(w, b, p);
  t = w[0] * pW->n[0] + w[1] * pW->n[1] + w[2] * pW->n[2];
  osSyncPrintf("-- Stand Vel = %f\n", t);
  if (ABS(t) > 0.1f)
    return 0;
  t = b->m[0][0] * b->n[0] + b->m[0][1] * b->n[1] + b->m[0][2] * b->n[2];
  if (t > 0.0f) {
    p[1] = 0.1f;
    p[0] = 0.0f;
    p[2] = 0.0f;
  } else {
    p[0] = 0.0f;
    p[1] = -0.1f;
    p[2] = 0.0f;
  }
  osSyncPrintf("-- Stand Point = %f\n", t);
  BrMat4RotateVecT(w, b->m, p);
  b->angVel[0] = b->angVel[0] + w[0];
  b->angVel[1] = b->angVel[1] + w[1];
  b->angVel[2] = b->angVel[2] + w[2];
  b->angVel[0] = b->angVel[0] + w[0];
  b->angVel[1] = b->angVel[1] + w[1];
  b->angVel[2] = b->angVel[2] + w[2];
  BrRbQuatDerivative(b->state);
  return 1;
}


/* WHAT IT DOES: Turn one candidate contact into a push-out: the contact
 * normal scaled by how far the point lies from the plane -- with the plane
 * given (mode other than 2) or, for the box-face case, after picking the
 * box face the triangle's centroid lies most flush against (sign from the
 * centroid's x) and scaling the face by the body's extents.  The PC twin is
 * BrCrPlaneResolve. */
/* @implements 0x8025DCB8 tgr BrCrPlaneResolve */
void BrCrPlaneResolve(BrTipBody *b, float *pA, float planeD, float *pEdgeN, float *v)
{
  int pad0;                     /* pad0, u: declared, never used; */
  float c[3];
  float s;
  int sgn;
  int k;
  float u[3];                   /* the frame holds them */

  if (D_802A4A28 != 2) {
    s = (pA[0] * pEdgeN[0] + pA[1] * pEdgeN[1] + pA[2] * pEdgeN[2]) - planeD;
    D_8037EAC8[0] = pA[0] * -s;
    D_8037EAC8[1] = pA[1] * -s;
    D_8037EAC8[2] = pA[2] * -s;
  } else {
    for (k = 0; k < 3; k++) {
      c[k] = v[k] + v[k + 3] + v[k + 6];
      c[k] /= 3.0f;
    }
    if ((c[0] < 0.0f ? -c[0] : c[0]) < (c[1] < 0.0f ? -c[1] : c[1])) {
      if ((c[0] < 0.0f ? -c[0] : c[0]) < (c[2] < 0.0f ? -c[2] : c[2])) {
        D_8037EAA8[1] = D_8037EAA8[2] = 0.0f;
        if (c[0] < 0) {
          sgn = -1;
        } else {
          sgn = 1;
        }
        D_8037EAA8[0] = sgn * 0.5f;
        goto face;
      }
    } else {
      if ((c[1] < 0.0f ? -c[1] : c[1]) < (c[2] < 0.0f ? -c[2] : c[2])) {
        D_8037EAA8[0] = D_8037EAA8[2] = 0.0f;
        if (c[0] < 0) {
          sgn = -1;
        } else {
          sgn = 1;
        }
        D_8037EAA8[1] = sgn * 0.5f;
        goto face;
      }
    }
    D_8037EAA8[0] = D_8037EAA8[1] = 0.0f;
    if (!(c[0] < 0)) {
      sgn = 1;
    } else {
      sgn = -1;
    }
    D_8037EAA8[2] = sgn * 0.5f;
  face:
    s = (pA[0] * D_8037EAA8[0] + pA[1] * D_8037EAA8[1] + pA[2] * D_8037EAA8[2]) - planeD;
    D_8037EAA8[0] = D_8037EAA8[0] * b->f1DC;
    D_8037EAA8[0] = D_8037EAA8[0] * b->f1E0;
    D_8037EAA8[3] = D_8037EAA8[3] * b->f1E4;
    D_8037EAC8[0] = pA[0] * -s;
    D_8037EAC8[1] = pA[1] * -s;
    D_8037EAC8[2] = pA[2] * -s;
  }
}

/* WHAT IT DOES: One frame of a car body's position physics in 1/120 s
 * substeps: clear the contact list and the shared plane, gather the nearby
 * track once, then (kicking a car standing on its nose upright) collide
 * car against car, step the state, rebuild the matrix, hand the unit-box
 * view to the collision response and keep the result, until 1/30 s is
 * used; then count the stuck timer down while the body is not upright.
 * The PC twin is BrCarPhysAdvance.  The substep is t / 4 (IDO propagates
 * t's literal into it but does not fold the division). */
/* @implements 0x8025E55C tgr BrCarPhysAdvance */
void BrCarPhysAdvance(BrTipBody *b)
{
  float m[4][4];
  float s[3];
  float t;
  float dt;
  float spare[4];               /* declared, never used: the frame holds it */

  D_802A4A20 = 0;
  D_802A4A24 = D_80379940;
  D_8037EAA8[0] = 0.0f;
  D_8037EAA8[1] = 0.0f;
  D_8037EAA8[2] = 0.0f;
  D_8037EAC8[0] = 0.0f;
  D_8037EAC8[1] = 0.0f;
  D_8037EAC8[2] = 0.0f;
  D_802A4A28 = 0;
  D_802A4A2C = 0;
  BrPerfMark(0, 0x80, 0x80, 0x80, 0xff);
  s[1] = 0.1f;
  s[0] = 0.1f;
  s[2] = 0.1f;
  BrMat4InvertScaled(b->m, m, s);
  BrCollRespBroadPhase(b, m);
  BrPerfMark(0, 0x80, 0x80, 0, 0xff);
  s[0] = 1.0f / b->f1DC;
  t = 0.033333335f;
  s[1] = 1.0f / b->f1E0;
  s[2] = 1.0f / b->f1E4;
  dt = t / 4;
  while (t > 0.002f) {
    if (BrCollRespTipKick(b) != 0) {
      osSyncPrintf("Standing on it's F'in Nose damnit\n");
    }
    func_8025FDE4();
    BrRbStateStep(b->cur, b->state, dt);
    BrQuatToMat(b->m, b->cur);
    BrMat4InvertScaled(b->m, m, s);
    m[3][2] -= b->f1E8;
    if (func_8025DFCC(b, m) != 0) {
      BrRbQuatDerivative(b->cur);
      BrQuatToMat(b->m, b->cur);
    }
    memcpy(b->state, b->cur, 0x44);
    t -= dt;
  }
  BrQuatToMat(b->m, b->cur);
  BrPerfMark(0, 0x80, 0x80, 0, 0xff);
  memcpy(b->state, b->cur, 0x44);
  if (b->m[2][2] < 0.5f) {
    if (b->stuck < 0) {
      b->stuck = -1;
    }
    b->stuck--;
  } else {
    b->stuck = 35;
  }
}
