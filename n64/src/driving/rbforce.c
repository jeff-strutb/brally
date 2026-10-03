/* rbforce.c -- forces acting on the rigid-body car model
 */
#include "tgr/common.h"

/* -- declarations -- */
typedef struct BrRbBody {       /* a rigid body with up to four attached */
  int x0;
  struct BrRbBody *sub[4];      /* 0x04 */
  char pad14[0x18 - 0x14];
  struct BrRbForce *forces;     /* 0x18  applied forces, linked */
  int kind;                     /* 0x1C  2: does not rotate */
  char pad20[0x2c - 0x20];
  float mass;                   /* 0x2C */
  char pad30[0x54 - 0x30];
  float Iinv[3][3];             /* 0x54  inverse inertia */
  float f78[3];                 /* 0x78  (an attachment point, on attached bodies) */
  float vel[3];                 /* 0x84 */
  char pad90[0xa0 - 0x90];
  float angVel[3];              /* 0xA0 */
  float padac[4];
  float m[4][4];                /* 0xBC  orientation */
  float force[3];               /* 0xFC  accumulated this step */
  float torque[3];              /* 0x108 */
  char pad114[0x19c - 0x114];
  int x19c;                     /* 0x19C  on a wheel: its contact record */
  unsigned char surface;        /* 0x1A0  on a wheel: the surface under it */
  char pad1a1[0x1a4 - 0x1a1];
  float n[3];                   /* 0x1A4  on a wheel: the ground normal */
  char pad1b0[0x1b4 - 0x1b0];
  int x1b4;                     /* 0x1B4  on a wheel: it is on the ground */
  char pad1b8[0x1c0 - 0x1b8];
  float steer;                  /* 0x1C0  on a wheel: steering angle */
  float spin;                   /* 0x1C4  on a wheel: spin rate */
  float inertia;                /* 0x1C8  on a wheel */
  float drive;                  /* 0x1CC  on a wheel: drive torque */
  float brake;                  /* 0x1D0  on a wheel: brake torque */
  float angle;                  /* 0x1D4  on a wheel: display angle, degrees; on the car: the visual roll */
  char pad1d8[0x1fd - 0x1d8];
  unsigned char tyres;          /* 0x1FD  on the car: the tyre compound, 1-3 */
  char pad1fe[0x204 - 0x1fe];
  unsigned char slide;          /* 0x204  on the car: 0x80 while sliding */
} BrRbBody;
void BrRbAddWheelForces(BrRbBody *b, BrRbBody *w);
void BrRbSolveAccel(BrRbBody *b);
void func_802586C0(float out[3], float m[4][4], float v[3]);   /* v into the body frame */
void func_80258758(float out[3], float m[4][4], float v[3]);   /* and back out */
void BrMat3MulVec(float out[3], float m[3][3], float v[3]);
typedef struct { float v[3]; } BrRbVec;
typedef struct BrRbForce {      /* a force applied to a body */
  struct BrRbForce *next;       /* 0x00 */
  int frame;                    /* 0x04  0: world axes, 1: body axes */
  float f[3];                   /* 0x08 */
  float at[3];                  /* 0x14  where, in body axes */
} BrRbForce;
void BrRbAddForces(BrRbBody *b);
void BrRbAddForce(BrRbBody *b, BrRbForce *a);
void func_802607DC();
/* -- end declarations -- */

/* WHAT IT DOES: Turn a body's accumulated force and torque into
 * accelerations: force over mass, and (unless the body does not rotate)
 * torque taken into the body frame, through the inverse inertia and back
 * out. */
/* @implements 0x80258950 tgr BrRbAccel */
void BrRbAccel(BrRbBody *b)
{
  float bodyT[3];
  float acc[3];

  b->force[0] *= 1.0f / b->mass;
  b->force[1] *= 1.0f / b->mass;
  b->force[2] *= 1.0f / b->mass;
  if (b->kind != 2) {
    func_802586C0(bodyT, b->m, b->torque);
    BrMat3MulVec(acc, (float (*)[3])b->Iinv, bodyT);
    func_80258758(b->torque, b->m, acc);
  }
}

