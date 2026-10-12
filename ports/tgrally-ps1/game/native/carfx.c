/* carfx.c: the cars' rigid-body physics for the PlayStation, in fixed point.
 *
 * The game's own physics (driving/carphys.c, rbforce.c, rbquat.c, tyre.c,
 * collresp.c, obb.c) is single-precision float throughout, and the R3000 has
 * no FPU: every multiply or add is a call into the soft-float routines (50 to
 * 70 cycles each).  This is the same model worked in 32-bit integers with
 * 64-bit products: the same forces, the same order of steps, the same
 * collision response, its numbers no longer the N64's to the bit.
 *
 * Each car's physical state lives here (FxCar) and is mirrored into the
 * car's record, where the rest of the game reads (and now and then writes)
 * it as floats: before a car is stepped every mirrored field whose float
 * differs from what was last written there is read back (the game moved the
 * car, or reset it), and after the step every field that changed is written
 * out.  Contacts between the car's body and the track, and between cars,
 * are rare: when one happens, the response is worked by the game's own float
 * routines on the mirrored record and read back.
 *
 * Scales: positions and velocities 1/65536 of a unit (Q16), angular
 * velocities 1/65536 of a radian a second, quaternions and rotation matrix
 * entries 1/2^30 (Q30), forces, torques and loads 1/256 (Q8), cube space
 * 1/4096 of a cube (Q12).
 */
#include <stdlib.h>
#include "tgr/common.h"
#include "tgr/car.h"
#include "tgr/track.h"

typedef long long i64;
void tgr_trace(const char *kind, const char *fmt, ...);
typedef unsigned long long u64;

/* ---- the game's data and routines this works with ---------------------- */
extern int D_8028C800;                  /* the weather (difficulty row + 1) */
extern int D_8026FF18;                  /* the game mode */
extern int D_802A4A30;                  /* the broad phase walks its cell backwards (flips every frame) */
extern float D_802A4A38[24];            /* grip by weather row and surface */
extern float D_802A4A98[24];            /* the lateral speed above which grip falls */
extern float D_802A4AF8[24];            /* and below which it is full */
extern float D_8037EAA8[4];             /* the shared contact point / normal */
extern float D_8037EAC8[3];             /* the push-out it gives */
extern int D_802A4A28;                  /* the contact's kind */
extern void *D_802A4A2C;                /* the contact's plane */
typedef struct FxWeatherObj {           /* 0x78 bytes */
  char pad00[0x60];
  TgrAddr car;                          /* BrCar * -- 0x60 */
  char pad64[0x78 - 0x64];
} FxWeatherObj;
extern FxWeatherObj D_803239A0[];       /* the cars in the race */
extern int D_8028B7F0;                  /* and how many */
unsigned int BrGridCellRangeAt(float x, float y);
unsigned short BrU16QueuePop(unsigned short *q);
void BrCrPlaneResolve(void *b, float *pA, float planeD, float *pEdgeN, float *v);
int BrCrImpulseSolve(void *b, float *pN, float *pDir, int flag, float rest);
int BrCrContactKick(void *b, float *pN, int dampFlag, int spinFlag);
void BrVec3NormaliseF(float v[3]);
void BrQuatToMat(void *m, void *st);
void BrRbQuatDerivative(void *st);
int BrObbOverlap(float *m, float *t, float *a, float *b);
int BrTriCubeTest(float *tri, float *norm);
void BrMat3FromMat4T(float out[3][3], float m[4][4]);
void BrMat3FromMat4(float out[3][3], float m[4][4]);
void BrMat3Mul(float out[3][3], float a[3][3], float b[3][3]);
void BrMat3MulVec(float out[3], float m[3][3], float v[3]);
void BrMat4RotateVec(float out[3], float m[4][4], float v[3]);

/* ---- fixed point ------------------------------------------------------- */
#define QK(x, q) ((int)((x) * (double)(1LL << (q)) + ((x) >= 0 ? 0.5 : -0.5)))
#define Q16(x) QK(x, 16)
#define Q30(x) QK(x, 30)
#define Q8(x) QK(x, 8)
#define Q12(x) QK(x, 12)
#define MUL(a, b, s) ((int)(((i64)(a) * (i64)(b)) >> (s)))
#define ABS(x) ((x) < 0 ? -(x) : (x))
#define SGN(x) ((x) > 0 ? 1 : (x) < 0 ? -1 : 0)
#define DT_Q30 Q30(0.033333335)         /* the frame: 1/30 s */
#define DT4_Q30 Q30(0.008333334)        /* a substep: 1/120 s */
#define FX_RAW 100                      /* a mirrored int, copied */
#define FX_BYTE 101                     /* a mirrored byte */

/* a float's bits to a fixed-point value of scale 2^q, rounded to nearest
 * (zero for denormals, the limits for what does not fit) */
static int f2q(unsigned int u, int q)
{
  int e = (int)((u >> 23) & 0xFF), sh;
  unsigned int m, v;
  if (e == 0)
    return 0;
  m = (u & 0x7FFFFF) | 0x800000;
  sh = e - 150 + q;
  if (sh >= 0)
    v = sh > 7 ? 0x7FFFFFFFu : m << sh;
  else if (sh > -25)
    v = (m + (1u << (-sh - 1))) >> -sh;
  else
    v = 0;
  return (u >> 31) ? -(int)v : (int)v;
}

/* a fixed-point value of scale 2^q to a float's bits, rounded to nearest even */
static unsigned int q2f(int v, int q)
{
  unsigned int s = 0, a = (unsigned int)v;
  int e = 150 - q;
  if (v == 0)
    return 0;
  if (v < 0) {
    s = 0x80000000u;
    a = 0u - a;
  }
  if (a >= 0x1000000u) {
    int k = 1;
    unsigned int r, half;
    while ((a >> k) >= 0x1000000u)
      k++;
    r = a & ((1u << k) - 1);
    half = 1u << (k - 1);
    a >>= k;
    e += k;
    if (r > half || (r == half && (a & 1))) {
      a++;
      if (a == 0x1000000u) {
        a >>= 1;
        e++;
      }
    }
  } else {
    if (a < 0x100u) { a <<= 16; e -= 16; }
    if (a < 0x10000u) { a <<= 8; e -= 8; }
    if (a < 0x100000u) { a <<= 4; e -= 4; }
    if (a < 0x400000u) { a <<= 2; e -= 2; }
    if (a < 0x800000u) { a <<= 1; e -= 1; }
  }
  return s | ((unsigned int)e << 23) | (a & 0x7FFFFF);
}

static inline int fq(const void *p, int q) { return f2q(*(const unsigned int *)p, q); }
static inline int fqbe(const void *p, int q) { return f2q(tgr_rd32(p), q); }

static unsigned int isqrt32(unsigned int x)
{
  unsigned int r = 0, b = 1u << 30;
  while (b > x)
    b >>= 2;
  while (b) {
    if (x >= r + b) {
      x -= r + b;
      r = (r >> 1) + b;
    } else
      r >>= 1;
    b >>= 2;
  }
  return r;
}

static unsigned int isqrt64(u64 x)
{
  u64 r = 0, b = 1ULL << 62;
  if (x < 0x100000000ULL)
    return isqrt32((unsigned int)x);
  while (b > x)
    b >>= 2;
  while (b) {
    if (x >= r + b) {
      x -= r + b;
      r = (r >> 1) + b;
    } else
      r >>= 1;
    b >>= 2;
  }
  return (unsigned int)r;
}

/* sine and cosine (Q30) of an angle in radians (Q28) */
static void fx_sincos(int a, int *s, int *c)
{
  int k, r, r2, ps, pc, t;
  /* the nearest quarter turn, and what is left (within an eighth of a turn) */
  k = (int)(((i64)a * Q30(0.63661977236758134) + (1LL << 57)) >> 58);      /* a / (pi/2) */
  r = a - (int)(((i64)k * QK(1.5707963267948966, 28)));
  r <<= 2;                                                                      /* Q30 */
  r2 = MUL(r, r, 30);
  ps = MUL(r2, Q30(-1.0 / 362880.0), 30) + Q30(1.0 / 5040.0);
  ps = MUL(r2, ps, 30) - Q30(1.0 / 120.0);
  ps = MUL(r2, ps, 30) + Q30(1.0 / 6.0);
  ps = MUL(r2, ps, 30);
  ps = r - MUL(r, ps, 30);
  pc = MUL(r2, Q30(1.0 / 40320.0), 30) - Q30(1.0 / 720.0);
  pc = MUL(r2, pc, 30) + Q30(1.0 / 24.0);
  pc = MUL(r2, pc, 30) - Q30(0.5);
  pc = Q30(1.0) + MUL(r2, pc, 30);
  switch (k & 3) {
  case 0: *s = ps; *c = pc; break;
  case 1: *s = pc; *c = -ps; break;
  case 2: *s = -ps; *c = -pc; break;
  default: t = ps; *s = -pc; *c = t; break;
  }
}

/* out = m v (world into the body's axes; m Q30, v and out any one scale) */
static inline void fx_rot(int out[3], const int m[3][3], const int v[3])
{
  int t0 = (int)(((i64)m[0][0] * v[0] + (i64)m[0][1] * v[1] + (i64)m[0][2] * v[2]) >> 30);
  int t1 = (int)(((i64)m[1][0] * v[0] + (i64)m[1][1] * v[1] + (i64)m[1][2] * v[2]) >> 30);
  int t2 = (int)(((i64)m[2][0] * v[0] + (i64)m[2][1] * v[1] + (i64)m[2][2] * v[2]) >> 30);
  out[0] = t0;
  out[1] = t1;
  out[2] = t2;
}
/* out = m^T v (the body's axes back into the world) */
static inline void fx_rott(int out[3], const int m[3][3], const int v[3])
{
  int t0 = (int)(((i64)m[0][0] * v[0] + (i64)m[1][0] * v[1] + (i64)m[2][0] * v[2]) >> 30);
  int t1 = (int)(((i64)m[0][1] * v[0] + (i64)m[1][1] * v[1] + (i64)m[2][1] * v[2]) >> 30);
  int t2 = (int)(((i64)m[0][2] * v[0] + (i64)m[1][2] * v[1] + (i64)m[2][2] * v[2]) >> 30);
  out[0] = t0;
  out[1] = t1;
  out[2] = t2;
}
/* out = a x b, the products shifted down by sh */
static inline void fx_cross(int out[3], const int a[3], const int b[3], int sh)
{
  int t0 = (int)(((i64)a[1] * b[2] - (i64)a[2] * b[1]) >> sh);
  int t1 = (int)(((i64)a[2] * b[0] - (i64)a[0] * b[2]) >> sh);
  int t2 = (int)(((i64)a[0] * b[1] - (i64)a[1] * b[0]) >> sh);
  out[0] = t0;
  out[1] = t1;
  out[2] = t2;
}
static inline int fx_dot(const int a[3], const int b[3], int sh)
{
  return (int)(((i64)a[0] * b[0] + (i64)a[1] * b[1] + (i64)a[2] * b[2]) >> sh);
}

