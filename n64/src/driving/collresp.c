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
  char pad148[0x1a4 - 0x148];
  float n[3];                   /* 0x1A4  ground plane normal */
  float d;                      /* 0x1B0 */
  int f1B4;                     /* 0x1B4  contact count */
  char pad1b8[0x1dc - 0x1b8];
  float f1DC;                   /* 0x1DC */
  float f1E0;                   /* 0x1E0 */
  float f1E4;                   /* 0x1E4 */
  float f1E8;                   /* 0x1E8 */
} BrTipBody;
void BrQuatToMat(float m[4][4], void *st);
void BrMat3MulVecRows(float out[3], float m[4][4], float v[3]);
void BrRbVelAtPoint(float out[3], BrTipBody *b, float *pt);
void BrMat4RotateVecT(float out[3], float m[4][4], float v[3]);
void BrRbQuatDerivative(void *st);

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