/* WHAT IT DOES: Add one applied force into a body's accumulators: the force
 * (given in world or body axes) into the force sum and, unless the body
 * does not rotate, its moment about the body's centre into the torque sum;
 * then two debug prints: the force record, and the body's sums.
 * RESIDUE (32): FP registers one off from the case-0 copy on (the ROM's
 * join loads f[0] first; ours loads the force sum first) -- the class of
 * BrRbAddForces; add spellings, if/switch and a struct-typed f leave it. */
/* @implements 0x802589F4 tgr BrRbAddForce */
void BrRbAddForce(BrRbBody *b, BrRbForce *a)
{
  float r[3];
  float f[3];
  float t[3];

  switch (a->frame) {
  case 0:
    f[0] = a->f[0];
    f[1] = a->f[1];
    f[2] = a->f[2];
    break;
  case 1:
    func_80258758(f, b->m, a->f);
    break;
  }
  b->force[0] = f[0] + b->force[0];
  b->force[1] = f[1] + b->force[1];
  b->force[2] = f[2] + b->force[2];
  if (b->kind != 2) {
    func_80258758(r, b->m, a->at);
    t[0] = r[1] * f[2] - f[1] * r[2];
    t[1] = r[2] * f[0] - f[2] * r[0];
    t[2] = r[0] * f[1] - f[0] * r[1];
    b->torque[0] = t[0] + b->torque[0];
    b->torque[1] = t[1] + b->torque[1];
    b->torque[2] = t[2] + b->torque[2];
  }
  func_802607DC("Force = %10.4f, %10.4f, %10.4f, %10.4f, %10.4f, %10.4f\n",
                a->f[0], a->f[1], a->f[2], a->at[0], a->at[1], a->at[2]);
  func_802607DC("F/T = %10.4f, %10.4f, %10.4f, %10.4f, %10.4f, %10.4f\n",
                b->force[0], b->force[1], b->force[2], b->torque[0], b->torque[1], b->torque[2]);
}

/* WHAT IT DOES: Apply every force attached to a rigid body for this step:
 * walks the body's list of force records and adds each one in turn. */
/* @implements 0x80258BDC tgr BrRbApplyForces */
void BrRbApplyForces(int param_1)
{
  int *piVar1;
  
  for (piVar1 = *(int **)(param_1 + 0x18); piVar1 != (int *)0x0; piVar1 = (int *)*piVar1) {
    BrRbAddForce(param_1,piVar1);
  }
}

/* WHAT IT DOES: Does nothing with its argument. An empty rigid-body hook
 * the physics still calls. */
/* @implements 0x80258070 tgr BrStub80258070 */
void BrStub80258070(int arg0)
{
}

/* WHAT IT DOES: Does nothing with its argument. A second empty rigid-body
 * hook the physics still calls. */
/* @implements 0x80258078 tgr BrStub80258078 */
void BrStub80258078(int arg0)
{
}

/* WHAT IT DOES: Always returns 1 and ignores its three arguments. A
 * placeholder physics callback nothing calls directly. */
/* @implements 0x8025BBA4 tgr BrStub8025BBA4 */
int BrStub8025BBA4(int arg0,int arg1,int arg2)
{
  return 1;
}

/* WHAT IT DOES: Always returns 0 and ignores its argument. A placeholder
 * physics callback nothing calls directly. */
/* @implements 0x8025D35C tgr BrStub8025D35C */
int BrStub8025D35C(int arg0)
{
  return 0;
}

/* WHAT IT DOES: The world velocity of a body at another body's offset
 * taken flat (its z dropped): the offset goes into the body frame, the
 * spin crossed with it is added to the linear velocity, and the sum is
 * turned back by the body's rotation. */