/* ---- a car's state ------------------------------------------------------ */
typedef struct FxSt {           /* BrRbState: a body's state (17 words) */
  int pos[3];                   /* Q16 */
  int vel[3];                   /* Q16 */
  int q[4];                     /* Q30 */
  int w[3];                     /* Q16 */
  int qd[4];                    /* Q30 a second */
} FxSt;
typedef struct FxWheel {        /* what is mirrored of a wheel's body */
  int pos[3];                   /* 0x78  its mount (z: its drop, held to [-0.4, 0]) Q16 */
  int tr[3];                    /* 0xEC  its matrix's position Q16 */
  int hit;                      /* 0x19C  the triangle under it (0: none) */
  int surf;                     /* 0x1A0  its surface */
  int n[3];                     /* 0x1A4  the triangle's normal Q30 */
  int d;                        /* 0x1B0  and constant Q16 */
  int cnt;                      /* 0x1B4  frames on the ground (0: off it) */
  int steer;                    /* 0x1C0  Q16 */
  int spin;                     /* 0x1C4  Q16 */
  int inertia;                  /* 0x1C8  Q16 */
  int drive;                    /* 0x1CC  Q8 */
  int brake;                    /* 0x1D0  Q8 */
  int angle;                    /* 0x1D4  degrees Q16 */
  int depth;                    /* 0x1D8  minus the probe's drop Q16 */
  int mass;                     /* 0x2C  Q8 */
} FxWheel;
typedef struct FxMirror {       /* every mirrored field, in k_runs' order */
  FxSt st;                      /* 0x1C0 */
  int m[4][3];                  /* 0x204  st's matrix: rotation Q30, position Q16 */
  FxSt sa;                      /* 0x25C  stA: the state the substeps start from */
  FxSt sb;                      /* 0x2A0  stB: the state being stepped */
  int roll;                     /* 0x31C  degrees Q16 */
  int box[4];                   /* 0x324  the body's size and its height (f1DC..f1E8) Q16 */
  int hitN[3];                  /* 0x334  Q30 */
  int stuck;                    /* 0x340 */
  int hitSize;                  /* 0x344 */
  int tyres;                    /* 0x345 */
  int hitPeak;                  /* 0x347 */
  int idle;                     /* 0x348 */
  int landed;                   /* 0x34B */
  int slide;                    /* 0x34C */
  int mass;                     /* 0x174  Q8 */
  int Iinv[3][3];               /* 0x19C  Q30 */
  int bn[3];                    /* 0x2EC  the body's own plane normal Q30 */
  int spring;                   /* 0x300  Q0 */
  int damper;                   /* 0x304  Q0 */
  int gripR;                    /* 0xE44  Q8 */
  int slipR;                    /* 0xE48 */
  int gripF;                    /* 0xE4C  Q8 */
  int slipF;                    /* 0xE50 */
  int skip;                     /* 0xE54 */
  FxWheel w[4];
} FxMirror;
#define NMIR ((int)(sizeof(FxMirror) / sizeof(int)))

/* the mirror's runs: base (0 the car, 1-4 its wheel bodies), offset, count, scale */
static const struct { unsigned char base; unsigned short off; unsigned char n, q; } k_runs[] = {
#define FX_STRUNS(o) {0, (o), 3, 16}, {0, (o) + 0x0C, 3, 16}, {0, (o) + 0x18, 4, 30}, {0, (o) + 0x28, 3, 16}, {0, (o) + 0x34, 4, 30}
  FX_STRUNS(0x1C0),
  {0, 0x204, 3, 30}, {0, 0x214, 3, 30}, {0, 0x224, 3, 30}, {0, 0x234, 3, 16},
  FX_STRUNS(0x25C),
  FX_STRUNS(0x2A0),
  {0, 0x31C, 1, 16}, {0, 0x324, 4, 16}, {0, 0x334, 3, 30}, {0, 0x340, 1, FX_RAW},
  {0, 0x344, 1, FX_BYTE}, {0, 0x345, 1, FX_BYTE}, {0, 0x347, 1, FX_BYTE}, {0, 0x348, 1, FX_BYTE},
  {0, 0x34B, 1, FX_BYTE}, {0, 0x34C, 1, FX_BYTE},
  {0, 0x174, 1, 8}, {0, 0x19C, 9, 30}, {0, 0x2EC, 3, 30}, {0, 0x300, 1, 0}, {0, 0x304, 1, 0},
  {0, 0xE44, 1, 8}, {0, 0xE48, 1, FX_BYTE}, {0, 0xE4C, 1, 8}, {0, 0xE50, 1, FX_BYTE}, {0, 0xE54, 1, FX_RAW},
#define FX_WRUNS(k) {k, 0x78, 3, 16}, {k, 0xEC, 3, 16}, {k, 0x19C, 1, FX_RAW}, {k, 0x1A0, 1, FX_BYTE}, \
  {k, 0x1A4, 3, 30}, {k, 0x1B0, 1, 16}, {k, 0x1B4, 1, FX_RAW}, {k, 0x1C0, 1, 16}, {k, 0x1C4, 1, 16}, \
  {k, 0x1C8, 1, 16}, {k, 0x1CC, 1, 8}, {k, 0x1D0, 1, 8}, {k, 0x1D4, 1, 16}, {k, 0x1D8, 1, 16}, {k, 0x2C, 1, 8}
  FX_WRUNS(1), FX_WRUNS(2), FX_WRUNS(3), FX_WRUNS(4),
};
#define NRUNS ((int)(sizeof k_runs / sizeof k_runs[0]))

typedef struct FxCar {
  FxMirror v;                   /* the values */
  int last[NMIR];               /* each as last read or written */
  unsigned int sh[NMIR];        /* and the record's word then */
  BrCar *car;                   /* the record (0: not yet read) */
  char *wb[4];                  /* its wheel bodies */
  int dirty;                    /* changed here since it was last written out */
  int invm;                     /* 2^32 / the mass */
  int invI;                     /* (the wheels' 1/inertia, Q16: all four the same) */
  int mass, inertia;            /* (what they were worked from) */
} FxCar;
static FxCar s_fx[4];

static char *fx_base(FxCar *c, int k)
{
  return k == 0 ? (char *)c->car : c->wb[k - 1];
}

/* read back what the game changed in the car's record (all of it the first time) */
static void fx_import(FxCar *c, BrCar *car)
{
  int r, i, j = 0, all = c->car != car;
  int *v = (int *)&c->v;
  if (all) {
    c->car = car;
    for (i = 0; i < 4; i++)
      c->wb[i] = TGR_PTR(char *, car->wheel[i]);
  }
  for (r = 0; r < NRUNS; r++) {
    char *p = fx_base(c, k_runs[r].base) + k_runs[r].off;
    int q = k_runs[r].q;
    for (i = 0; i < k_runs[r].n; i++, j++) {
      unsigned int u;
      if (q == FX_BYTE)
        u = ((unsigned char *)p)[i];
      else
        u = ((unsigned int *)p)[i];
      if (all || u != c->sh[j]) {
        c->sh[j] = u;
        v[j] = q >= FX_RAW ? (int)u : f2q(u, q);
        c->last[j] = v[j];
      }
    }
  }
  if (c->v.mass != c->mass) {
    c->mass = c->v.mass;
    c->invm = c->v.mass ? (int)((1LL << 40) / c->v.mass) : 0;          /* (Q8 mass: 2^32 / mass) */
  }
  if (c->v.w[0].inertia != c->inertia) {
    c->inertia = c->v.w[0].inertia;
    c->invI = c->inertia ? (int)((1LL << 32) / c->inertia) : 0;
  }
}

/* write out what changed here */
static void fx_export(FxCar *c)
{
  int r, i, j = 0;
  int *v = (int *)&c->v;
  for (r = 0; r < NRUNS; r++) {
    char *p = fx_base(c, k_runs[r].base) + k_runs[r].off;
    int q = k_runs[r].q;
    for (i = 0; i < k_runs[r].n; i++, j++) {
      unsigned int u;
      if (v[j] == c->last[j])
        continue;
      c->last[j] = v[j];
      if (q == FX_BYTE) {
        u = (unsigned char)v[j];
        ((unsigned char *)p)[i] = (unsigned char)u;
      } else {
        u = q >= FX_RAW ? (unsigned int)v[j] : q2f(v[j], q);
        ((unsigned int *)p)[i] = u;
      }
      c->sh[j] = u;
    }
  }
  /* the constant parts of the matrices */
  {
    float *m = (float *)((char *)c->car + 0x204);
    m[3] = m[7] = m[11] = 0.0f;
    m[15] = 1.0f;
  }
  c->dirty = 0;
}

static FxCar *fx_of(BrCar *car)
{
  return &s_fx[car->slot & 3];
}

/* ---- the body's state ------------------------------------------------- */
static void fx_quat_mat(int m[4][3], const FxSt *s)
{
  int w = s->q[0], x = s->q[1], y = s->q[2], z = s->q[3];
  int ww = MUL(w, w, 30), xx = MUL(x, x, 30), yy = MUL(y, y, 30), zz = MUL(z, z, 30);
  int xy2 = MUL(y, x, 29), wz2 = MUL(z, w, 29), xz2 = MUL(z, x, 29);
  int wy2 = MUL(y, w, 29), yz2 = MUL(z, y, 29), wx2 = MUL(x, w, 29);
  m[0][0] = ww + xx - yy - zz;
  m[0][1] = xy2 + wz2;
  m[0][2] = xz2 - wy2;
  m[1][0] = xy2 - wz2;
  m[1][1] = ww - xx + yy - zz;
  m[1][2] = yz2 + wx2;
  m[2][0] = xz2 + wy2;
  m[2][1] = yz2 - wx2;
  m[2][2] = ww - xx - yy + zz;
  m[3][0] = s->pos[0];
  m[3][1] = s->pos[1];
  m[3][2] = s->pos[2];
}

static void fx_quat_deriv(FxSt *s)
{
  int h0 = s->w[0] >> 1, h1 = s->w[1] >> 1, h2 = s->w[2] >> 1;    /* Q16 */
  int *q = s->q;
  s->qd[0] = (int)((-(i64)h0 * q[1] - (i64)h1 * q[2] - (i64)h2 * q[3]) >> 16);
  s->qd[1] = (int)(((i64)q[0] * h0 + (i64)h1 * q[3] - (i64)h2 * q[2]) >> 16);
  s->qd[2] = (int)(((i64)q[0] * h1 + (i64)h2 * q[1] - (i64)h0 * q[3]) >> 16);
  s->qd[3] = (int)(((i64)q[0] * h2 + (i64)h0 * q[2] - (i64)h1 * q[1]) >> 16);
}

/* a quaternion back to unit length: one Newton step of 1/sqrt from 1, which
 * is all a step's drift needs; a far one is taken the long way */
static void fx_quat_norm(int q[4])
{
  i64 s = (i64)q[0] * q[0] + (i64)q[1] * q[1] + (i64)q[2] * q[2] + (i64)q[3] * q[3];
  int l = (int)(s >> 30), k, i;
  if (l > Q30(0.9) && l < Q30(1.1)) {
    k = Q30(1.5) - (l >> 1);                                          /* (3 - l) / 2 */
    k = MUL(k, Q30(1.5) - (MUL(MUL(k, k, 30), l, 30) >> 1), 30);     /* (a second step) */
  } else {
    unsigned int r = isqrt64((u64)s);                                  /* Q30 */
    if (r == 0)
      return;
    k = (int)((1LL << 60) / r);
  }
  for (i = 0; i < 4; i++)
    q[i] = MUL(q[i], k, 30);
}

