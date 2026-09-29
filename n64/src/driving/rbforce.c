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
  char pad114[0x1b4 - 0x114];
  int x1b4;                     /* 0x1B4  on a wheel: it is on the ground */
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