/* @implements 0x80259A18 tgr BrRbVelAtFlatPoint */
void BrRbVelAtFlatPoint(float out[3], BrRbBody *b, BrRbBody *at)
{
  float p[3];
  float c[3];
  float r[3];

  p[0] = at->f78[0];
  p[1] = at->f78[1];
  p[2] = 0.0f;
  func_80258758(r, b->m, p);
  out[0] = b->vel[0];
  out[1] = b->vel[1];
  out[2] = b->vel[2];
  c[0] = b->angVel[1] * r[2] - b->angVel[2] * r[1];
  c[1] = b->angVel[2] * r[0] - b->angVel[0] * r[2];
  c[2] = b->angVel[0] * r[1] - b->angVel[1] * r[0];
  p[0] = out[0] + c[0];
  p[1] = out[1] + c[1];
  p[2] = out[2] + c[2];
  func_802586C0(out, b->m, p);
}

/* WHAT IT DOES: The velocity of a body at an attached body's attachment
 * point: the point taken into world axes, then the body's velocity plus
 * its angular velocity crossed with that offset.  The PC twin is
 * BrRbVelAtBodyPoint.
 */
/* @implements 0x80259B1C tgr BrRbVelAtBodyPoint */
void BrRbVelAtBodyPoint(float out[3], BrRbBody *b, BrRbBody *at)
{
  float p[3];
  float c[3];
  float r[3];

  p[0] = at->f78[0];
  p[1] = at->f78[1];
  p[2] = at->f78[2];
  func_80258758(r, b->m, p);
  out[0] = b->vel[0];
  out[1] = b->vel[1];
  out[2] = b->vel[2];
  c[0] = b->angVel[1] * r[2] - b->angVel[2] * r[1];
  c[1] = b->angVel[2] * r[0] - b->angVel[0] * r[2];
  c[2] = b->angVel[0] * r[1] - b->angVel[1] * r[0];
  out[0] = out[0] + c[0];
  out[1] = out[1] + c[1];
  out[2] = out[2] + c[2];
}

/* WHAT IT DOES: The velocity of a body at a point given in its own axes. */
/* @implements 0x80259C18 tgr BrRbVelAtPoint */
void BrRbVelAtPoint(float out[3], BrRbBody *b, float *pt)
{
  float p[3];
  float c[3];
  float r[3];

  p[0] = pt[0];
  p[1] = pt[1];
  p[2] = pt[2];
  func_80258758(r, b->m, p);
  out[0] = b->vel[0];
  out[1] = b->vel[1];
  out[2] = b->vel[2];
  c[0] = b->angVel[1] * r[2] - b->angVel[2] * r[1];
  c[1] = b->angVel[2] * r[0] - b->angVel[0] * r[2];
  c[2] = b->angVel[0] * r[1] - b->angVel[1] * r[0];
  out[0] = out[0] + c[0];
  out[1] = out[1] + c[1];
  out[2] = out[2] + c[2];
}

/* -- declarations: BrCarAxleGrip -- */
extern int D_8028C800;                  /* the weather (difficulty row + 1) */
extern float D_802A4A38[24];            /* grip by weather row and surface */
extern float D_802A4A98[24];            /* the lateral speed above which grip falls */
extern float D_802A4AF8[24];            /* and below which it is full */
float sqrtf(float x);
float sinf(float x);
float cosf(float x);
#define ABS(x) ((x) < 0 ? -(x) : (x))
#define SIGN(x) ((x) == 0 ? 0.0 : ((x) > 0 ? 1.0 : -1.0))
/* -- end declarations -- */

/* WHAT IT DOES: The axle constraint on a car body: each axle's drive slip
 * from its driven wheel (spin over radius, against the mass, clamped to
 * 1.5); then, for the front axle with a wheel down, the axle's sideways
 * velocity is cut by a grip factor -- from the surface pair and weather
 * row's table, falling off between two lateral speeds, less on worn tyres,
 * more when steering straight, more at low speed -- once the lateral load
 * passes a hold threshold (lower while already sliding), flagging the
 * slide; then the same for the rear axle along its steered heading; the
 * two axle velocities set the body's forward, lateral and yaw velocity
 * (when either axle ran), and the visual roll eases toward the side
 * force.  The PC twin is BrCarPhysDriveMatch (br_cardrive.c).
 * RESIDUE (976): the ROM keeps the body pointer and most locals in its
 * 0x158 frame (reloading the parameter after every store) while still
 * allocating some floats across blocks; ours allocates normally.  It is
 * not the -Olimit fallback (that gives 1282 instructions to the ROM's 993)
 * nor -O1.  The frame has a dead {1, 0, 0} initialised array (at 0x98,
 * from .data 0x802A4B58) in the slide-flag block. */