/* the velocity of the body (st) at a point in its axes: world axes */
static void fx_vel_at(int out[3], FxMirror *v, const int p[3])
{
  int r[3], c[3];
  fx_rott(r, v->m, p);
  fx_cross(c, v->st.w, r, 16);
  out[0] = v->st.vel[0] + c[0];
  out[1] = v->st.vel[1] + c[1];
  out[2] = v->st.vel[2] + c[2];
}

/* ---- the collision grid: each 32-unit square's triangles, as planes --------- */
typedef struct FxTri {
  int n[3];                     /* the unit normal Q30 */
  int d;                        /* the plane's constant Q16 */
  int v[3][3];                  /* the corners Q16 */
  short box[4];                 /* the x, y box in whole units, widened by one */
  unsigned short vi[3];         /* the corners' indices */
  unsigned char surf;           /* the surface */
  unsigned char ax;             /* the axis the normal is most along */
  short pad;
} FxTri;
#define FXT_POOL 768
#define FXC_N 12
static FxTri s_tri[FXT_POOL];
static int s_tri_head;
static struct { int key, tris, start, n; unsigned int used; } s_cell[FXC_N];
static unsigned int s_cell_clock;
unsigned int g_fx_cells, g_fx_cell_t;   /* (TGR_PERF: cells loaded, and the time) */

static void fx_tri_make(FxTri *t, unsigned short tri)
{
  const unsigned char *tris = BEPTR(const unsigned char *, D_80025C00.tris) + tri * 8;
  const BrVec3be *verts = BEPTR(const BrVec3be *, D_80025C00.verts);
  i64 n[3];
  int a[3], b[3], k, sh;
  u64 l2;
  unsigned int l;
  for (k = 0; k < 3; k++) {
    const BrVec3be *p;
    t->vi[k] = tgr_rd16(tris + 2 * k);
    p = &verts[t->vi[k]];
    t->v[k][0] = fqbe(&p->x, 16);
    t->v[k][1] = fqbe(&p->y, 16);
    t->v[k][2] = fqbe(&p->z, 16);
  }
  t->surf = BEPTR(unsigned char *, D_80025C00.surf)[tri] & 7;
  for (k = 0; k < 3; k++) {
    a[k] = t->v[1][k] - t->v[0][k];
    b[k] = t->v[2][k] - t->v[0][k];
  }
  n[0] = (i64)a[1] * b[2] - (i64)a[2] * b[1];
  n[1] = (i64)a[2] * b[0] - (i64)a[0] * b[2];
  n[2] = (i64)a[0] * b[1] - (i64)a[1] * b[0];
  /* the normal brought to 30 bits or under, then to unit length */
  sh = 0;
  for (;;) {
    i64 m0 = n[0] < 0 ? -n[0] : n[0], m1 = n[1] < 0 ? -n[1] : n[1], m2 = n[2] < 0 ? -n[2] : n[2];
    i64 m = m0 > m1 ? m0 : m1;
    if (m2 > m)
      m = m2;
    if (m < (1LL << 30))
      break;
    n[0] >>= 1;
    n[1] >>= 1;
    n[2] >>= 1;
    sh++;
  }
  l2 = (u64)(n[0] * n[0]) + (u64)(n[1] * n[1]) + (u64)(n[2] * n[2]);
  l = isqrt64(l2);
  for (k = 0; k < 3; k++)
    t->n[k] = l ? (int)((n[k] << 30) / (i64)l) : 0;
  t->d = -(int)(((i64)t->n[0] * t->v[0][0] + (i64)t->n[1] * t->v[0][1] + (i64)t->n[2] * t->v[0][2]) >> 30);
  /* the box */
  {
    int lx = t->v[0][0], hx = lx, ly = t->v[0][1], hy = ly;
    for (k = 1; k < 3; k++) {
      if (t->v[k][0] < lx) lx = t->v[k][0];
      if (t->v[k][0] > hx) hx = t->v[k][0];
      if (t->v[k][1] < ly) ly = t->v[k][1];
      if (t->v[k][1] > hy) hy = t->v[k][1];
    }
    t->box[0] = (short)((lx >> 16) - 1);
    t->box[1] = (short)((hx >> 16) + 1);
    t->box[2] = (short)((ly >> 16) - 1);
    t->box[3] = (short)((hy >> 16) + 1);
  }
  /* the axis the normal is most along (BrCrTriContainsPoint's) */
  {
    int a0 = ABS(t->n[0]), a1 = ABS(t->n[1]), a2 = ABS(t->n[2]);
    if (a1 < a0)
      t->ax = a2 < a0 ? 0 : 2;
    else
      t->ax = a2 < a1 ? 1 : 2;
  }
}

/* the triangles of the square under (x, y) (Q16), loaded if need be */
static FxTri *fx_cell(int x, int y, int *pn)
{
  int ix = x >> 16, iy = y >> 16, key, i, victim = 0, n;
  unsigned int tris = BE32(D_80025C00.tris);
  unsigned int packed;
  unsigned short cur[2], tri;
  if (x < 0)
    ix = -((-x) >> 16);
  if (y < 0)
    iy = -((-y) >> 16);
  key = ((iy / 32) << 6) + ix / 32;
  s_cell_clock++;
  for (i = 0; i < FXC_N; i++) {
    if (s_cell[i].n >= 0 && s_cell[i].key == key && s_cell[i].tris == (int)tris && s_cell[i].used) {
      s_cell[i].used = s_cell_clock;
      *pn = s_cell[i].n;
      return &s_tri[s_cell[i].start];
    }
    if (s_cell[i].used < s_cell[victim].used)
      victim = i;
  }
  {
    unsigned int t0 = *(volatile unsigned int *)0xBF801120 & 0xFFFF;
    float fx = (float)x * (1.0f / 65536.0f), fy = (float)y * (1.0f / 65536.0f);
    packed = BrGridCellRangeAt(fx, fy);
    n = (int)(packed >> 16);
    if (n > FXT_POOL)
      n = FXT_POOL;
    if (s_tri_head + n > FXT_POOL)
      s_tri_head = 0;
    /* whatever the new run covers is gone */
    for (i = 0; i < FXC_N; i++)
      if (s_cell[i].used && s_cell[i].n > 0 && s_cell[i].start < s_tri_head + n && s_tri_head < s_cell[i].start + s_cell[i].n)
        s_cell[i].used = 0;
    s_cell[victim].key = key;
    s_cell[victim].tris = (int)tris;
    s_cell[victim].start = s_tri_head;
    s_cell[victim].used = s_cell_clock;
    cur[1] = (packed >> 16) & 0xffff;
    cur[0] = packed;
    i = 0;
    if (packed != 0)
      while (i < n && (tri = BrU16QueuePop(cur)) != 0)
        fx_tri_make(&s_tri[s_tri_head + i++], tri);
    s_cell[victim].n = i;
    s_tri_head += i;
    g_fx_cells++;
    g_fx_cell_t += ((*(volatile unsigned int *)0xBF801120 & 0xFFFF) - t0) & 0xFFFF;
    *pn = i;
    return &s_tri[s_cell[victim].start];
  }
}

/* BrCrTriContainsPoint: the point (Q16) inside the triangle, flattened along
 * the axis its normal is most along (corners and point relative to the
 * first corner, in 1/4096 of a unit, the divisions turned into products) */
static int fx_tri_contains(const FxTri *t, const int p[3])
{
  int c = t->ax, i1, i2;
  i64 d1, d2, b1, b2, c1, c2;
  if (c == 0) { i1 = 1; i2 = 2; }
  else if (c == 1) { i1 = 2; i2 = 0; }
  else { i1 = 0; i2 = 1; }
  d1 = (p[i1] - t->v[0][i1]) >> 4;
  d2 = (p[i2] - t->v[0][i2]) >> 4;
  b1 = (t->v[1][i1] - t->v[0][i1]) >> 4;
  b2 = (t->v[1][i2] - t->v[0][i2]) >> 4;
  c1 = (t->v[2][i1] - t->v[0][i1]) >> 4;
  c2 = (t->v[2][i2] - t->v[0][i2]) >> 4;
  if (b1 == 0) {
    /* u = d1 / c1, v = (d2 - u c2) / b2 = (d2 c1 - d1 c2) / (c1 b2) */
    i64 nv, dv, s;
    if (c1 == 0 || b2 == 0)
      return 0;
    if (c1 > 0 ? (d1 < 0 || d1 > c1) : (d1 > 0 || d1 < c1))
      return 0;
    nv = d2 * c1 - d1 * c2;
    dv = c1 * b2;
    if (nv != 0 && (nv < 0) != (dv < 0))
      return 0;
    s = d1 * b2 + nv;                    /* (u + v) dv */
    return dv > 0 ? s <= dv : s >= dv;
  } else {
    /* u = (d2 b1 - d1 b2) / (c2 b1 - c1 b2), v = (d1 - u c1) / b1 */
    i64 nu = d2 * b1 - d1 * b2, du = c2 * b1 - c1 * b2, nv, dv, s;
    if (du == 0)
      return 0;
    if (du > 0 ? (nu < 0 || nu > du) : (nu > 0 || nu < du))
      return 0;
    nv = d1 * du - nu * c1;
    dv = b1 * du;
    if (nv != 0 && (nv < 0) != (dv < 0))
      return 0;
    s = nu * b1 + nv;                    /* (u + v) dv */
    return dv > 0 ? s <= dv : s >= dv;
  }
}

/* ---- the triangle against the unit cube (obb.c), in 1/4096 of a cube ------- */
#define CH Q12(0.5)
#define PAIRX(cum, out, expr, bp, bn, lim) \
  if ((cum) & ((bp) | (bn))) { \
    s = (expr); \
    if (((cum) & (bp)) && s > (lim)) (out) |= (bp); \
    else if (((cum) & (bn)) && s < -(lim)) (out) |= (bn); \
  }
static int fx_cube_trivial(const int v[3][3])
{
  int cum, i, bits, s;
  cum = ~0;
  for (i = 0; i < 3; i++) {
    int fb = 0;
    const int *p = v[i];
    PAIRX(~0, fb, p[0], 0x01, 0x02, CH)
    PAIRX(~0, fb, p[1], 0x04, 0x08, CH)
    PAIRX(~0, fb, p[2], 0x10, 0x20, CH)
    if (fb == 0)
      return 1;
    cum &= fb;
  }
  if (cum != 0)
    return 0;
  cum = ~0;
  for (i = 0; i < 3; i++) {
    bits = 0;
    PAIRX(cum, bits, v[i][0] + v[i][1], 0x001, 0x002, 2 * CH)
    PAIRX(cum, bits, v[i][0] - v[i][1], 0x004, 0x008, 2 * CH)
    PAIRX(cum, bits, v[i][0] + v[i][2], 0x010, 0x020, 2 * CH)
    PAIRX(cum, bits, v[i][0] - v[i][2], 0x040, 0x080, 2 * CH)
    PAIRX(cum, bits, v[i][1] + v[i][2], 0x100, 0x200, 2 * CH)
    PAIRX(cum, bits, v[i][1] - v[i][2], 0x400, 0x800, 2 * CH)
    cum = bits;
    if (cum == 0)
      break;
  }
  if (cum != 0)
    return 0;
  cum = ~0;
  for (i = 0; i < 3; i++) {
    bits = 0;
    PAIRX(cum, bits, v[i][0] + v[i][1] + v[i][2], 0x01, 0x02, 3 * CH)
    PAIRX(cum, bits, v[i][0] + v[i][1] - v[i][2], 0x04, 0x08, 3 * CH)
    PAIRX(cum, bits, v[i][0] - v[i][1] + v[i][2], 0x10, 0x20, 3 * CH)
    PAIRX(cum, bits, v[i][0] - v[i][1] - v[i][2], 0x40, 0x80, 3 * CH)
    cum = bits;
    if (cum == 0)
      break;
  }
  if (cum != 0)
    return 0;
  return -1;
}

