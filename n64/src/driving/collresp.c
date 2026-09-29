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
  char pad1fc[0x200 - 0x1fc];
  unsigned char idle;           /* 0x200  frames with no contact (to 40) */
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
int BrCrRespWalk(BrTipBody *b, float m[4][4]);
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
short BrCollGridCellAcquire(float x, float y);
int BrTriCubeTest(float *tri, float *norm);
void BrCrListPush(void *plane);

extern int D_8026FF18;                /* the game mode; 4 arms the contact-kick path */
void BrVec3NormaliseF(float v[3]);
int func_8025B73C(BrTipBody *b, void *plane, int flag, int spin);
int func_8025BBB8(BrTipBody *b, float *n, void *plane, int flag, float rest);
void BrCrPlaneResolve(BrTipBody *b, float *pA, float planeD, float *pEdgeN, float *v);

extern unsigned short D_8037EA80[4];   /* the grid cache: each slot's cell */
extern unsigned int D_8037EA90[4];     /* when each slot was last used */
extern unsigned int D_8037EAA0;        /* the use clock */
typedef struct BrCrVtx { float x, y, z; } BrCrVtx;
typedef struct BrTrackGeom {    /* the loaded track's header at 0x80025C00 */
  char pad00[0xc];
  unsigned short *tris;         /* 0x0C  4 vertex indices per triangle */
  int x10;
  BrCrVtx *verts;               /* 0x14 */
  char pad18[0x94 - 0x18];
  unsigned char *surf;          /* 0x94  per triangle: surface bits */
} BrTrackGeom;
extern BrTrackGeom D_80025C00;
unsigned int BrGridCellRangeAt(float x, float y);
unsigned short BrU16QueuePop(unsigned short *q);

#define ABS(x) ((x) < 0.0f ? -(x) : (x))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define SIGN(x) ((x) == 0.0f ? 0.0 : ((x) > 0 ? 1.0 : -1.0))
/* -- end declarations -- */

/* WHAT IT DOES: Whether a point lies inside a collision triangle: flatten
 * both onto the two axes the triangle's normal is least aligned with, put
 * the point in edge coordinates u and v, and it is inside when u and v are
 * not negative and u, and u + v, stay within 1.  The PC twin is
 * FUN_100656F0 (br_tritest.c).  The modulus is a variable 3 (the ROM keeps
 * the divide's zero check).
 * RESIDUE (41): register naming only -- |n1| and n1 swap f12/f14, the axis
 * indices sit in v1/a3 where ours use t1/t2, and u and the vertex copies
 * trade spill slots. */
/* @implements 0x8025B3B0 tgr BrCrTriContainsPoint */
short BrCrTriContainsPoint(BrCrPlane *pT, float *pP)
{
  float a0;
  float a1;
  int c;
  int i1;
  int i2;
  float d1;
  float b1;
  float d2;
  float b2;
  float c1;
  float c2;
  int n;
  float v;
  int r;
  int x0;                       /* declared, never used: the frame holds it */
  float u;
  float ay;
  float ax;

  n = 3;
  a0 = pT->n[0] < 0.0f ? -pT->n[0] : pT->n[0];
  a1 = pT->n[1] < 0.0f ? -pT->n[1] : pT->n[1];
  if (a1 < a0) {
    a0 = pT->n[0] < 0.0f ? -pT->n[0] : pT->n[0];
    a1 = pT->n[2] < 0.0f ? -pT->n[2] : pT->n[2];
    if (a1 < a0) {
      c = 0;
    } else {
      c = 2;
    }
  } else {
    a0 = pT->n[1] < 0.0f ? -pT->n[1] : pT->n[1];
    a1 = pT->n[2] < 0.0f ? -pT->n[2] : pT->n[2];
    if (a1 < a0) {
      c = 1;
    } else {
      c = 2;
    }
  }
  i1 = (c + 1) % n;
  i2 = (c + 2) % n;
  ax = pT->v0[i1];
  d1 = pP[i1] - ax;
  ay = pT->v0[i2];
  d2 = pP[i2] - ay;
  b1 = pT->v1[i1] - ax;
  b2 = pT->v1[i2] - ay;
  c1 = pT->v2[i1] - ax;
  c2 = pT->v2[i2] - ay;
  r = 0;
  if (b1 == 0.0f) {
    u = d1 / c1;
    if (u >= 0.0f && u <= 1.0f) {
      v = (d2 - u * c2) / b2;
      r = v >= 0.0f && v + u <= 1.0f;
    }
  } else {
    u = (d2 * b1 - d1 * b2) / (c2 * b1 - c1 * b2);
    if (u >= 0.0f && u <= 1.0f) {
      v = (d1 - u * c1) / b1;
      r = v >= 0.0f && v + u <= 1.0f;
    }
  }
  return r;
}

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
  cell = BrCollGridCellAcquire(b->m[3][0], b->m[3][1]);
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
/* @t3 0x8025DCB8 */
/* @t4-pass 0x8025DCB8 1 2026-09-29 compiles 41 best 4 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x8025DCB8 2 2026-09-29 compiles 40 best 4 moved 0  (n64/tools/n64permute.py) */
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