/* @implements 0x80259D14 tgr BrCarAxleGrip */
void BrCarAxleGrip(BrRbBody *b, float dt, float *gripF, float *gripR, unsigned char *slipFp,
                   unsigned char *slipRp)
{
  float tmpB[3];
  float tmpA[3];
  float pt[3];
  float vB[3];
  float vA[3];
  float w[3];
  float sv[3];
  float side;
  int ran;
  unsigned char sC;
  unsigned char sD;
  unsigned char sA;
  unsigned char sB;
  float slipF;
  float slipR;
  float g;
  short row;
  float hold;
  float s;
  float v;
  float m4;
  float sp;
  float t;
  int idx;
  float save[3];
  float lat[3];

  ran = 0;
  side = 0.0f;
  if (b->sub[0]->x19c == 0) {
    b->sub[0]->x1b4 = 0;
  }
  if (b->sub[1]->x19c == 0) {
    b->sub[1]->x1b4 = 0;
  }
  if (b->sub[2]->x19c == 0) {
    b->sub[2]->x1b4 = 0;
  }
  if (b->sub[3]->x19c == 0) {
    b->sub[3]->x1b4 = 0;
  }
  slipF = SIGN(b->sub[2]->spin) * -ABS(b->sub[2]->brake) * 2.0;
  slipF /= b->sub[2]->inertia;
  slipR = SIGN(b->sub[0]->spin) * -ABS(b->sub[0]->brake) * 2.0;
  slipR /= b->sub[0]->inertia;
  m4 = b->mass / 4.0f;
  slipF /= m4;
  slipR /= m4;
  slipF *= dt * dt;
  slipR *= dt * dt;
  if (ABS(slipF) > 1.0f) {
    slipF = SIGN(slipF) * 1.5;
  }
  if (ABS(slipR) > 1.0f) {
    slipR = SIGN(slipR) * 1.5;
  }
  pt[2] = 0.0f;
  pt[1] = 0.0f;
  pt[0] = b->sub[0]->f78[0];
  BrRbVelAtPoint(tmpA, b, pt);
  func_802586C0(vA, b->m, tmpA);
  sA = b->sub[0]->surface;
  sB = b->sub[1]->surface;
  sC = b->sub[2]->surface;
  sD = b->sub[3]->surface;
  row = D_8028C800 - 1;
  if (row >= 3 || row < 0) {
    row = 0;
  }
  b->slide = 0;
  row <<= 3;
  if ((b->sub[0]->x1b4 == 0 && b->sub[1]->x1b4 == 0) || (b->sub[2]->x1b4 == 0 && b->sub[3]->x1b4 == 0)) {
    *slipFp = 0;
  } else {
    ran = 1;
    hold = 8000.0f;
    s = ABS(*gripF) + ABS(vA[1]) * b->mass / dt;
    if (ABS(slipF) > 0.0001) {
      s += 10000.0f * (ABS(slipR) > 0.0001);
    } else {
      s += 100000.0f * (ABS(slipR) > 0.0001);
    }
    if (*slipFp != 0) {
      hold = 5600.0f;
    }
    *slipFp = 0;
    idx = ((sA + sB + 1) >> 1) + row;
    v = s;
    if (D_802A4A98[idx] < s) {
      v = D_802A4A98[idx];
    }
    if (v < D_802A4AF8[idx]) {
      v = D_802A4AF8[idx];
    }
    g = D_802A4AF8[idx] / v * 20.0f * (float)(D_802A4A38[idx] - 0.002 * (b->tyres - 1));
    if (b->sub[2]->steer == 0) {
      g = g * 1.5;
    }
    if (ABS(g) > 1.0f) {
      g = 1.0f;
    }
    if (s < hold) {
      side = 0.0f;
      vA[1] = 0.0f;
    } else {
      sp = sqrtf(b->vel[0] * b->vel[0] + b->vel[1] * b->vel[1] + b->vel[2] * b->vel[2]);
      if (sp < 27.0f) {
        t = (27.0f - sp) * 0.1f / 27.0f;
        if (g < t) {
          g = t;
        }
      }
      vA[1] = vA[1] - vA[1] * g;
      side = vA[1] * g;
      *slipFp = 1;
    }
    {
      float fwd[3] = { 1.0f, 0.0f, 0.0f };

      if (ABS(vA[0]) > 1.0f) {
        if (ABS(vA[1] / vA[0]) > 0.25f) {
          b->slide = 0x80;
        }
      } else if (ABS(vA[1]) > 1.0f) {
        b->slide = 0x80;
      } else {
        b->slide = 0;
      }
    }
    t = vA[0];
    vA[0] = vA[0] - slipR;
    if (ABS(vA[0]) > 1e-05f) {
      if (SIGN(vA[0]) != SIGN(t)) {
        vA[0] = 0.0f;
      }
    }
  }
  pt[0] = b->sub[2]->f78[0];
  BrRbVelAtPoint(tmpB, b, pt);
  func_802586C0(vB, b->m, tmpB);
  if ((b->sub[2]->x1b4 != 0 || b->sub[3]->x1b4 != 0) && (b->sub[0]->x1b4 != 0 || b->sub[1]->x1b4 != 0)) {
    ran = 1;
    pt[0] = cosf(b->sub[2]->steer);
    pt[1] = sinf(b->sub[2]->steer);
    pt[2] = 0.0f;
    save[0] = vB[0];
    save[1] = vB[1];
    save[2] = vB[2];
    t = pt[0] * vB[0] + pt[1] * vB[1] + pt[2] * vB[2];
    vB[0] = pt[0] * t;
    vB[1] = pt[1] * t;
    vB[2] = pt[2] * t;
    lat[0] = save[0] - vB[0];
    lat[1] = save[1] - vB[1];
    lat[2] = save[2] - vB[2];
    sp = sqrtf(lat[0] * lat[0] + lat[1] * lat[1] + lat[2] * lat[2]);
    s = ABS(*gripR) + sp * b->mass / dt + 10000.0f * (ABS(slipF) > 0.0001);
    hold = 8000.0f;
    if (*slipRp != 0) {
      hold = hold * 0.7;
    }
    *slipRp = 0;
    if (hold < s) {
      idx = ((sC + sD + 1) >> 1) + row;
      v = s;
      if (D_802A4A98[idx] < s) {
        v = D_802A4A98[idx];
      }
      if (v < D_802A4AF8[idx]) {
        v = D_802A4AF8[idx];
      }
      g = D_802A4AF8[idx] / v * 20.0f * (D_802A4A38[idx] - 0.002f * (b->tyres - 1));
      if (b->sub[2]->steer == 0) {
        g = g * 1.5;
      }
      if (ABS(g) > 1.0f) {
        g = 1.0f;
      }
      sp = sqrtf(b->vel[0] * b->vel[0] + b->vel[1] * b->vel[1] + b->vel[2] * b->vel[2]);
      if (sp < 27.0f) {
        t = (27.0f - sp) * 0.1f / 27.0f;
        if (g < t) {
          g = t;
        }
      }
      lat[0] = lat[0] * g;
      lat[1] = lat[1] * g;
      lat[2] = lat[2] * g;
      vB[0] = save[0] - lat[0];
      vB[1] = save[1] - lat[1];
      vB[2] = save[2] - lat[2];
      *slipRp = 1;
    }
  }
  if (ran != 0) {
    sv[0] = (vA[0] + vB[0]) / 2.0f;
    sv[2] = (vA[1] - vB[1]) / (b->sub[0]->f78[0] - b->sub[2]->f78[0]);
    sv[1] = vA[1] - sv[2] * b->sub[0]->f78[0];
    func_802586C0(w, b->m, b->angVel);
    w[2] = sv[2];
    func_80258758(b->angVel, b->m, w);
    func_802586C0(w, b->m, b->vel);
    w[0] = sv[0];
    w[1] = sv[1];
    func_80258758(b->vel, b->m, w);
  }
  if (ABS(side) > 0.5f) {
    side = SIGN(side) * 0.5;
  }
  t = side / 0.5f * -4.0f;
  if (ABS(b->angle - t) < 0.26666668f) {
    b->angle = t;
  } else if (b->angle < t) {
    b->angle = b->angle + 0.26666668f;
  } else {
    b->angle = b->angle - 0.26666668f;
  }
}