static int fx_seg_cube(const int *v0, const int *v1)
{
  int i, e[3], sg[3];
  for (i = 0; i < 3; i++) {
    e[i] = v1[i] - v0[i];
    sg[i] = e[i] < 0 ? -1 : 1;
  }
  for (i = 0; i < 3; i++) {
    if (v0[i] * sg[i] > CH)
      return 0;
    if (v1[i] * sg[i] < -CH)
      return 0;
  }
  for (i = 0; i < 3; i++) {
    int i1 = (i + 1) % 3, i2 = (i + 2) % 3;
    i64 a = (i64)e[i2] * v0[i1] - (i64)e[i1] * v0[i2];             /* Q24 */
    i64 b = ((i64)e[i2] * sg[i1] + (i64)e[i1] * sg[i2]) * CH;       /* Q24 */
    if (a < 0)
      a = -a;
    if (b < 0)
      b = -b;
    if (a > b)
      return 0;
  }
  return 1;
}

static int fx_poly_point(const int v[3][3], const i64 n[3], const int p[3])
{
  i64 an[3];
  int z, x, y, i, count = 0;
  for (i = 0; i < 3; i++)
    an[i] = n[i] < 0 ? -n[i] : n[i];
  z = an[0] > an[2] ? (an[0] > an[1] ? 0 : 1) : 1 + (an[1] > an[2] ? 0 : 1);
  if (n[z] < 0) {
    x = (z + 2) % 3;
    y = (z + 1) % 3;
  } else {
    x = (z + 1) % 3;
    y = (z + 2) % 3;
  }
  for (i = 0; i < 3; i++) {
    const int *a = v[i], *b = v[(i + 1) % 3];
    int xd = (b[x] > p[x]) - (a[x] > p[x]);
    if (xd) {
      if ((b[y] > p[y]) - (a[y] > p[y])) {
        if (xd * (i64)(p[x] - a[x]) * (b[y] - a[y]) <= xd * (i64)(p[y] - a[y]) * (b[x] - a[x]))
          count += xd;
      } else if (a[y] <= p[y])
        count += xd;
    }
  }
  return count;
}

static int fx_cube_test(const int v[3][3])
{
  int r = fx_cube_trivial(v), i, dg[3], p[3];
  i64 n[3], num, den, t;
  int e1[3], e2[3];
  if (r != -1)
    return r;
  for (i = 0; i < 3; i++)
    if (fx_seg_cube(v[i], v[(i + 1) % 3]))
      return 1;
  for (i = 0; i < 3; i++) {
    e1[i] = v[1][i] - v[0][i];
    e2[i] = v[2][i] - v[0][i];
  }
  n[0] = (i64)e1[1] * e2[2] - (i64)e1[2] * e2[1];
  n[1] = (i64)e1[2] * e2[0] - (i64)e1[0] * e2[2];
  n[2] = (i64)e1[0] * e2[1] - (i64)e1[1] * e2[0];
  for (i = 0; i < 3; i++)
    dg[i] = n[i] < 0 ? -1 : 1;
  /* t = n.v0 / n.diagonal, in 1/4096 (the normal brought under 2^38 first) */
  while (ABS(n[0]) >= (1LL << 38) || ABS(n[1]) >= (1LL << 38) || ABS(n[2]) >= (1LL << 38)) {
    n[0] >>= 1;
    n[1] >>= 1;
    n[2] >>= 1;
  }
  num = n[0] * v[0][0] + n[1] * v[0][1] + n[2] * v[0][2];
  den = n[0] * dg[0] + n[1] * dg[1] + n[2] * dg[2];
  if (den == 0)
    return 0;
  t = num / den;
  if (t < -CH || t > CH)
    return 0;
  for (i = 0; i < 3; i++)
    p[i] = (int)t * dg[i];
  return fx_poly_point(v, n, p);
}

/* ---- the forces --------------------------------------------------------- */
static const int k_corner[4][2] = { { Q16(-1.5), Q16(-1.0) }, { Q16(-1.5), Q16(1.0) }, { Q16(1.5), Q16(-1.0) }, { Q16(1.5), Q16(1.0) } };
static int s_grip[3][24];       /* the grip tables: Q16, Q0, Q0 */
static int s_grip_ok;

/* BrTyreSprings: each wheel's spring load (Q8) from its drop */
static void fx_springs(FxMirror *v, int load[4])
{
  int i;
  for (i = 0; i < 4; i++) {
    FxWheel *w = &v->w[i];
    int prev = w->cnt, d = w->depth;
    if (w->cnt < 100)
      w->cnt++;
    if (d <= -Q16(0.3999)) {
      w->cnt = 0;
      d = -Q16(0.3);
    }
    if (d > 0)
      d = 0;
    d += Q16(0.3);
    if (d < 0)
      d = 0;
    d = MUL(d, d, 16);
    load[i] = (int)(((i64)d * v->spring) >> 8);
    if (w->cnt != 0 && prev == 0)
      v->landed = 0x80;
  }
}

/* BrWheelTyre: one wheel's drive along its rolling direction (its tyre force
 * into tf, body axes, Q8), its spin and its display angle */
static void fx_tyre(FxCar *c, int i, int *pA, int *pB, int tf[3])
{
  FxMirror *v = &c->v;
  FxWheel *w = &v->w[i];
  int a[3], cc[3], d[3], fwd[3], sn, cs, tq, q, load, t;
  if (w->hit == 0)
    return;
  if (w->n[2] < Q30(0.7))
    return;
  a[0] = v->m[1][0];                    /* the body's y axis in the world */
  a[1] = v->m[1][1];
  a[2] = v->m[1][2];
  fx_cross(cc, a, w->n, 30);
  fx_cross(d, w->n, cc, 30);
  fx_sincos(w->steer << 12, &sn, &cs);
  fwd[0] = MUL(cc[0], cs, 30) + MUL(d[0], sn, 30);
  fwd[1] = MUL(cc[1], cs, 30) + MUL(d[1], sn, 30);
  fwd[2] = MUL(cc[2], cs, 30) + MUL(d[2], sn, 30);
  if (v->w[0].cnt != 0 && v->w[2].cnt != 0 && v->w[1].cnt != 0 && v->w[3].cnt != 0) {
    int vel[3], dot, e[3], h;
    fx_vel_at(vel, v, w->pos);
    dot = fx_dot(vel, fwd, 30);                                         /* Q16 */
    load = (int)(((i64)(v->mass + 4 * w->mass) * Q16(2.9430003)) >> 16);  /* Q8 */
    load = MUL(load, w->n[2], 30);
    load = load * 3 + (load >> 1);
    tq = w->drive;
    q = (int)(((i64)tq * c->invI) >> 16);                               /* Q8 */
    *pA += q / 2;
    if (*pB != 0)
      q = MUL(q, Q16(0.9), 16);
    if (ABS(q) > ABS(load)) {
      t = MUL(ABS(load), Q16(0.1), 16);
      q = q < 0 ? -t : t;
    }
    e[0] = -MUL(q, fwd[0], 30);
    e[1] = -MUL(q, fwd[1], 30);
    e[2] = -MUL(q, fwd[2], 30);
    fx_rot(a, v->m, e);
    tf[0] += a[0];
    tf[1] += a[1];
    tf[2] += a[2];
    h = tq - MUL(w->inertia, q, 16);                                    /* Q8 */
    w->spin += (int)(((i64)h * DT_Q30) >> 22);
    w->spin -= MUL(MUL(w->spin, w->inertia, 16) + dot, Q16(0.4), 16);
  } else {
    tq = w->drive;
    w->spin += (int)(((i64)tq * DT_Q30) >> 22);
  }
  if (ABS(w->spin) > Q16(300.0))
    w->spin = w->spin < 0 ? -Q16(300.0) : Q16(300.0);
  w->angle -= (int)(((i64)w->spin * QK(57.295776 * 0.033333335, 24)) >> 24);
  while (w->angle > Q16(360.0))
    w->angle -= Q16(360.0);
  while (w->angle < 0)
    w->angle += Q16(360.0);
}

/* BrRbForcesClear and BrRbSolveAccel, worked in the body's axes: the body's
 * forces (four corner loads up its z axis, and one world force), the wheels'
 * forces (their flat parts turning the body when the wheel is down), into
 * the accelerations (Q16, world axes) */
static void fx_accel(FxCar *c, const int load[4], const int world[3], int wf[4][3], int acc[3], int aacc[3])
{
  FxMirror *v = &c->v;
  int F[3], T[3], t[3], i;
  i64 tb[3];
  fx_rot(F, v->m, world);
  tb[0] = tb[1] = tb[2] = 0;
  for (i = 0; i < 4; i++) {
    F[2] += load[i];
    tb[0] += (i64)k_corner[i][1] * load[i];
    tb[1] -= (i64)k_corner[i][0] * load[i];
  }
  if (wf)
    for (i = 0; i < 4; i++) {
      F[0] += wf[i][0];
      F[1] += wf[i][1];
      if (v->w[i].cnt != 0) {
        const int *p = v->w[i].tr;
        tb[0] -= (i64)p[2] * wf[i][1];
        tb[1] += (i64)p[2] * wf[i][0];
        tb[2] += (i64)p[0] * wf[i][1] - (i64)p[1] * wf[i][0];
      }
    }
  T[0] = (int)(tb[0] >> 16);
  T[1] = (int)(tb[1] >> 16);
  T[2] = (int)(tb[2] >> 16);
  for (i = 0; i < 3; i++)
    t[i] = (int)(((i64)F[i] * c->invm) >> 24);                           /* Q16 */
  fx_rott(acc, v->m, t);
  for (i = 0; i < 3; i++)
    t[i] = (int)(((i64)v->Iinv[i][0] * T[0] + (i64)v->Iinv[i][1] * T[1] + (i64)v->Iinv[i][2] * T[2]) >> 22);
  fx_rott(aacc, v->m, t);
}

static void fx_integrate(FxSt *s, const int acc[3], const int aacc[3])
{
  int i;
  for (i = 0; i < 3; i++) {
    s->vel[i] += MUL(acc[i], DT_Q30, 30);
    s->w[i] += MUL(aacc[i], DT_Q30, 30);
  }
}

/* the grip a surface pair gives at a lateral load s (Q0), as BrCarAxleGrip */
static int fx_grip(FxMirror *v, int idx, int s, int steered)
{
  int grip = s_grip[0][idx] - MUL(Q16(0.002), (v->tyres - 1) << 16, 16);
  int lo = s_grip[2][idx], hi = s_grip[1][idx], g = s;
  if (hi < s)
    g = hi;
  if (g < lo)
    g = lo;
  g = g ? (int)(((i64)lo << 16) / g) : 0;
  g = MUL(g * 20, grip, 16);
  if (!steered)
    g = g + (g >> 1);
  if (ABS(g) > Q16(1.0))
    g = Q16(1.0);
  return g;
}