/* WHAT IT DOES: The collision response: walk this frame's contact list,
 * and for each triangle that still touches the body's box build its plane
 * (normal, distance, the box corner it drives toward), resolve it and apply
 * the impulse -- or, in game mode 4, the contact kick for a triangle edge
 * against a box face -- and on a response push the body back out along the
 * triangle's normal and restore its orientation.  Counts frames with no
 * contact in body+0x200 (to 40).  Returns 1 if any contact responded.  The
 * PC twin is BrCrRespWalk.
 * RESIDUE (~300): register priority -- the ROM keeps the addresses of all
 * three plane globals in saved registers and homes m in its argument slot;
 * ours keeps m and loads D_802A4A2C by address each time, which moves every
 * saved register and temp after it.  Frame, slots and control flow match. */
/* @implements 0x8025DFCC tgr BrCrRespWalk */
int BrCrRespWalk(BrTipBody *b, float m[4][4])
{
  BrCrNode *node;
  float v[9];
  float nrm[3];
  float e1[3];
  float e2[3];
  BrCrPlane *pP;
  float planeD;
  int ret;
  short cnt;
  float sign[3];
  int flag;
  int spin;
  int sgn;
  int r;
  float d;
  int unused;                   /* unused, pad: declared, never used; */
  float dp[3];
  int pad[2];                   /* the frame holds them */

  ret = 0;
  cnt = 0;
  for (node = D_802A4A20; node != 0; node = node->next) {
    pP = node->plane;
    BrMat3MulVecRows(&v[0], m, pP->v0);
    BrMat3MulVecRows(&v[3], m, pP->v1);
    BrMat3MulVecRows(&v[6], m, pP->v2);
    e1[0] = v[3] - v[0];
    e1[1] = v[4] - v[1];
    e1[2] = v[5] - v[2];
    e2[0] = v[6] - v[0];
    e2[1] = v[7] - v[1];
    e2[2] = v[8] - v[2];
    nrm[0] = e1[1] * e2[2] - e2[1] * e1[2];
    nrm[1] = e1[2] * e2[0] - e2[2] * e1[0];
    nrm[2] = e1[0] * e2[1] - e2[0] * e1[1];
    if (BrTriCubeTest(v, nrm) != 0) {
      BrVec3NormaliseF(nrm);
      flag = 1;
      D_802A4A28 = 0;
      cnt++;
      planeD = v[2] * nrm[2] + nrm[0] * v[0] + nrm[1] * v[1];
      if (D_8026FF18 == 4) {
        if ((nrm[0] < 0.0f ? -nrm[0] : nrm[0]) <= 0.999f
            && (nrm[1] < 0.0f ? -nrm[1] : nrm[1]) <= 0.999f
            && (nrm[2] < 0.0f ? -nrm[2] : nrm[2]) <= 0.999f) {
          if ((planeD < 0.0f ? -planeD : planeD) < 0.5f) {
            osSyncPrintf("Triangle Edge to CubeFace\n");
            spin = 0;
            D_802A4A28 = 2;
          }
        } else {
          D_802A4A28 = 1;
          osSyncPrintf("Wank CT1 case\n");
          spin = 1;
        }
      }
      osSyncPrintf("Cube Edge to Triangle Face\n");
      e1[0] = nrm[0] * planeD;
      e1[1] = nrm[1] * planeD;
      e1[2] = nrm[2] * planeD;
      if (e1[0] < 0) {
        sgn = -1;
      } else {
        sgn = 1;
      }
      sign[0] = sgn * 0.5f;
      if (e1[1] < 0) {
        sgn = -1;
      } else {
        sgn = 1;
      }
      sign[1] = sgn * 0.5f;
      if (e1[2] < 0) {
        sgn = -1;
      } else {
        sgn = 1;
      }
      sign[2] = sgn * 0.5f;
      D_8037EAA8[0] = sign[0] * b->f1DC;
      D_8037EAA8[1] = sign[1] * b->f1E0;
      D_8037EAA8[2] = sign[2] * b->f1E4 + b->f1E8;
      D_802A4A2C = pP;
      BrCrPlaneResolve(b, nrm, planeD, sign, v);
      if (b->m[2][2] > 0.5f) {
        flag = 0;
      }
      if (flag) {
        osSyncPrintf("Resistive collision %10.3f\n", b->m[2][2]);
      }
      if (D_802A4A28 != 1) {
        r = func_8025BBB8(b, D_8037EAA8, D_802A4A2C, flag, 0.0f);
      } else {
        r = func_8025B73C(b, D_802A4A2C, flag, spin);
      }
      if (r != 0) {
        ret = 1;
        dp[0] = b->cur[0] - b->state[0];
        dp[1] = b->cur[1] - b->state[1];
        dp[2] = b->cur[2] - b->state[2];
        d = (pP->n[2] * dp[2] + dp[0] * pP->n[0] + dp[1] * pP->n[1]) * 1.1;
        dp[0] = pP->n[0] * d;
        dp[1] = pP->n[1] * d;
        dp[2] = pP->n[2] * d;
        b->cur[0] = b->cur[0] - dp[0];
        b->cur[1] = b->cur[1] - dp[1];
        b->cur[2] = b->cur[2] - dp[2];
        b->cur[6] = b->state[6];
        b->cur[7] = b->state[7];
        b->cur[8] = b->state[8];
        BrRbQuatDerivative(b->cur);
        BrQuatToMat(b->m, b->cur);
      }
    }
  }
  if (cnt == 0) {
    if (b->idle < 40) {
      b->idle++;
    }
  } else {
    b->idle = 0;
  }
  return ret;
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
    if (BrCrRespWalk(b, m) != 0) {
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


/* WHAT IT DOES: The collision-grid cache slot for the 32-unit square under a
 * point, reusing the least recently used of the four slots when the square
 * is not already loaded; a freshly loaded slot gets one plane record per
 * triangle of that square: its three vertex pointers, index and surface
 * bits, unit normal (v1 - v0) x (v2 - v0) and plane constant.  The PC twin
 * is BrCollGridCellAcquire.
 * RESIDUE (211): ours hoists the vertex-index scale (12) into a saved
 * register and multiplies; the ROM shifts ((i << 2) - i) << 2 in place, so
 * every saved register after it moves.  Index types and byte/float/struct
 * pointer spellings all hoist. */
/* @implements 0x8025F18C tgr BrCollGridCellAcquire */
short BrCollGridCellAcquire(float x, float y)
{
  int spare;                    /* spare, spare2: declared, never used; */
  int victim;
  unsigned int best;
  unsigned int packed;
  int spare2;                   /* the frame holds them */
  unsigned short cur[2];
  unsigned short tri;
  unsigned short n;
  float a[3];
  float b[3];
  float *p;
  short key;
  int i;

  D_8037EAA0++;
  key = ((int)y / 32 << 6) + (int)x / 32;
  best = 0x40000000;
  for (i = 0; i < 4; i++) {
    if (key == D_8037EA80[i]) {
      D_8037EA90[i] = D_8037EAA0;
      return i;
    }
    if (D_8037EA90[i] < best) {
      victim = i;
      best = D_8037EA90[i];
    }
  }
  D_8037EA90[victim] = D_8037EAA0;
  D_8037EA80[victim] = key;
  n = 0;
  p = D_80379F80[victim][0].n;
  packed = BrGridCellRangeAt(x, y);
  cur[0] = packed;
  cur[1] = packed >> 16;
  if (packed != 0) {
    while ((tri = BrU16QueuePop(cur)) != 0) {
      ((BrCrVtx **)p)[4] = &D_80025C00.verts[D_80025C00.tris[tri * 4]];
      ((BrCrVtx **)p)[5] = &D_80025C00.verts[D_80025C00.tris[tri * 4 + 1]];
      ((BrCrVtx **)p)[6] = &D_80025C00.verts[D_80025C00.tris[tri * 4 + 2]];
      *(unsigned short *)(p + 7) = tri;
      ((unsigned char *)p)[0x1e] = D_80025C00.surf[tri] & 7;
      a[0] = ((float **)p)[5][0] - ((float **)p)[4][0];
      a[1] = ((float **)p)[5][1] - ((float **)p)[4][1];
      a[2] = ((float **)p)[5][2] - ((float **)p)[4][2];
      b[0] = ((float **)p)[6][0] - ((float **)p)[4][0];
      b[1] = ((float **)p)[6][1] - ((float **)p)[4][1];
      b[2] = ((float **)p)[6][2] - ((float **)p)[4][2];
      p[0] = a[1] * b[2] - b[1] * a[2];
      p[1] = a[2] * b[0] - b[2] * a[0];
      p[2] = a[0] * b[1] - b[0] * a[1];
      BrVec3NormaliseF(p);
      n++;
      p[3] = -(p[0] * ((float **)p)[4][0] + p[1] * ((float **)p)[4][1] + ((float **)p)[4][2] * p[2]);
      p += 8;
    }
  }
  D_8037EA88[victim] = n;
  return victim;
}