/* WHAT IT DOES: The tyre of one wheel on the ground (its normal within 45
 * degrees or so of up): the rolling direction is the car's sideways axis
 * crossed into the contact plane and turned by the steer angle.  With all
 * four wheels down the drive torque becomes a force along it, capped by
 * the wheel's load (falling to a tenth past the cap) and weaker while the
 * slide flag is set; half the raw force is reported through pA, the force
 * is added to the wheel's force list, and the reaction and the ground
 * speed slow the wheel's spin; otherwise the wheel free-spins on its
 * torque.  The spin is clamped to +-300 and the display angle advanced and
 * wrapped into a turn.  The PC twin is BrCarPhysTyre (br_carphys.c).  The
 * locals follow the ROM frame (the {0, 1, 0} axis is initialised from
 * .data right after BrCarAxleGrip's).
 * RESIDUE (423): the cross products' load order and the spill temps (the
 * ROM uses two, 0x20/0x24); the axis sits at 0x50, the ROM's 0x58. */
/* @implements 0x8025AC9C tgr BrWheelTyre */
void BrWheelTyre(BrRbBody *b, BrRbBody *w, float *pA, unsigned char *pB, float dt)
{
  float a[3];
  float c[3];
  float d[3];
  float e[3];
  float side[3];
  float fwd[3];
  float v[3];
  float dot;
  float sn;
  float cs;
  float load;
  int u0;
  float q;
  int u1[2];
  float tq;
  int u2[3];
  float axis[3] = { 0.0f, 1.0f, 0.0f };

  if (w->x19c == 0) {
    return;
  }
  if (w->n[2] < 0.7) {
    return;
  }
  func_80258758(a, b->m, axis);
  c[0] = a[1] * w->n[2] - w->n[1] * a[2];
  c[1] = a[2] * w->n[0] - w->n[2] * a[0];
  c[2] = a[0] * w->n[1] - w->n[0] * a[1];
  d[0] = w->n[1] * c[2] - c[1] * w->n[2];
  d[1] = w->n[2] * c[0] - c[2] * w->n[0];
  d[2] = w->n[0] * c[1] - c[0] * w->n[1];
  cs = cosf(w->steer);
  sn = sinf(w->steer);
  side[0] = c[0] * -sn;
  side[1] = c[1] * -sn;
  side[2] = c[2] * -sn;
  fwd[0] = d[0] * sn + c[0] * cs;
  fwd[1] = d[1] * sn + c[1] * cs;
  fwd[2] = d[2] * sn + c[2] * cs;
  c[0] = d[0] * cs;
  c[1] = d[1] * cs;
  c[2] = d[2] * cs;
  side[0] = c[0] + side[0];
  side[1] = c[1] + side[1];
  side[2] = c[2] + side[2];
  if (b->sub[0]->x1b4 != 0 && b->sub[2]->x1b4 != 0 && b->sub[1]->x1b4 != 0 && b->sub[3]->x1b4 != 0) {
    BrRbVelAtBodyPoint(v, b, w);
    dot = fwd[2] * v[2] + v[0] * fwd[0] + v[1] * fwd[1];
    a[1] = 0.0f;
    a[0] = 0.0f;
    a[2] = (b->mass + w->mass * 4.0f) * 2.943f + (w->f78[2] - -0.97) * 0;
    tq = w->drive;
    q = tq / w->inertia;
    load = (w->n[2] * a[2] + w->n[0] * a[0] + w->n[1] * a[1]) * 3.5f;
    *pA = *pA + q / 2.0f;
    if (*pB != 0) {
      q = q * 0.9;
    }
    if (ABS(load) < ABS(q)) {
      load = load / q;
      if (load < 0) {
        load = -load;
      }
      q = q * load * 0.1;
    }
    e[2] = -q;
    e[0] = fwd[0] * e[2];
    e[1] = fwd[1] * e[2];
    e[2] = fwd[2] * e[2];
    func_802586C0(a, b->m, e);
    w->forces->f[0] = a[0] + w->forces->f[0];
    w->forces->f[1] = a[1] + w->forces->f[1];
    w->forces->f[2] = a[2] + w->forces->f[2];
    w->spin = w->spin + (tq - w->inertia * q) * dt;
    w->spin = w->spin - (w->spin * w->inertia + dot) * 0.4;
    if (ABS(w->spin) > 300.0f) {
      w->spin = SIGN(w->spin) * 300.0;
    }
  } else {
    w->spin = w->spin + w->drive * dt;
    if (ABS(w->spin) > 300.0f) {
      w->spin = SIGN(w->spin) * 300.0;
    }
  }
  w->angle = w->angle - w->spin * 57.29578f * dt;
  while (w->angle > 360.0) {
    w->angle = w->angle - 360.0;
  }
  if (w->angle < 0.0) {
    do {
      w->angle = w->angle + 360.0f;
    } while (w->angle < 0.0f);
  }
}