/* BrCarAxleGrip */
static void fx_axle_grip(FxCar *c)
{
  FxMirror *v = &c->v;
  FxWheel *W = v->w;
  int slipF, slipR, pt[3], tA[3], vA[3], vB[3], side = 0, ran = 0, row, g, t, i, sp;
  int m4inv;
  for (i = 0; i < 4; i++)
    if (W[i].hit == 0)
      W[i].cnt = 0;
  /* the drive slip of each axle: its braked wheel's torque over its inertia and a quarter of the mass, over a frame squared */
  m4inv = v->mass ? (int)((4LL << 32) / v->mass) : 0;                     /* Q24 of 4/mass (Q8 mass) */
  {
    int k = (int)(((i64)ABS(W[2].brake) * c->invI) >> 16);               /* Q8 */
    k = (int)(((i64)k * m4inv) >> 16);                                    /* Q16 */
    k = MUL(k, MUL(DT_Q30, DT_Q30, 30), 30) * 2;
    slipF = W[2].spin > 0 ? -k : W[2].spin < 0 ? k : 0;
    k = (int)(((i64)ABS(W[0].brake) * c->invI) >> 16);
    k = (int)(((i64)k * m4inv) >> 16);
    k = MUL(k, MUL(DT_Q30, DT_Q30, 30), 30) * 2;
    slipR = W[0].spin > 0 ? -k : W[0].spin < 0 ? k : 0;
  }
  if (ABS(slipF) > Q16(1.0))
    slipF = slipF > 0 ? Q16(1.5) : -Q16(1.5);
  if (ABS(slipR) > Q16(1.0))
    slipR = slipR > 0 ? Q16(1.5) : -Q16(1.5);
  pt[0] = W[0].pos[0];
  pt[1] = pt[2] = 0;
  fx_vel_at(tA, v, pt);
  fx_rot(vA, v->m, tA);
  row = D_8028C800 - 1;
  if (row >= 3 || row < 0)
    row = 0;
  v->slide = 0;
  row <<= 3;
  if ((W[0].cnt != 0 || W[1].cnt != 0) && (W[2].cnt != 0 || W[3].cnt != 0)) {
    int idx = ((W[0].surf + W[1].surf + 1) >> 1) + row, s, hold;
    s = (int)(((i64)ABS(vA[1]) * v->mass * 30) >> 24) + (ABS(v->gripF) >> 8);          /* Q0: |v| mass / dt */
    ran = 1;
    hold = 8000;
    if (ABS(slipF) > 6)                                                  /* (0.0001) */
      s += ABS(slipR) > 6 ? 10000 : 0;
    else
      s += ABS(slipR) > 6 ? 100000 : 0;
    if (v->slipF != 0)
      hold = 5600;
    v->slipF = 0;
    g = fx_grip(v, idx, s, W[2].steer != 0);
    if (s < hold) {
      side = vA[1] = 0;
    } else {
      sp = (int)isqrt64((u64)((i64)v->st.vel[0] * v->st.vel[0] + (i64)v->st.vel[1] * v->st.vel[1] + (i64)v->st.vel[2] * v->st.vel[2]));
      if (sp < Q16(27.0)) {
        t = MUL(Q16(27.0) - sp, Q16(0.1 / 27.0), 16);
        if (g < t)
          g = t;
      }
      vA[1] = vA[1] - MUL(vA[1], g, 16);
      side = MUL(vA[1], g, 16);
      v->slipF = 1;
    }
    if (ABS(vA[0]) > Q16(1.0)) {
      if (ABS(vA[1]) > (ABS(vA[0]) >> 2))
        v->slide = 0x80;
    } else if (ABS(vA[1]) > Q16(1.0)) {
      v->slide = 0x80;
    } else {
      v->slide = 0;
    }
    t = vA[0];
    vA[0] -= slipR;
    if (vA[0] != 0 && SGN(vA[0]) != SGN(t))
      vA[0] = 0;
  } else {
    v->slipF = 0;
  }
  pt[0] = W[2].pos[0];
  pt[1] = pt[2] = 0;
  fx_vel_at(tA, v, pt);
  fx_rot(vB, v->m, tA);
  if ((W[2].cnt != 0 || W[3].cnt != 0) && (W[0].cnt != 0 || W[1].cnt != 0)) {
    int sn, cs, d, save[3], lat[3], vv, gg;
    ran = 1;
    fx_sincos(W[2].steer << 12, &sn, &cs);
    d = (int)(((i64)cs * vB[0] + (i64)sn * vB[1]) >> 30);
    save[0] = vB[0];
    save[1] = vB[1];
    save[2] = vB[2];
    vB[0] = MUL(cs, d, 30);
    vB[1] = MUL(sn, d, 30);
    vB[2] = 0;
    lat[0] = save[0] - vB[0];
    lat[1] = save[1] - vB[1];
    lat[2] = save[2] - vB[2];
    sp = (int)isqrt64((u64)((i64)lat[0] * lat[0] + (i64)lat[1] * lat[1] + (i64)lat[2] * lat[2]));
    vv = (ABS(v->gripR) >> 8) + (int)(((i64)sp * v->mass * 30) >> 24);
    gg = 8000;
    vv += ABS(slipF) > 6 ? 10000 : 0;
    if (v->slipR != 0)
      gg = 5600;
    v->slipR = 0;
    if (gg < vv) {
      int idx = ((W[2].surf + W[3].surf + 1) >> 1) + row;
      g = fx_grip(v, idx, vv, W[2].steer != 0);
      sp = (int)isqrt64((u64)((i64)v->st.vel[0] * v->st.vel[0] + (i64)v->st.vel[1] * v->st.vel[1] + (i64)v->st.vel[2] * v->st.vel[2]));
      if (sp < Q16(27.0)) {
        t = MUL(Q16(27.0) - sp, Q16(0.1 / 27.0), 16);
        if (g < t)
          g = t;
      }
      for (i = 0; i < 3; i++)
        vB[i] = save[i] - MUL(lat[i], g, 16);
      v->slipR = 1;
    }
  }
  if (ran) {
    int sv[3], w[3], wb = W[0].pos[0] - W[2].pos[0];
    sv[0] = (vB[0] + vA[0]) / 2;
    sv[2] = wb ? (int)(((i64)(vA[1] - vB[1]) << 16) / wb) : 0;
    sv[1] = vA[1] - MUL(W[0].pos[0], sv[2], 16);
    fx_rot(w, v->m, v->st.w);
    w[2] = sv[2];
    fx_rott(v->st.w, v->m, w);
    fx_rot(w, v->m, v->st.vel);
    w[0] = sv[0];
    w[1] = sv[1];
    fx_rott(v->st.vel, v->m, w);
  }
  if (ABS(side) > Q16(0.5))
    side = side > 0 ? Q16(0.5) : -Q16(0.5);
  side = -side * 8;                     /* (/ 0.5, * -4) */
  if (ABS(v->roll - side) < Q16(0.26666668))
    v->roll = side;
  else if (v->roll < side)
    v->roll += Q16(0.26666668);
  else
    v->roll -= Q16(0.26666668);
}

/* ---- collisions -------------------------------------------------------- */
#define FXL_N 160
typedef struct FxList { const FxTri *t[FXL_N]; int n; } FxList;
static FxList s_list;
unsigned int g_fx_bp[4];        /* (TGR_PERF: broad phase tests, hits; walk tests, hits) */

/* (TGR_FXDBG=2: the float test on the same corners) */
static int s_dbg = -1;
unsigned int g_fx_chk[4];
static int fx_check_cube(int c[3][3])
{
  float vf[9], e1[3], e2[3], n[3];
  int i;
  g_fx_chk[0]++;
  for (i = 0; i < 9; i++)
    ((unsigned int *)vf)[i] = q2f(c[i / 3][i % 3], 12);
  for (i = 0; i < 3; i++) {
    e1[i] = vf[3 + i] - vf[i];
    e2[i] = vf[6 + i] - vf[i];
  }
  n[0] = e1[1] * e2[2] - e1[2] * e2[1];
  n[1] = e1[2] * e2[0] - e1[0] * e2[2];
  n[2] = e1[0] * e2[1] - e1[1] * e2[0];
  return BrTriCubeTest(vf, n) != 0;
}

/* a corner into a cube's space: its offset from p along the axes m (Q30),
 * each scaled by s (Q16), less off on z (Q12) */
static inline void fx_to_cube(int out[3], const int v[3], const int p[3], const int m[3][3], const int s[3], int zoff)
{
  int r[3], k;
  for (k = 0; k < 3; k++)
    r[k] = v[k] - p[k];
  for (k = 0; k < 3; k++) {
    int l = (int)(((i64)m[k][0] * r[0] + (i64)m[k][1] * r[1] + (i64)m[k][2] * r[2]) >> 30);    /* Q16 */
    int c = (int)(((i64)l * s[k]) >> 20);                                                        /* Q12 */
    if (c > (1 << 22)) c = 1 << 22;
    if (c < -(1 << 22)) c = -(1 << 22);
    out[k] = c;
  }
  out[2] -= zoff;
}

/* BrCollRespBroadPhase: the square's triangles that meet the cube 5 units
 * each way along the car's axes, onto the contact list */
static void fx_broad(FxMirror *v)
{
  int n, i, cx = v->m[3][0] >> 16, cy = v->m[3][1] >> 16;
  static const int s01[3] = { Q16(0.1), Q16(0.1), Q16(0.1) };
  FxTri *t = fx_cell(v->m[3][0], v->m[3][1], &n);
  s_list.n = 0;
  for (i = 0; i < n; i++) {
    const FxTri *p = D_802A4A30 ? &t[n - 1 - i] : &t[i];
    int dist, r, k, c[3][3];
    if ((p->n[0] | p->n[1] | p->n[2]) == 0)
      continue;                         /* (a triangle with no area) */
    if (cx < p->box[0] - 10 || cx > p->box[1] + 10 || cy < p->box[2] - 10 || cy > p->box[3] + 10)
      continue;
    dist = fx_dot(p->n, v->m[3], 30) + p->d;
    dist = ABS(dist);
    if (dist > Q16(5.0)) {
      r = 0;
      for (k = 0; k < 3; k++) {
        int a = fx_dot(p->n, v->m[k], 44);                                /* Q16 */
        r += ABS(a);
      }
      if (dist > r * 5 + (r >> 2) + Q16(0.02))
        continue;
    }
    for (k = 0; k < 3; k++)
      fx_to_cube(c[k], p->v[k], v->m[3], (const int (*)[3])v->m, s01, 0);
    g_fx_bp[0]++;
    if (s_dbg == 2 && fx_check_cube(c) != (fx_cube_test((const int (*)[3])c) != 0))
      g_fx_chk[3]++;
    if (fx_cube_test((const int (*)[3])c) != 0) {
      g_fx_bp[1]++;
      if (s_list.n < FXL_N)
        s_list.t[s_list.n++] = p;
    }
  }
}

/* the float body a response is worked on: a BrTipBody's fields at their places */
typedef struct FxTip { float f[0x208 / 4]; } FxTip;
#define TIPF(b, off) (&(b)->f[(off) / 4])