/* WHAT IT DOES: Turn a car body's summed force and torque into
 * accelerations: in the body frame the force (with its four wheels' forces
 * added on the two ground axes) is divided by the mass, the torque goes
 * through the inverse inertia, and both come back to the world frame.  The
 * PC twin is BrRbSolveAccel (br_rbaccum.c). */
/* @implements 0x8025980C tgr BrRbSolveAccel */
void BrRbSolveAccel(BrRbBody *b)
{
  float t[3];
  float spare[3];                 /* declared and unused: it only holds a stack slot */
  float u[3];
  float w[3];

  func_802586C0(t, b->m, b->force);
  b->force[0] = t[0];
  b->force[1] = t[1];
  b->force[2] = t[2];
  t[0] = (b->force[0] + b->sub[0]->force[0] + b->sub[1]->force[0] + b->sub[2]->force[0] +
          b->sub[3]->force[0]) / b->mass;
  t[1] = (b->force[1] + b->sub[0]->force[1] + b->sub[1]->force[1] + b->sub[2]->force[1] +
          b->sub[3]->force[1]) / b->mass;
  t[2] = b->force[2] / b->mass;
  func_80258758(b->force, b->m, t);
  func_802586C0(u, b->m, b->torque);
  BrMat3MulVec(w, b->Iinv, u);
  func_80258758(b->torque, b->m, w);
}

/* WHAT IT DOES: Add a body's applied forces into its accumulators: each
 * force (given in world or body axes; any other kind adds a stale value)
 * goes into the force sum and, unless the body does not rotate, its moment
 * about the body's centre into the torque sum.
 * RESIDUE (25): float temporaries rotate one register off from the
 * world-axes copy on (f10 vs f16); operand orders swept, 394 permuter
 * compiles leave it. */
/* @t4-pass 0x802594BC 1 2026-09-29 compiles 26 best 25 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x802594BC 2 2026-09-29 compiles 26 best 25 moved 0  (n64/tools/n64permute.py) */
/* @t3 0x802594BC */
/* @implements 0x802594BC tgr BrRbAddForces */
void BrRbAddForces(BrRbBody *b)
{
  BrRbForce *a;
  float f[3];
  float r[3];
  float t[3];

  for (a = b->forces; a != 0; a = a->next) {
    switch (a->frame) {
    case 0:
      f[0] = a->f[0];
      f[1] = a->f[1];
      f[2] = a->f[2];
      break;
    case 1:
      func_80258758(f, b->m, a->f);
      break;
    }
    b->force[0] = b->force[0] + f[0];
    b->force[1] = b->force[1] + f[1];
    b->force[2] = f[2] + b->force[2];
    if (b->kind != 2) {
      func_80258758(r, b->m, a->at);
      t[0] = r[1] * f[2] - f[1] * r[2];
      t[1] = r[2] * f[0] - f[2] * r[0];
      t[2] = r[0] * f[1] - f[0] * r[1];
      b->torque[0] = t[0] + b->torque[0];
      b->torque[1] = b->torque[1] + t[1];
      b->torque[2] = b->torque[2] + t[2];
    }
  }
}