static void fx_st_out(float *o, const FxSt *s)
{
  int i;
  for (i = 0; i < 3; i++) {
    ((unsigned int *)o)[i] = q2f(s->pos[i], 16);
    ((unsigned int *)o)[3 + i] = q2f(s->vel[i], 16);
    ((unsigned int *)o)[10 + i] = q2f(s->w[i], 16);
  }
  for (i = 0; i < 4; i++) {
    ((unsigned int *)o)[6 + i] = q2f(s->q[i], 30);
    ((unsigned int *)o)[13 + i] = q2f(s->qd[i], 30);
  }
}
static void fx_st_in(FxSt *s, const float *o)
{
  int i;
  for (i = 0; i < 3; i++) {
    s->pos[i] = fq(&o[i], 16);
    s->vel[i] = fq(&o[3 + i], 16);
    s->w[i] = fq(&o[10 + i], 16);
  }
  for (i = 0; i < 4; i++) {
    s->q[i] = fq(&o[6 + i], 30);
    s->qd[i] = fq(&o[13 + i], 30);
  }
}
static void fx_tip_out(FxTip *b, FxMirror *v)
{
  int i, j;
  float *m = TIPF(b, 0xBC);
  for (i = 0; i < 3; i++) {
    for (j = 0; j < 3; j++) {
      ((unsigned int *)m)[i * 4 + j] = q2f(v->m[i][j], 30);
      ((unsigned int *)TIPF(b, 0x54))[i * 3 + j] = q2f(v->Iinv[i][j], 30);
    }
    m[i * 4 + 3] = 0.0f;
    ((unsigned int *)m)[12 + i] = q2f(v->m[3][i], 16);
    ((unsigned int *)TIPF(b, 0x1EC))[i] = q2f(v->hitN[i], 30);
  }
  m[15] = 1.0f;
  *(unsigned int *)TIPF(b, 0x2C) = q2f(v->mass, 8);
  for (i = 0; i < 4; i++)
    ((unsigned int *)TIPF(b, 0x1DC))[i] = q2f(v->box[i], 16);
  fx_st_out(TIPF(b, 0x114), &v->sa);
  fx_st_out(TIPF(b, 0x158), &v->sb);
  ((unsigned char *)b)[0x1FC] = (unsigned char)v->hitSize;
  ((unsigned char *)b)[0x1FF] = (unsigned char)v->hitPeak;
  ((unsigned char *)b)[0x200] = (unsigned char)v->idle;
}
static void fx_tip_in(FxMirror *v, FxTip *b)
{
  int i, j;
  float *m = TIPF(b, 0xBC);
  for (i = 0; i < 3; i++) {
    for (j = 0; j < 3; j++)
      v->m[i][j] = fq(&m[i * 4 + j], 30);
    v->m[3][i] = fq(&m[12 + i], 16);
    v->hitN[i] = fq(&TIPF(b, 0x1EC)[i], 30);
  }
  fx_st_in(&v->sb, TIPF(b, 0x158));
  v->hitSize = ((unsigned char *)b)[0x1FC];
  v->hitPeak = ((unsigned char *)b)[0x1FF];
}

/* BrCrRespWalk: each listed triangle that meets the car's box (the cube
 * space of m: the box's centre p, its axes mr, scales s, z less zoff) is
 * resolved; the response itself (rare) by the game's float routines */
static int fx_walk(FxMirror *v, const int p[3], const int mr[3][3], const int s[3], int zoff)
{
  int k, ret = 0, cnt = 0, ext[3], up, cen[3], ax[3][3], j;
  /* the box as the walk begins: a plane that misses it is passed over */
  up = MUL(v->box[3], v->box[2], 16);
  for (k = 0; k < 3; k++)
    cen[k] = v->m[3][k] + MUL(v->m[2][k], up, 30);
  ext[0] = v->box[0];
  ext[1] = v->box[1];
  ext[2] = v->box[2];
  for (k = 0; k < 3; k++)
    for (j = 0; j < 3; j++)
      ax[k][j] = v->m[k][j];
  for (k = s_list.n - 1; k >= 0; k--) {
    const FxTri *t = s_list.t[k];
    int c[3][3], i, dist, r;
    dist = fx_dot(t->n, cen, 30) + t->d;
    dist = ABS(dist);
    r = 0;
    for (i = 0; i < 3; i++) {
      int a = fx_dot(t->n, ax[i], 30);                                   /* Q30 */
      r += MUL(ABS(a), ext[i], 30);
    }
    if (dist > (r >> 1) + (r >> 5) + Q16(0.02)) {
      if (s_dbg == 2) {
        for (i = 0; i < 3; i++)
          fx_to_cube(c[i], t->v[i], p, mr, s, zoff);
        if (fx_check_cube(c) != 0)
          g_fx_chk[2]++;          /* (passed over, but the float test says it meets) */
      }
      continue;
    }
    for (i = 0; i < 3; i++)
      fx_to_cube(c[i], t->v[i], p, mr, s, zoff);
    g_fx_bp[2]++;
    if (s_dbg == 2 && fx_check_cube(c) != (fx_cube_test((const int (*)[3])c) != 0))
      g_fx_chk[1]++;
    if (fx_cube_test((const int (*)[3])c) == 0)
      continue;
    g_fx_bp[3]++;
    cnt++;
    {
      /* the response, in float */
      static FxTip b;
      float vf[9], nrm[3], e1[3], e2[3], planeD, sign[3], dp[3], dd, pn[3];
      int flag = 1, spin = 0, rr, sgn;
      for (i = 0; i < 9; i++)
        ((unsigned int *)vf)[i] = q2f(c[i / 3][i % 3], 12);
      for (i = 0; i < 3; i++) {
        e1[i] = vf[3 + i] - vf[i];
        e2[i] = vf[6 + i] - vf[i];
        ((unsigned int *)pn)[i] = q2f(t->n[i], 30);
      }
      nrm[0] = e1[1] * e2[2] - e1[2] * e2[1];
      nrm[1] = e1[2] * e2[0] - e1[0] * e2[2];
      nrm[2] = e1[0] * e2[1] - e1[1] * e2[0];
      BrVec3NormaliseF(nrm);
      D_802A4A28 = 0;
      planeD = (nrm[0] * vf[0] + nrm[1] * vf[1]) + nrm[2] * vf[2];
      if (D_8026FF18 == 4) {
        if ((nrm[0] < 0.0f ? -nrm[0] : nrm[0]) > 0.999f || (nrm[1] < 0.0f ? -nrm[1] : nrm[1]) > 0.999f
            || (nrm[2] < 0.0f ? -nrm[2] : nrm[2]) > 0.999f) {
          D_802A4A28 = 1;
          spin = 1;
        } else if ((planeD < 0.0f ? -planeD : planeD) < 0.5f) {
          spin = 0;
          D_802A4A28 = 2;
        }
      }
      e1[0] = nrm[0] * planeD;
      e1[1] = nrm[1] * planeD;
      e1[2] = nrm[2] * planeD;
      sgn = e1[0] < 0 ? -1 : 1;
      sign[0] = sgn * 0.5f;
      sgn = e1[1] < 0 ? -1 : 1;
      sign[1] = sgn * 0.5f;
      sgn = e1[2] < 0 ? -1 : 1;
      sign[2] = sgn * 0.5f;
      fx_tip_out(&b, v);
      D_8037EAA8[0] = sign[0] * *TIPF(&b, 0x1DC);
      D_8037EAA8[1] = sign[1] * *TIPF(&b, 0x1E0);
      D_8037EAA8[2] = sign[2] * *TIPF(&b, 0x1E4) + *TIPF(&b, 0x1E8);
      D_802A4A2C = pn;
      BrCrPlaneResolve(&b, nrm, planeD, sign, vf);
      if (*TIPF(&b, 0xBC + 0x28) > 0.5f)
        flag = 0;
      if (D_802A4A28 != 1)
        rr = BrCrImpulseSolve(&b, D_8037EAA8, pn, flag, 0.0f);
      else
        rr = BrCrContactKick(&b, pn, flag, spin);
      if (rr != 0) {
        float *cur = TIPF(&b, 0x158), *st = TIPF(&b, 0x114);
        ret = 1;
        dp[0] = cur[0] - st[0];
        dp[1] = cur[1] - st[1];
        dp[2] = cur[2] - st[2];
        dd = ((dp[0] * pn[0] + dp[1] * pn[1]) + dp[2] * pn[2]) * 1.1;
        cur[0] -= pn[0] * dd;
        cur[1] -= pn[1] * dd;
        cur[2] -= pn[2] * dd;
        cur[6] = st[6];
        cur[7] = st[7];
        cur[8] = st[8];
        BrRbQuatDerivative(cur);
        BrQuatToMat(TIPF(&b, 0xBC), cur);
      }
      fx_tip_in(v, &b);
    }
  }
  if (cnt == 0) {
    if (v->idle < 40)
      v->idle++;
  } else
    v->idle = 0;
  return ret;
}

/* BrCollRespTipKick: a car on one side or its nose, nearly still, is kicked
 * over */
static int fx_tip_kick(FxMirror *v)
{
  int count = 0, best = Q16(100.0), i, p[3], w[3], t, last = -1;
  fx_quat_mat(v->m, &v->sa);
  for (i = 0; i < 4; i++) {
    FxWheel *W = &v->w[i];
    int d;
    if (W->cnt == 0)
      continue;
    count++;
    last = i;
    p[0] = W->pos[0] > 0 ? v->box[0] >> 1 : W->pos[0] < 0 ? -(v->box[0] >> 1) : 0;
    p[1] = W->pos[1] > 0 ? v->box[1] >> 1 : W->pos[1] < 0 ? -(v->box[1] >> 1) : 0;
    p[2] = -(v->box[2] >> 1) + v->box[3];
    fx_rott(w, v->m, p);
    w[0] += v->m[3][0];
    w[1] += v->m[3][1];
    w[2] += v->m[3][2];
    d = fx_dot(W->n, w, 30) + W->d;
    d = ABS(d);
    if (d < best)
      best = d;
  }
  if (count > 2 || count < 1)
    return 0;
  if (best > Q16(0.6))
    return 0;
  fx_vel_at(w, v, p);
  t = fx_dot(w, v->w[last].n, 30);
  if (ABS(t) > Q16(0.1))
    return 0;
  t = fx_dot(v->m[0], v->bn, 30);
  p[0] = 0;
  p[2] = 0;
  p[1] = t > 0 ? Q16(0.1) : -Q16(0.1);
  fx_rott(w, v->m, p);
  for (i = 0; i < 3; i++)
    v->sa.w[i] += 2 * w[i];
  fx_quat_deriv(&v->sa);
  return 1;
}

/* BrObbOverlap, Q16 offsets and sizes, Q30 rotation */
static int fx_obb(const int m[9], const int t[3], const int a[3], const int b[3])
{
  int am[9], i, d;
  for (i = 0; i < 9; i++)
    am[i] = ABS(m[i]);
#define P(x, y) MUL(x, y, 30)
  if (ABS(t[0]) > a[0] + P(b[0], am[0]) + P(b[1], am[1]) + P(b[2], am[2])) return 0;
  d = P(t[0], m[0]) + P(t[1], m[3]) + P(t[2], m[6]);
  if (ABS(d) > b[0] + P(a[0], am[0]) + P(a[1], am[3]) + P(a[2], am[6])) return 0;
  if (ABS(t[1]) > a[1] + P(b[0], am[3]) + P(b[1], am[4]) + P(b[2], am[5])) return 0;
  if (ABS(t[2]) > a[2] + P(b[0], am[6]) + P(b[1], am[7]) + P(b[2], am[8])) return 0;
  d = P(t[0], m[1]) + P(t[1], m[4]) + P(t[2], m[7]);
  if (ABS(d) > b[1] + P(a[0], am[1]) + P(a[1], am[4]) + P(a[2], am[7])) return 0;
  d = P(t[0], m[2]) + P(t[1], m[5]) + P(t[2], m[8]);
  if (ABS(d) > b[2] + P(a[0], am[2]) + P(a[1], am[5]) + P(a[2], am[8])) return 0;
  d = P(t[2], m[3]) - P(t[1], m[6]);
  if (ABS(d) > P(a[1], am[6]) + P(a[2], am[3]) + P(b[1], am[2]) + P(b[2], am[1])) return 0;
  d = P(t[2], m[4]) - P(t[1], m[7]);
  if (ABS(d) > P(a[1], am[7]) + P(a[2], am[4]) + P(b[0], am[2]) + P(b[2], am[0])) return 0;
  d = P(t[2], m[5]) - P(t[1], m[8]);
  if (ABS(d) > P(a[1], am[8]) + P(a[2], am[5]) + P(b[0], am[1]) + P(b[1], am[0])) return 0;
  d = P(t[0], m[6]) - P(t[2], m[0]);
  if (ABS(d) > P(a[0], am[6]) + P(a[2], am[0]) + P(b[1], am[5]) + P(b[2], am[4])) return 0;
  d = P(t[0], m[7]) - P(t[2], m[1]);
  if (ABS(d) > P(a[0], am[7]) + P(a[2], am[1]) + P(b[0], am[5]) + P(b[2], am[3])) return 0;
  d = P(t[0], m[8]) - P(t[2], m[2]);
  if (ABS(d) > P(a[0], am[8]) + P(a[2], am[2]) + P(b[0], am[4]) + P(b[1], am[3])) return 0;
  d = P(t[1], m[0]) - P(t[0], m[3]);
  if (ABS(d) > P(a[0], am[3]) + P(a[1], am[0]) + P(b[1], am[8]) + P(b[2], am[7])) return 0;
  d = P(t[1], m[1]) - P(t[0], m[4]);
  if (ABS(d) > P(a[0], am[4]) + P(a[1], am[1]) + P(b[0], am[8]) + P(b[2], am[6])) return 0;
  d = P(t[1], m[2]) - P(t[0], m[5]);
  if (ABS(d) > P(a[0], am[5]) + P(a[1], am[2]) + P(b[0], am[7]) + P(b[1], am[6])) return 0;
#undef P
  return 1;
}

/* BrCarCarCollide's response for one pair (cars a and b, their records as
 * they now stand): the game's float code */
static void fx_carcar_respond(BrCar *A, BrCar *B)
{
  BrVec3 d;
  BrRbState *pa = &A->st, *pb = &B->st;
  float s, x, dd[3], t[3], sd[3], imp[3];
  d.x = pa->pos.x - pb->pos.x;
  d.y = pa->pos.y - pb->pos.y;
  d.z = pa->pos.z - pb->pos.z;
  BrVec3NormaliseF(&d.x);
  s = 0.0f;
  s += d.z * pa->vel.z + (pa->vel.x * d.x + pa->vel.y * d.y);
  x = s;
  s += pb->vel.x * d.x + pb->vel.y * d.y + pb->vel.z * d.z;
  s *= 0.5f;
  imp[0] = d.x * s;
  imp[1] = d.y * s;
  imp[2] = d.z * s;
  x = s < x ? -(s - x) : s - x;
  if (x > 27.0f)
    x = 27.0f;
  if (A->hitAge > 40)
    B->sndHitA = A->sndHitA = tgr_f2u(127.0f * x / 27.0f + 128.0f);
  A->hitAge = 0;
  sd[0] = -d.x;
  sd[1] = -d.y;
  sd[2] = -d.z;
  t[0] = sd[0] * 2.5f;
  t[1] = sd[1] * 2.5f;
  t[2] = sd[2] * 2.5f;
  BrMat4RotateVec(dd, A->stMtx, t);
  pa->vel.x -= imp[0];
  pa->vel.y -= imp[1];
  pa->vel.z -= imp[2];
  memcpy(&A->stB, &A->st, sizeof(BrRbState));
  BrCrImpulseSolve((char *)A + 0x148, dd, &d.x, 0, 0.45f);
  memcpy(&A->st, &A->stB, sizeof(BrRbState));
  A->stB.vel.x += imp[0];
  A->stB.vel.y += imp[1];
  A->stB.vel.z += imp[2];
  A->stA.vel = A->stB.vel;
  pa->vel.x += imp[0];
  pa->vel.y += imp[1];
  pa->vel.z += imp[2];
  t[0] = d.x * 2.5f;
  t[1] = d.y * 2.5f;
  t[2] = d.z * 2.5f;
  BrMat4RotateVec(dd, B->stMtx, t);
  pb->vel.x -= imp[0];
  pb->vel.y -= imp[1];
  pb->vel.z -= imp[2];
  memcpy(&B->stB, &B->st, sizeof(BrRbState));
  BrCrImpulseSolve((char *)B + 0x148, dd, sd, 0, 0.45f);
  memcpy(&B->st, &B->stB, sizeof(BrRbState));
  B->stB.vel.x += imp[0];
  B->stB.vel.y += imp[1];
  B->stB.vel.z += imp[2];
  B->stA.vel = B->stB.vel;
  pb->vel.x += imp[0];
  pb->vel.y += imp[1];
  pb->vel.z += imp[2];
}

/* BrCarCarCollide: the cars within 5 units of each other whose boxes meet */
static void fx_carcar(void)
{
  int i, j;
  static const int ext[3] = { Q16(2.5), Q16(1.0), Q16(1.0) };
  for (i = 0; i < D_8028B7F0; i++) {
    BrCar *A = TGR_PTR(BrCar *, D_803239A0[i].car);
    FxCar *ca;
    if (A == 0 || A->colour[3] == 2)
      continue;
    A->hitAge++;
    ca = fx_of(A);
    if (ca->car != A)
      fx_import(ca, A);
    for (j = i + 1; j < D_8028B7F0; j++) {
      BrCar *B = TGR_PTR(BrCar *, D_803239A0[j].car);
      FxCar *cb;
      int d[3], k, l, mr[9], t[3];
      i64 d2;
      if (B == 0 || B->colour[3] == 2)
        continue;
      cb = fx_of(B);
      if (cb->car != B)
        fx_import(cb, B);
      for (k = 0; k < 3; k++)
        d[k] = ca->v.st.pos[k] - cb->v.st.pos[k];
      d2 = (i64)d[0] * d[0] + (i64)d[1] * d[1] + (i64)d[2] * d[2];
      if (d2 >= (25LL << 32))
        continue;
      /* mR = mB^T mA, t = mA d */
      for (k = 0; k < 3; k++)
        for (l = 0; l < 3; l++)
          mr[k * 3 + l] = (int)(((i64)cb->v.m[0][k] * ca->v.m[0][l] + (i64)cb->v.m[1][k] * ca->v.m[1][l] +
                                 (i64)cb->v.m[2][k] * ca->v.m[2][l]) >> 30);
      fx_rot(t, (const int (*)[3])ca->v.m, d);
      if (!fx_obb(mr, t, ext, ext))
        return;
      /* the response on the records */
      fx_export(ca);
      fx_export(cb);
      fx_carcar_respond(A, B);
      fx_import(ca, A);
      fx_import(cb, B);
      ca->dirty = cb->dirty = 1;
    }
  }
}

static void fx_state_step(FxSt *o, const FxSt *s)
{
  int i;
  for (i = 0; i < 3; i++) {
    o->pos[i] = s->pos[i] + MUL(s->vel[i], DT4_Q30, 30);
    o->vel[i] = s->vel[i];
    o->w[i] = s->w[i];
  }
  for (i = 0; i < 4; i++) {
    o->q[i] = s->q[i] + MUL(s->qd[i], DT4_Q30, 30);
    o->qd[i] = s->qd[i];
  }
  fx_quat_norm(o->q);
}

/* BrCarPhysAdvance */
unsigned int g_fx_t[6];         /* (TGR_PERF: forces, advance, probes...) */
static void fx_advance(FxCar *c)
{
  FxMirror *v = &c->v;
  int s[3], k, step, zoff;
  D_8037EAA8[0] = D_8037EAA8[1] = D_8037EAA8[2] = 0.0f;
  D_8037EAC8[0] = D_8037EAC8[1] = D_8037EAC8[2] = 0.0f;
  D_802A4A28 = 0;
  D_802A4A2C = 0;
  fx_broad(v);
  for (k = 0; k < 3; k++)
    s[k] = v->box[k] ? (int)((1LL << 32) / v->box[k]) : 0;               /* Q16 of 1 / size */
  zoff = v->box[3] >> 4;                                                  /* Q12 */
  for (step = 0; step < 4; step++) {
    int mr[3][3], p[3], i, j;
    fx_tip_kick(v);
    fx_carcar();
    fx_state_step(&v->sb, &v->sa);
    fx_quat_mat(v->m, &v->sb);
    for (i = 0; i < 3; i++) {
      p[i] = v->m[3][i];
      for (j = 0; j < 3; j++)
        mr[i][j] = v->m[i][j];
    }
    if (fx_walk(v, p, (const int (*)[3])mr, s, zoff)) {
      fx_quat_deriv(&v->sb);
      fx_quat_mat(v->m, &v->sb);
    }
    v->sa = v->sb;
  }
  fx_quat_mat(v->m, &v->sb);
  v->sa = v->sb;
  if (v->m[2][2] < Q30(0.5)) {
    if (v->stuck < 0)
      v->stuck = -1;
    v->stuck--;
  } else
    v->stuck = 35;
}

/* BrWheelGroundProbe: how far the wheel can drop to the ground (Q16; 100
 * when nothing is within 2), the triangle it meets kept on the wheel */
static int fx_probe(FxMirror *v, FxWheel *w)
{
  int world[3], dir[3], n, i, best = Q16(100.0), wx, wy, mount[3];
  FxTri *t;
  mount[0] = w->pos[0];
  mount[1] = w->pos[1];
  mount[2] = 0;
  fx_rott(world, v->m, mount);
  world[0] += v->m[3][0];
  world[1] += v->m[3][1];
  world[2] += v->m[3][2];
  dir[0] = -v->m[2][0];
  dir[1] = -v->m[2][1];
  dir[2] = -v->m[2][2];
  w->hit = 0;
  t = fx_cell(world[0], world[1], &n);
  wx = world[0] >> 16;
  wy = world[1] >> 16;
  for (i = 0; i < n; i++, t++) {
    int d, tt, h, p[3];
    if (wx < t->box[0] - 3 || wx > t->box[1] + 3 || wy < t->box[2] - 3 || wy > t->box[3] + 3)
      continue;
    d = fx_dot(t->n, world, 30) + t->d;
    if (d <= -Q16(2.0) || d >= Q16(2.0))
      continue;
    tt = fx_dot(t->n, dir, 30);                                          /* Q30 */
    if (ABS(tt) <= Q30(0.001))
      continue;
    h = -(int)(((i64)d << 30) / tt);                                      /* Q16 */
    if (h <= -Q16(2.0) || h >= Q16(2.0) || h >= best || t->n[2] <= Q30(0.2))
      continue;
    p[0] = world[0] + MUL(dir[0], h, 30);
    p[1] = world[1] + MUL(dir[1], h, 30);
    p[2] = world[2] + MUL(dir[2], h, 30);
    if ((p[0] >> 16) < t->box[0] - 1 || (p[0] >> 16) > t->box[1] + 1 || (p[1] >> 16) < t->box[2] - 1 || (p[1] >> 16) > t->box[3] + 1)
      continue;
    if (!fx_tri_contains(t, p))
      continue;
    w->hit = (int)tgr_addr32(t);
    w->surf = t->surf;
    best = h;
    w->n[0] = t->n[0];
    w->n[1] = t->n[1];
    w->n[2] = t->n[2];
    w->d = t->d;
  }
  return best;
}