/* WHAT IT DOES: Add a wheel body's applied forces: each force (world axes
 * are taken into the car body's axes, body axes are used as they are) goes
 * into the wheel's force sum, and while the wheel is on the ground the
 * moment of its flat (x, y) part about the wheel's mounting point goes into
 * the car body's torque.
 * RESIDUE (42): the float-register rotation of BrRbAddForces, from the
 * body-axes copy on; 298 permuter compiles leave it. */
/* @t4-pass 0x80259634 1 2026-09-29 compiles 26 best 42 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x80259634 2 2026-09-29 compiles 26 best 42 moved 0  (n64/tools/n64permute.py) */
/* @t3 0x80259634 */
/* @implements 0x80259634 tgr BrRbAddWheelForces */
void BrRbAddWheelForces(BrRbBody *b, BrRbBody *w)
{
  BrRbForce *a;
  float g[3];
  float flat[3];
  float f[3];
  float p[3];
  float r[3];
  float t[3];

  for (a = w->forces; a != 0; a = a->next) {
    if (a->frame == 0) {
      func_802586C0(f, b->m, a->f);
    }
    if (a->frame == 1) {
      f[0] = a->f[0];
      f[1] = a->f[1];
      f[2] = a->f[2];
    }
    flat[0] = f[0];
    flat[1] = f[1];
    flat[2] = 0.0f;
    func_80258758(g, b->m, flat);
    w->force[0] = w->force[0] + f[0];
    w->force[1] = f[1] + w->force[1];
    w->force[2] = w->force[2] + f[2];
    if (0 != w->x1b4) {
      p[0] = w->m[3][0];
      p[1] = w->m[3][1];
      p[2] = w->m[3][2];
      func_80258758(r, b->m, p);
      t[0] = r[1] * g[2] - g[1] * r[2];
      t[1] = r[2] * g[0] - g[2] * r[0];
      t[2] = r[0] * g[1] - g[0] * r[1];
      b->torque[0] = t[0] + b->torque[0];
      b->torque[1] = b->torque[1] + t[1];
      b->torque[2] = t[2] + b->torque[2];
    }
  }
}

/* WHAT IT DOES: Clear the force and torque accumulators of a car body and
 * of each of its four wheel bodies before the forces are summed again. */
/* @implements 0x8025993C tgr BrRbForcesClear */
void BrRbForcesClear(BrRbBody *b)
{
  b->force[2] = b->force[1] = b->force[0] = 0.0f;
  b->torque[2] = b->torque[1] = b->torque[0] = 0.0f;
  b->sub[0]->force[0] = 0.0f;
  b->sub[0]->force[1] = 0.0f;
  b->sub[0]->force[2] = 0.0f;
  b->sub[1]->force[0] = 0.0f;
  b->sub[1]->force[1] = 0.0f;
  b->sub[1]->force[2] = 0.0f;
  b->sub[2]->force[0] = 0.0f;
  b->sub[2]->force[1] = 0.0f;
  b->sub[2]->force[2] = 0.0f;
  b->sub[3]->force[0] = 0.0f;
  b->sub[3]->force[1] = 0.0f;
  b->sub[3]->force[2] = 0.0f;
  BrRbAddForces(b);
  BrRbAddWheelForces(b, b->sub[0]);
  BrRbAddWheelForces(b, b->sub[1]);
  BrRbAddWheelForces(b, b->sub[2]);
  BrRbAddWheelForces(b, b->sub[3]);
  BrRbSolveAccel(b);
}