/* ---- the step ----------------------------------------------------------- */
#define FX_NOW() (*(volatile unsigned int *)0xBF801120 & 0xFFFF)

#define FXDBG(tag) do { if (s_dbg == 1) tgr_trace("fx", "%s pos %d %d %d vel %d %d %d sb %d %d %d", tag, v->st.pos[0] >> 6, \
  v->st.pos[1] >> 6, v->st.pos[2] >> 6, v->st.vel[0] >> 6, v->st.vel[1] >> 6, v->st.vel[2] >> 6, v->sb.pos[0] >> 6, \
  v->sb.pos[1] >> 6, v->sb.pos[2] >> 6); } while (0)

/* BrCarPhysStep, natively: the car's frame of physics */
void BrFxCarStep(BrCar *car)
{
  FxCar *c = fx_of(car);
  FxMirror *v = &c->v;
  int load[4], wf[4][3], acc[3], aacc[3], g[3], i, k;
  unsigned int t0 = FX_NOW(), t1;
  if (!s_grip_ok) {
    for (i = 0; i < 24; i++) {
      s_grip[0][i] = fq(&D_802A4A38[i], 16);
      s_grip[1][i] = fq(&D_802A4A98[i], 0);
      s_grip[2][i] = fq(&D_802A4AF8[i], 0);
    }
    s_grip_ok = 1;
  }
  fx_import(c, car);
  {
    if (s_dbg < 0)
      s_dbg = getenv("TGR_FXDBG") ? atoi(getenv("TGR_FXDBG")) : 0;
    if (s_dbg == 1)
      tgr_trace("fx", "slot %d car %d in pos %d %d %d vel %d %d %d skip %d", car->slot, (int)(car - D_8031B760),
                v->st.pos[0] >> 6, v->st.pos[1] >> 6, v->st.pos[2] >> 6, v->st.vel[0] >> 6, v->st.vel[1] >> 6,
                v->st.vel[2] >> 6, v->skip);
  }
  /* the first pass: springs, tyres, gravity */
  for (i = 0; i < 4; i++)
    wf[i][0] = wf[i][1] = wf[i][2] = 0;
  fx_springs(v, load);
  if (v->skip == 0) {
    v->gripR = v->gripF = 0;
    v->slipF = 0;
    fx_tyre(c, 0, &v->gripF, &v->slipF, wf[0]);
    fx_tyre(c, 1, &v->gripF, &v->slipF, wf[1]);
    fx_tyre(c, 2, &v->gripR, &v->slipR, wf[2]);
    fx_tyre(c, 3, &v->gripR, &v->slipR, wf[3]);
  }
  v->skip = 0;
  /* each wheel's share of the weight (0, 0, -103.005 in the world), in the body's axes */
  for (i = 0; i < 4; i++)
    for (k = 0; k < 3; k++)
      wf[i][k] += MUL(v->m[k][2], -Q8(103.005005), 30);
  g[0] = g[1] = 0;
  g[2] = -Q8(15450.75);
  fx_accel(c, load, g, wf, acc, aacc);
  fx_integrate(&v->st, acc, aacc);
  fx_axle_grip(c);
  FXDBG("pass1");
  fx_quat_deriv(&v->st);
  /* the second pass: drag, the wheels' dampers */
  {
    int drag[3], sp2;
    i64 vv = (i64)v->st.vel[0] * v->st.vel[0] + (i64)v->st.vel[1] * v->st.vel[1] + (i64)v->st.vel[2] * v->st.vel[2];
    int k110 = 110, any4 = v->w[0].surf == 4 || v->w[1].surf == 4 || v->w[2].surf == 4 || v->w[3].surf == 4;
    if (vv > (16LL << 32) && D_8028C800 != 3 && any4)
      k110 += 220;
    for (k = 0; k < 3; k++)
      drag[k] = -(int)(((i64)v->st.vel[k] * k110) >> 8);                  /* Q8 */
    (void)sp2;
    for (i = 0; i < 4; i++) {
      int p[3], r[3], cc[3], vb[3];
      load[i] = 0;
      if (v->w[i].cnt == 0)
        continue;
      p[0] = v->w[i].pos[0];
      p[1] = v->w[i].pos[1];
      p[2] = 0;
      fx_rott(r, v->m, p);
      fx_cross(cc, v->st.w, r, 16);
      for (k = 0; k < 3; k++)
        cc[k] += v->st.vel[k];
      fx_rot(vb, v->m, cc);
      if (vb[2] >= 0)
        load[i] = (int)(((i64)vb[2] * v->damper) >> 8);
    }
    fx_accel(c, load, drag, 0, acc, aacc);
  }
  v->sb = v->st;
  fx_integrate(&v->sb, acc, aacc);
  for (k = 0; k < 3; k++) {
    v->st.w[k] = SGN(v->st.w[k]) != SGN(v->sb.w[k]) ? 0 : v->sb.w[k];
    v->st.vel[k] = SGN(v->st.vel[k]) != SGN(v->sb.vel[k]) ? 0 : v->sb.vel[k];
  }
  v->sa = v->st;
  fx_quat_deriv(&v->sa);
  FXDBG("damped");
  t1 = FX_NOW();
  g_fx_t[0] += (t1 - t0) & 0xFFFF;
  t0 = t1;
  fx_advance(c);
  FXDBG("advance");
  v->st = v->sb;
  t1 = FX_NOW();
  g_fx_t[1] += (t1 - t0) & 0xFFFF;
  t0 = t1;
  /* BrTyreDepthAll: each wheel's drop, and its mount held to [-0.4, 0] */
  for (i = 0; i < 4; i++) {
    FxWheel *w = &v->w[i];
    int f = -fx_probe(v, w);
    w->depth = f;
    if (f > 0)
      f = 0;
    w->pos[2] = f < -Q16(0.4) ? -Q16(0.4) : f;
    w->tr[0] = w->pos[0];
    w->tr[1] = w->pos[1];
    w->tr[2] = w->pos[2];
  }
  t1 = FX_NOW();
  g_fx_t[2] += (t1 - t0) & 0xFFFF;
  t0 = t1;
  fx_export(c);
  for (i = 0; i < 4; i++)
    if (s_fx[i].dirty && s_fx[i].car)
      fx_export(&s_fx[i]);
  /* the force lists as the game's step leaves them */
  *(TgrAddr *)((char *)car + 0x160) = tgr_addr32((char *)car + 0xBF0);
  for (i = 0; i < 4; i++)
    *(TgrAddr *)(c->wb[i] + 0x18) = 0;
  g_fx_t[3] += (FX_NOW() - t0) & 0xFFFF;
  if (s_dbg == 2)
    tgr_trace("fxchk", "%u walk-mismatch %u walk-far-missed %u broad-mismatch %u", g_fx_chk[0], g_fx_chk[1], g_fx_chk[2], g_fx_chk[3]);
}

/* ---- the draw matrices ---------------------------------------------------- */
/* r = a b (3x3, Q30) */
static void fx_mul3(int r[3][3], const int a[3][3], const int b[3][3])
{
  int i, j;
  for (i = 0; i < 3; i++)
    for (j = 0; j < 3; j++)
      r[i][j] = (int)(((i64)a[i][0] * b[0][j] + (i64)a[i][1] * b[1][j] + (i64)a[i][2] * b[2][j]) >> 30);
}
/* guRotateF about one axis (0 x, 1 y, 2 z) by an angle in radians (Q28) */
static void fx_rot_axis(int m[3][3], int axis, int a)
{
  int s, c, i, j, u = (axis + 1) % 3, w = (axis + 2) % 3;
  fx_sincos(a, &s, &c);
  for (i = 0; i < 3; i++)
    for (j = 0; j < 3; j++)
      m[i][j] = i == j ? Q30(1.0) : 0;
  m[u][u] = c;
  m[w][w] = c;
  m[u][w] = s;
  m[w][u] = -s;
}
/* a float matrix from a rotation (Q30) and a position (Q16) */
static void fx_mat_out(float *o, const int r[3][3], const int p[3])
{
  int i, j;
  for (i = 0; i < 3; i++) {
    for (j = 0; j < 3; j++)
      ((unsigned int *)o)[i * 4 + j] = q2f(r[i][j], 30);
    o[i * 4 + 3] = 0.0f;
    ((unsigned int *)o)[12 + i] = q2f(p[i], 16);
  }
  o[15] = 1.0f;
}
#define DEG_Q28(d) ((int)(((i64)(d) * QK(3.14159265358979 / 180.0, 28)) >> 16))   /* degrees Q16 to radians Q28 */

/* BrCarBuildMatrices, natively: the body rolled by its roll angle; each
 * wheel spun about its axle (the steered pair also turned, by twice the
 * steering angle) at its mount 0.254 lower in the body's frame */
void BrFxBuildMatrices(BrCar *car)
{
  static const unsigned char k_slot[4][2] = { { 0, 2 }, { 1, 3 }, { 2, 1 }, { 3, 0 } };  /* (wheel body, matrix) */
  FxCar *c = fx_of(car);
  FxMirror *v = &c->v;
  int S[3][3], R[3][3], T[3][3], U[3][3], i, k, p[3], q[3];
  fx_import(c, car);
  for (i = 0; i < 3; i++)
    for (k = 0; k < 3; k++)
      S[i][k] = v->m[i][k];
  fx_rot_axis(R, 0, DEG_Q28(v->roll));
  fx_mul3(T, R, (const int (*)[3])S);
  fx_mat_out(&car->mtx0[0][0], (const int (*)[3])T, v->m[3]);
  for (i = 0; i < 4; i++) {
    FxWheel *w = &v->w[k_slot[i][0]];
    fx_rot_axis(R, 1, DEG_Q28(w->angle));
    if (k_slot[i][0] >= 2) {                            /* (the steered pair) */
      fx_rot_axis(U, 2, w->steer << 13);
      fx_mul3(T, (const int (*)[3])R, (const int (*)[3])U);
      fx_mul3(R, (const int (*)[3])T, (const int (*)[3])S);
    } else {
      fx_mul3(T, (const int (*)[3])R, (const int (*)[3])S);
      memcpy(R, T, sizeof R);
    }
    q[0] = w->tr[0];
    q[1] = w->tr[1];
    q[2] = w->tr[2] + Q16(0.254);
    fx_rott(p, (const int (*)[3])S, q);
    p[0] += v->m[3][0];
    p[1] += v->m[3][1];
    p[2] += v->m[3][2];
    fx_mat_out(&car->wheelMtx[k_slot[i][1]][0][0], (const int (*)[3])R, p);
  }
}
