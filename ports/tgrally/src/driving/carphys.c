/* carphys.c -- the car's physical state
 */
#include "tgr/common.h"
#include "tgr/body.h"
#include "tgr/car.h"

/* -- declarations -- */
void BrMatToQuat(float *param_1,float *param_2);
void BrQuatToMat(float *param_1,float *param_2);
void guRotateF(float m[4][4], float a, float x, float y, float z);
void guMtxCatF(float m[4][4], float n[4][4], float r[4][4]);
void BrMat3MulVecRows(float out[3], float m[4][4], float v[3]);
void BrPerfMark(int bar, int r, int g, int b, int a);
void BrTyreSprings(void *body);
void BrWheelTyre(void *body, void *wheel, float *grip, unsigned char *slip, float dt);
void BrRbForcesClear(void *body);
void BrRbIntegrate(void *state, void *body, float dt);
void BrCarAxleGrip(void *body, float dt, float *gripF, float *gripR, unsigned char *slipF, unsigned char *slipR);
void BrRbQuatDerivative(void *state);
void BrTyreSkidCheck(void *body, void *force);
void BrTyreLoads(void *body);
void BrCarPhysAdvance(void *body);
void BrTyreDepthAll(void *body);
typedef BrRbBody BrCarWheel;     /* (tgr/body.h) */
typedef BrRbBody BrPhysBody;     /* (tgr/body.h) */
typedef struct BrCarMounts {    /* the wheel mounts in a car's model buffer */
  char pad00[0xEC];
  bef_t rearX;                  /* 0xEC */
  bef_t rearY;                  /* 0xF0 */
  char padf4[0xF8 - 0xF4];
  bef_t frontX;                 /* 0xF8 */
  bef_t frontY;                 /* 0xFC */
} BrCarMounts;                  /* (cartridge data: big-endian) */
void BrRbBodyInit(void *body);
void BrRbSetParams(void *force, float fx, float fy, float fz, float rx, float ry, float rz, int frame);
/* -- end declarations -- */

/* WHAT IT DOES: Place a car at a new position: copies the 4x4 placement
 * matrix into the car, sets its position and velocity from it and rebuilds
 * the car's orientation. */
/* @implements 0x80220150 tgr BrCarPlaceAt */
void BrCarPlaceAt(char *carp, float *mtx)
{
  BrCar *car = (BrCar *)carp;           /* port: its fields by name (the ROM's offsets from the car) */
  float *q;
  float x;
  float y;
  float z;
  float w;

  memcpy(car->mtx0, mtx, 0x40);
  BrMatToQuat(mtx, car->body[0].q);
  q = car->body[0].q;
  x = q[0];
  y = q[1];
  z = q[2];
  w = q[3];
  car->body[0].stA.q[0] = car->body[0].stB.q[0] = x;
  car->body[0].stA.q[1] = car->body[0].stB.q[1] = y;
  car->body[0].stA.q[2] = car->body[0].stB.q[2] = z;
  car->body[0].stA.q[3] = car->body[0].stB.q[3] = w;
  BrQuatToMat((void *)car->body[0].m, (void *)&car->body[0].st);
}


/* WHAT IT DOES: Advance one car's rigid-body physics by a frame: hang the
 * wheel force lists off the body and zero them, run the force generators
 * (springs, the four tyre passes at most once a frame, drive, skid, loads),
 * integrate the body's velocity and orientation into a scratch copy and the
 * live state with a sign-change damper between them, advance the car, keep
 * the suspension depths, and rebuild each wheel's matrix.  Ported from the
 * PC twin (BrCarPhysStep); tmp and u are declared and unused (the ROM frame
 * keeps their slots). */
/* @implements 0x80221940 tgr BrCarPhysStep */
void BrCarPhysStep(BrCar *car)
{
  BrRbState tmp;
  short i;
  int u[2];

  BrPerfMark(0, 0x80, 0x80, 0, 0xFF);
  car->body[0].forces = &car->forcerec[0];
  car->body[0].sub[0]->forces = &car->forcerec[12];
  car->body[0].sub[1]->forces = &car->forcerec[14];
  car->body[0].sub[2]->forces = &car->forcerec[13];
  car->body[0].sub[3]->forces = &car->forcerec[15];
  ((BrRbBody *)(car->body[0].sub[0]))->forces->f[0] = 0.0f;
  ((BrRbBody *)(car->body[0].sub[0]))->forces->f[1] = 0.0f;
  ((BrRbBody *)(car->body[0].sub[0]))->forces->f[2] = 0.0f;
  ((BrRbBody *)(car->body[0].sub[1]))->forces->f[0] = 0.0f;
  ((BrRbBody *)(car->body[0].sub[1]))->forces->f[1] = 0.0f;
  ((BrRbBody *)(car->body[0].sub[1]))->forces->f[2] = 0.0f;
  ((BrRbBody *)(car->body[0].sub[2]))->forces->f[0] = 0.0f;
  ((BrRbBody *)(car->body[0].sub[2]))->forces->f[1] = 0.0f;
  ((BrRbBody *)(car->body[0].sub[2]))->forces->f[2] = 0.0f;
  ((BrRbBody *)(car->body[0].sub[3]))->forces->f[0] = 0.0f;
  ((BrRbBody *)(car->body[0].sub[3]))->forces->f[1] = 0.0f;
  ((BrRbBody *)(car->body[0].sub[3]))->forces->f[2] = 0.0f;
  BrTyreSprings(((char *)&(car)->body[0]));
  if (*(int *)((char *)&(car)->pade44[0x10]) == 0) {
    *(float *)((char *)&(car)->pade44[0x0]) = 0.0f;
    *(float *)((char *)&(car)->pade44[0x8]) = 0.0f;
    *(unsigned char *)((char *)&(car)->pade44[0xC]) = 0;
    BrWheelTyre(((char *)&(car)->body[0]), ((BrRbBody *)(car->body[0].sub[0])), (float *)((char *)&(car)->pade44[0x8]), (unsigned char *)((char *)&(car)->pade44[0xC]), 0.033333335f);
    BrWheelTyre(((char *)&(car)->body[0]), ((BrRbBody *)(car->body[0].sub[1])), (float *)((char *)&(car)->pade44[0x8]), (unsigned char *)((char *)&(car)->pade44[0xC]), 0.033333335f);
    BrWheelTyre(((char *)&(car)->body[0]), ((BrRbBody *)(car->body[0].sub[2])), (float *)((char *)&(car)->pade44[0x0]), (unsigned char *)((char *)&(car)->pade44[0x4]), 0.033333335f);
    BrWheelTyre(((char *)&(car)->body[0]), ((BrRbBody *)(car->body[0].sub[3])), (float *)((char *)&(car)->pade44[0x0]), (unsigned char *)((char *)&(car)->pade44[0x4]), 0.033333335f);
  }
  *(int *)((char *)&(car)->pade44[0x10]) = 0;
  *(float *)((char *)&(car)->body[0].force[0]) = 0.0f;
  *(float *)((char *)&(car)->body[0].force[1]) = 0.0f;
  *(float *)((char *)&(car)->body[0].force[2]) = 0.0f;
  *(float *)((char *)&(car)->body[0].torque[0]) = 0.0f;
  *(float *)((char *)&(car)->body[0].torque[1]) = 0.0f;
  *(float *)((char *)&(car)->body[0].torque[2]) = 0.0f;
  BrRbForcesClear(((char *)&(car)->body[0]));
  BrRbIntegrate(&car->body[0].st, ((char *)&(car)->body[0]), 0.033333335f);
  BrCarAxleGrip(((char *)&(car)->body[0]), 0.033333335f, (float *)((char *)&(car)->pade44[0x8]), (float *)((char *)&(car)->pade44[0x0]),
                (unsigned char *)((char *)&(car)->pade44[0xC]), (unsigned char *)((char *)&(car)->pade44[0x4]));
  BrRbQuatDerivative(&car->body[0].st);
  car->body[0].forces = &car->forcerec[4];
  ((BrRbBody *)(car->body[0].sub[0]))->forces = 0;
  ((BrRbBody *)(car->body[0].sub[2]))->forces = 0;
  ((BrRbBody *)(car->body[0].sub[1]))->forces = 0;
  ((BrRbBody *)(car->body[0].sub[3]))->forces = 0;
  BrTyreSkidCheck(((char *)&(car)->body[0]), ((char *)&(car)->forcerec[11]));
  BrTyreLoads(((char *)&(car)->body[0]));
  *(float *)((char *)&(car)->body[0].force[0]) = 0.0f;
  *(float *)((char *)&(car)->body[0].force[1]) = 0.0f;
  *(float *)((char *)&(car)->body[0].force[2]) = 0.0f;
  *(float *)((char *)&(car)->body[0].torque[0]) = 0.0f;
  *(float *)((char *)&(car)->body[0].torque[1]) = 0.0f;
  *(float *)((char *)&(car)->body[0].torque[2]) = 0.0f;
  BrRbForcesClear(((char *)&(car)->body[0]));
  memcpy((char *)&car->body[0].stB, (char *)&car->body[0].st, sizeof(BrRbState));
  BrRbIntegrate(&car->body[0].stB, ((char *)&(car)->body[0]), 0.033333335f);
  for (i = 0; i < 3; i++) {
    if (((&car->body[0].st.angVel.x)[i] == 0.0f ? 0.0f : ((&car->body[0].st.angVel.x)[i] > 0.0f ? 1.0f : -1.0f)) !=
        ((&car->body[0].stB.angVel.x)[i] == 0.0f ? 0.0f : ((&car->body[0].stB.angVel.x)[i] > 0.0f ? 1.0f : -1.0f))) {
      (&car->body[0].st.angVel.x)[i] = 0.0f;
    } else {
      (&car->body[0].st.angVel.x)[i] = (&car->body[0].stB.angVel.x)[i];
    }
    if (((&car->body[0].st.vel.x)[i] == 0.0f ? 0.0f : ((&car->body[0].st.vel.x)[i] > 0.0f ? 1.0f : -1.0f)) !=
        ((&car->body[0].stB.vel.x)[i] == 0.0f ? 0.0f : ((&car->body[0].stB.vel.x)[i] > 0.0f ? 1.0f : -1.0f))) {
      (&car->body[0].st.vel.x)[i] = 0.0f;
    } else {
      (&car->body[0].st.vel.x)[i] = (&car->body[0].stB.vel.x)[i];
    }
  }
  memcpy((char *)&car->body[0].stA, (char *)&car->body[0].st, sizeof(BrRbState));
  BrRbQuatDerivative(&car->body[0].stA);
  BrRbQuatDerivative(&car->body[0].stA);
  BrCarPhysAdvance(((char *)&(car)->body[0]));
  memcpy((char *)&car->body[0].st, (char *)&car->body[0].stB, sizeof(BrRbState));
  BrTyreDepthAll(((char *)&(car)->body[0]));
  BrQuatToMat((float *)((BrRbBody *)(car->body[0].sub[0]))->mtx, (float *)((BrRbBody *)(car->body[0].sub[0]))->state);
  BrQuatToMat((float *)((BrRbBody *)(car->body[0].sub[1]))->mtx, (float *)((BrRbBody *)(car->body[0].sub[1]))->state);
  BrQuatToMat((float *)((BrRbBody *)(car->body[0].sub[2]))->mtx, (float *)((BrRbBody *)(car->body[0].sub[2]))->state);
  BrQuatToMat((float *)((BrRbBody *)(car->body[0].sub[3]))->mtx, (float *)((BrRbBody *)(car->body[0].sub[3]))->state);
  BrPerfMark(0, 0, 0x80, 0, 0xFF);
}

/* -- declarations: BrCarDriveInput -- */
typedef struct BrDrivePad {     /* the pad record as the drive input reads it */
  unsigned int flags;           /* 0x00  0x10000 accelerate, 0x20000 brake, 0x40000 handbrake,
                                 *       0x100000/0x200000 gear up/down */
  char pad04[0x20 - 0x04];
  float steer;                  /* 0x20 */
  char pad24;
  unsigned char kind;           /* 0x25  4: a steering wheel */
} BrDrivePad;
extern float D_8028B720;                /* this frame's steering lock, degrees */
extern float D_8028B724;                /* the steering slew per frame */
extern float D_8028B728;                /* the lock at the centre of the stick */
extern float D_8028B72C;                /* lock lost at 90 forward */
extern short D_8028B730;
extern int D_8026FF18;                  /* game mode */
float sqrtf(float x);
void BrMat4RotateVec(float out[3], float m[4][4], BrVec3 *v);
void BrPadConsume(BrDrivePad *pad, int bit);
/* -- end declarations -- */

#define SGN(x) ((x) == 0.0f ? 0.0f : ((x) > 0.0f ? 1.0f : -1.0f))

/* WHAT IT DOES: Turn a car's pad into its drive inputs for the frame.  A
 * steering wheel sets the wheel angle straight from the wheel (a 7/8 power
 * curve, 10 degrees of lock).  A stick is dead-zoned and gives a steering
 * target within a lock that narrows with forward speed (the handling mode
 * picks the lock, its fall-off and the slew rate); past three quarters it
 * is full lock, and in between the lock also narrows with the car's speed
 * when the mode allows; the wheel angle then slews towards the target, at
 * once back through the centre.  Then the gearbox (automatic on the
 * engine speed, or the gear buttons), the engine force from the torque
 * curve (in two-player arcade the car behind gets up to 30 more), and the
 * engine speed from the driven wheels' spin through the gear ratio (or
 * revved freely in neutral), clamped and slewed by at most 400; the
 * handbrake sets the brake force.  The PC twin is BrCtlInputApply (a
 * different tuning of the same idea). */
/* @implements 0x80222050 tgr BrCarDriveInput */
void BrCarDriveInput(BrCar *car)
{
  float rate;
  float s;
  float t;
  float a;
  float sp;
  float lim;
  float v[3];
  float x;
  float lo;
  float tgt;
  int ctlt;
  float old;
  unsigned int flags;
  short k;
  int g;

  s = ((BrDrivePad *)((unsigned int *)(car->pad)))->steer;
  if (((BrDrivePad *)((unsigned int *)(car->pad)))->kind == 4) {
    a = s < 0.0f ? -s : s;
    car->xdf0 = -(SGN(s) * sqrtf(sqrtf(a) * sqrtf(sqrtf(a) * a))) * 10.0f * 3.1415927f / 180.0f;
  } else {
    if (0.0f < s) {
      s = s - 0.07f;
      if (s < 0.0f) {
        s = 0;
      }
    } else {
      s = s + 0.07f;
      if (0.0f < s) {
        s = 0;
      }
    }
    sp = car->xfe4[0];
    sqrtf(car->body[0].st.vel.z * car->body[0].st.vel.z + (car->body[0].st.vel.x * car->body[0].st.vel.x + car->body[0].st.vel.y * car->body[0].st.vel.y));
    BrMat4RotateVec(v, car->body[0].m, &car->body[0].st.vel);
    t = v[0];
    if (t < 10.0f) {
      t = 10.0f;
    }
    if (70.0f < t) {
      t = 70.0f;
    }
    if (sp < 140.0f) {
      sp = 140.0f;
    }
    if (340.0f < sp) {
      sp = 340.0f;
    }
    sp -= 140.0f;
    k = 1;
    switch (car->xe68) {
    case 0:
      D_8028B720 = 14.0f;
      D_8028B724 = 0.01f;
      D_8028B728 = 6.0f;
      D_8028B72C = 10.0f;
      D_8028B730 = 0;
      k = D_8028B730;
      x = D_8028B728;
      break;
    case 1:
      D_8028B720 = 14.0f;
      D_8028B724 = 0.01f;
      D_8028B728 = 6.0f;
      D_8028B72C = 10.0f;
      D_8028B730 = 0;
      k = D_8028B730;
      x = 3.0f;
      break;
    case 2:
      D_8028B720 = 17.0f;
      D_8028B724 = 0.1f;
      D_8028B728 = 16.0f;
      D_8028B72C = 14.0f;
      D_8028B730 = 0;
      k = D_8028B730;
      x = D_8028B728;
      break;
    }
    rate = D_8028B724;
    lim = D_8028B720 - t / 90.0f * D_8028B72C;
    if ((s < 0.0f ? -s : s) < 0.001f) {
      tgt = 0.0f;
      t = lim * 3.1415927f / 180.0f;
      lo = -lim * 3.1415927f / 180.0f;
    } else if (s < -0.75f) {
      t = lim * 3.1415927f / 180.0f;
      lo = -lim * 3.1415927f / 180.0f;
      tgt = t;
    } else if (0.75f < s) {
      lo = -lim * 3.1415927f / 180.0f;
      t = lim * 3.1415927f / 180.0f;
      tgt = lo;
    } else {
      x = x - k * (x / 2) * (sp / 200.0f);
      tgt = -s * (x * 3.1415927f / 180.0f);
      t = lim * 3.1415927f / 180.0f;
      lo = -lim * 3.1415927f / 180.0f;
    }
    if (t < tgt) {
      tgt = t;
    }
    if (tgt < lo) {
      tgt = lo;
    }
    k = SGN(tgt) != SGN(car->xdf0) || (0.0f < car->xdf0 && tgt < car->xdf0) || (car->xdf0 < 0.0f && car->xdf0 < tgt);
    if (car->xdf0 == 0.0f) {
      k = 0;
      *(signed char *)((char *)&(car)->pade44[0xD]) = 0;
    }
    g = tgt < car->xdf0;
    ctlt = car->xdf0 < tgt;
    if (g && *(signed char *)((char *)&(car)->pade44[0xD]) < 0) {
      k = 1;
    }
    if (ctlt && *(signed char *)((char *)&(car)->pade44[0xD]) > 0) {
      k = 1;
    }
    if (k) {
      *(signed char *)((char *)&(car)->pade44[0xD]) = g ? -1 : 1;
      rate = 1.0f;
      if (SGN(car->xdf0) != SGN(tgt)) {
        tgt = 0.0f;
      }
    } else {
      *(signed char *)((char *)&(car)->pade44[0xD]) = 0;
    }
    if ((car->xdf0 < tgt ? -(car->xdf0 - tgt) : car->xdf0 - tgt) < rate) {
      car->xdf0 = tgt;
    } else if (tgt < car->xdf0) {
      car->xdf0 = car->xdf0 - rate;
    } else {
      car->xdf0 = car->xdf0 + rate;
    }
  }
  old = car->xdf4;
  if (car->xdf4 < 800.0f) {
    car->xdf4 = 800.0f;
  }
  if (car->xe30 != 0) {
    g = car->xe40;
    if (g >= 2 && car->xdf4 < 4000.0f) {
      car->xe40 = g - 1;
    } else if (g < car->xe28[0] && 6000.0f < car->xdf4 && !(((BrDrivePad *)((unsigned int *)(car->pad)))->flags & 0x20000)) {
      car->xe40 = g + 1;
    }
    flags = ((BrDrivePad *)((unsigned int *)(car->pad)))->flags;
  } else if (car->xe40 > 0 && (((BrDrivePad *)((unsigned int *)(car->pad)))->flags & 0x200000)) {
    BrPadConsume((BrDrivePad *)((unsigned int *)(car->pad)), 0x200000);
    car->xe40--;
    flags = ((BrDrivePad *)((unsigned int *)(car->pad)))->flags;
  } else {
    flags = ((BrDrivePad *)((unsigned int *)(car->pad)))->flags;
    if (car->xe40 < car->xe28[0] && (flags & 0x100000) && !(flags & 0x20000)) {
      BrPadConsume((BrDrivePad *)((unsigned int *)(car->pad)), 0x100000);
      car->xe40++;
      flags = ((BrDrivePad *)((unsigned int *)(car->pad)))->flags;
    }
  }
  lo = 0.0f;
  if (D_8026FF18 == 1 && car->xfac == 1) {
    a = (D_8031B760[car->slot ^ 1].xfa8 < car->xfa8 ? -(D_8031B760[car->slot ^ 1].xfa8 - car->xfa8)
                                                    : D_8031B760[car->slot ^ 1].xfa8 - car->xfa8) * 0.5f;
    lo = a - 18.0f;
    if (a < 18.0f) {
      lo = 0.0f;
    } else if (30.0f < lo) {
      lo = 30.0f;
    }
  }
  t = car->xe14 * car->xdf4 * car->xdf4 * car->xdf4 + car->xe18 * car->xdf4 * car->xdf4 +
      car->xe1c * car->xdf4 + car->xe20 + lo;
  if (car->xe30 == 0) {
    t = t * 1.03;
  }
  if (!(flags & 0x10000)) {
    t = 0.0f;
  }
  g = 0;
  car->xe38 = t * 7.0f;
  if (((struct BrCarLink *)(car->link))->flags & 1) {
    car->xe40 = 0;
  } else {
    g = car->xe40;
    if (g == 0) {
      car->xe40 = 1;
      g = 1;
    }
  }
  if (g != 0) {
    car->xdf4 = (-(*(float *)((char *)&(car)->body[2].spin) / 6.2831855f) * 60.0f * car->xe24) * ((float *)car->xdf8)[g];
  } else {
    if (((BrDrivePad *)((unsigned int *)(car->pad)))->flags & 0x10000) {
      car->xdf4 += 300.0f;
    } else {
      car->xdf4 -= 200.0f;
    }
    car->xe38 = 0.0f;
  }
  if (car->xdf4 < 0.0f) {
    car->xdf4 = -car->xdf4;
  }
  if (car->xdf4 < 900.0f) {
    car->xdf4 = 900.0f;
  }
  if (8000.0f < car->xdf4) {
    car->xdf4 = 8000.0f;
  }
  lim = car->xdf4 - old;
  if (400.0f < (lim < 0.0f ? -lim : lim)) {
    lim = SGN(lim) * 400.0f;
  }
  car->xdf4 = old + lim;
  car->xe3c = 0;
  if (((BrDrivePad *)((unsigned int *)(car->pad)))->flags & 0x40000) {
    car->xe3c = -140000.0f;
  }
}

#undef SGN

/* WHAT IT DOES: Set up a car's rigid bodies for a race: the chassis (a
 * 3.5 by 2 by 1.5 box of 1000 at 2 up, level, with its spring set from the
 * car's class and its damper), each wheel (a fixed body at its mount from
 * the model, 0.1 down, the right side mirrored) and the force lists (the
 * four corners then weight, a drag list, the wheels' tyre nodes on shared
 * weight nodes); the tyre pass is skipped on the first frame.  The PC
 * twin is BrCarPhysInit (a port-side reconstruction).
 * The wheel bodies' x1d8/x1c4 and the two trailing fields are cleared
 * with an integer 0 (a second zero constant, which leaves f22 free for the
 * 2.0 across the chassis init); each wheel sets pos[2] right after its
 * mount position; the link stores follow the ROM's order. */
/* @implements 0x80222D54 tgr BrCarPhysInit */
void BrCarPhysInit(BrCar *car)
{
  *(int *)((char *)&(car)->pade44[0x10]) = 1;
  *(int *)((char *)&(car)->xe40) = 0;
  *(float *)((char *)&(car)->xdf0) = 0.0f;
  *(float *)((char *)&(car)->xdf4) = 0.0f;
  if (car->model != 0) {
    (&(car)->body[0])->shape = 1;
    (&(car)->body[0])->h = 2.0f;
    (&(car)->body[0])->mass = 1000.0f;
    (&(car)->body[0])->w = 3.5f;
    (&(car)->body[0])->d = 1.5f;
    BrRbBodyInit((&(car)->body[0]));
    (&(car)->body[0])->pos[0] = 0.0f;
    (&(car)->body[0])->pos[1] = 0.0f;
    (&(car)->body[0])->pos[2] = 2.0f;
    (&(car)->body[0])->vel[0] = 0.0f;
    (&(car)->body[0])->vel[1] = 0.0f;
    (&(car)->body[0])->vel[2] = 0.0f;
    (&(car)->body[0])->q[1] = 0.0f;
    (&(car)->body[0])->q[2] = 0.0f;
    (&(car)->body[0])->q[3] = 0.0f;
    (&(car)->body[0])->omega[0] = 0.0f;
    (&(car)->body[0])->omega[1] = 0.0f;
    (&(car)->body[0])->omega[2] = 0.0f;
    (&(car)->body[0])->q[0] = 1.0f;
    BrQuatToMat((float *)(&(car)->body[0])->mtx, (&(car)->body[0])->pos);
    (&(car)->body[0])->spring = (car->xe64 * 4.0f + 20.0f) * 16000.0f;
    (&(car)->body[0])->damper = -3000.0f;
    (&(car)->body[1])->hit = 0;
    (&(car)->body[1])->shape = 2;
    (&(car)->body[1])->mass = 0.0f;
    (&(car)->body[1])->w = 0.0f;
    (&(car)->body[1])->h = 0.0f;
    (&(car)->body[1])->d = 0.0f;
    (&(car)->body[1])->x1d8 = 0;
    (&(car)->body[1])->x1c4 = 0;
    BrRbBodyInit((&(car)->body[1]));
    (&(car)->body[1])->pos[0] = BEF(((BrCarMounts *)((char *)(car->model)))->frontX);
    (&(car)->body[1])->pos[1] = BEF(((BrCarMounts *)((char *)(car->model)))->frontY);
    (&(car)->body[1])->pos[2] = -0.1f;
    (&(car)->body[1])->vel[2] = 0.0f;
    (&(car)->body[1])->vel[1] = 0.0f;
    (&(car)->body[1])->vel[0] = 0.0f;
    (&(car)->body[1])->q[3] = 0.0f;
    (&(car)->body[1])->q[2] = 0.0f;
    (&(car)->body[1])->q[1] = 0.0f;
    (&(car)->body[1])->q[0] = 1.0f;
    (&(car)->body[1])->omega[2] = 0.0f;
    (&(car)->body[1])->omega[1] = 0.0f;
    (&(car)->body[1])->omega[0] = 0.0f;
    BrQuatToMat((float *)(&(car)->body[1])->mtx, (&(car)->body[1])->pos);
    (&(car)->body[3])->x1d8 = 0;
    (&(car)->body[3])->hit = 0;
    (&(car)->body[3])->x1c4 = 0;
    (&(car)->body[3])->shape = 2;
    (&(car)->body[3])->mass = 0.0f;
    (&(car)->body[3])->w = 0.0f;
    (&(car)->body[3])->h = 0.0f;
    (&(car)->body[3])->d = 0.0f;
    BrRbBodyInit((&(car)->body[3]));
    (&(car)->body[3])->pos[0] = BEF(((BrCarMounts *)((char *)(car->model)))->frontX);
    (&(car)->body[3])->pos[1] = -BEF(((BrCarMounts *)((char *)(car->model)))->frontY);
    (&(car)->body[3])->pos[2] = -0.1f;
    (&(car)->body[3])->vel[2] = 0.0f;
    (&(car)->body[3])->vel[1] = 0.0f;
    (&(car)->body[3])->vel[0] = 0.0f;
    (&(car)->body[3])->q[3] = 0.0f;
    (&(car)->body[3])->q[2] = 0.0f;
    (&(car)->body[3])->q[1] = 0.0f;
    (&(car)->body[3])->q[0] = 1.0f;
    (&(car)->body[3])->omega[2] = 0.0f;
    (&(car)->body[3])->omega[1] = 0.0f;
    (&(car)->body[3])->omega[0] = 0.0f;
    BrQuatToMat((float *)(&(car)->body[3])->mtx, (&(car)->body[3])->pos);
    (&(car)->body[2])->x1d8 = 0;
    (&(car)->body[2])->hit = 0;
    (&(car)->body[2])->x1c4 = 0;
    (&(car)->body[2])->shape = 2;
    (&(car)->body[2])->mass = 0.0f;
    (&(car)->body[2])->w = 0.0f;
    (&(car)->body[2])->h = 0.0f;
    (&(car)->body[2])->d = 0.0f;
    BrRbBodyInit((&(car)->body[2]));
    (&(car)->body[2])->pos[0] = BEF(((BrCarMounts *)((char *)(car->model)))->rearX);
    (&(car)->body[2])->pos[1] = BEF(((BrCarMounts *)((char *)(car->model)))->rearY);
    (&(car)->body[2])->pos[2] = -0.1f;
    (&(car)->body[2])->vel[2] = 0.0f;
    (&(car)->body[2])->vel[1] = 0.0f;
    (&(car)->body[2])->vel[0] = 0.0f;
    (&(car)->body[2])->q[3] = 0.0f;
    (&(car)->body[2])->q[2] = 0.0f;
    (&(car)->body[2])->q[1] = 0.0f;
    (&(car)->body[2])->q[0] = 1.0f;
    (&(car)->body[2])->omega[2] = 0.0f;
    (&(car)->body[2])->omega[1] = 0.0f;
    (&(car)->body[2])->omega[0] = 0.0f;
    BrQuatToMat((float *)(&(car)->body[2])->mtx, (&(car)->body[2])->pos);
    (&(car)->body[4])->x1d8 = 0;
    (&(car)->body[4])->hit = 0;
    (&(car)->body[4])->x1c4 = 0;
    (&(car)->body[4])->shape = 2;
    (&(car)->body[4])->mass = 0.0f;
    (&(car)->body[4])->w = 0.0f;
    (&(car)->body[4])->h = 0.0f;
    (&(car)->body[4])->d = 0.0f;
    BrRbBodyInit((&(car)->body[4]));
    (&(car)->body[4])->pos[0] = BEF(((BrCarMounts *)((char *)(car->model)))->rearX);
    (&(car)->body[4])->pos[1] = -BEF(((BrCarMounts *)((char *)(car->model)))->rearY);
    (&(car)->body[4])->pos[2] = -0.1f;
    (&(car)->body[4])->vel[2] = 0.0f;
    (&(car)->body[4])->vel[1] = 0.0f;
    (&(car)->body[4])->vel[0] = 0.0f;
    (&(car)->body[4])->q[3] = 0.0f;
    (&(car)->body[4])->q[2] = 0.0f;
    (&(car)->body[4])->q[1] = 0.0f;
    (&(car)->body[4])->q[0] = 1.0f;
    (&(car)->body[4])->omega[2] = 0.0f;
    (&(car)->body[4])->omega[1] = 0.0f;
    (&(car)->body[4])->omega[0] = 0.0f;
    BrQuatToMat((float *)(&(car)->body[4])->mtx, (&(car)->body[4])->pos);
    car->body[0].sub[0] = ((BrRbBody *)((char *)&(car)->body[1]));
    car->body[0].sub[1] = ((BrRbBody *)((char *)&(car)->body[3]));
    car->body[0].sub[2] = ((BrRbBody *)((char *)&(car)->body[2]));
    car->body[0].sub[3] = ((BrRbBody *)((char *)&(car)->body[4]));
    BrRbSetParams(((char *)&(car)->forcerec[0]), 0.0f, 0.0f, 0.0f, -1.5f, -1.0f, 0.0f, 1);
    BrRbSetParams(((char *)&(car)->forcerec[2]), 0.0f, 0.0f, 0.0f, -1.5f, 1.0f, 0.0f, 1);
    BrRbSetParams(((char *)&(car)->forcerec[1]), 0.0f, 0.0f, 0.0f, 1.5f, -1.0f, 0.0f, 1);
    BrRbSetParams(((char *)&(car)->forcerec[3]), 0.0f, 0.0f, 0.0f, 1.5f, 1.0f, 0.0f, 1);
    BrRbSetParams(((char *)&(car)->forcerec[8]), 0.0f, 0.0f, -15450.75f, 0.0f, 0.0f, 0.0f, 0);
    BrRbSetParams(((char *)&(car)->forcerec[9]), 0.0f, 0.0f, -103.005005f, 0.0f, 0.0f, 0.0f, 0);
    BrRbSetParams(((char *)&(car)->forcerec[10]), 0.0f, 0.0f, -103.005005f, 0.0f, 0.0f, 0.0f, 0);
    car->forcerec[0].next = &car->forcerec[2];
    car->forcerec[2].next = &car->forcerec[1];
    car->forcerec[1].next = &car->forcerec[3];
    car->forcerec[3].next = &car->forcerec[8];
    car->forcerec[8].next = 0;
    car->body[0].forces = &car->forcerec[0];
    BrRbSetParams(((char *)&(car)->forcerec[12]), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1);
    BrRbSetParams(((char *)&(car)->forcerec[14]), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1);
    BrRbSetParams(((char *)&(car)->forcerec[13]), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1);
    BrRbSetParams(((char *)&(car)->forcerec[15]), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1);
    car->body[1].forces = &car->forcerec[12];
    car->body[3].forces = &car->forcerec[14];
    car->body[2].forces = &car->forcerec[13];
    car->body[4].forces = &car->forcerec[15];
    car->forcerec[10].next = 0;
    car->forcerec[9].next = 0;
    car->forcerec[12].next = &car->forcerec[10];
    car->forcerec[14].next = &car->forcerec[10];
    car->forcerec[13].next = &car->forcerec[9];
    car->forcerec[15].next = &car->forcerec[9];
    BrRbSetParams(((char *)&(car)->forcerec[4]), 0.0f, 0.0f, 0.0f, -1.5f, -1.0f, 0.0f, 1);
    BrRbSetParams(((char *)&(car)->forcerec[6]), 0.0f, 0.0f, 0.0f, -1.5f, 1.0f, 0.0f, 1);
    BrRbSetParams(((char *)&(car)->forcerec[5]), 0.0f, 0.0f, 0.0f, 1.5f, -1.0f, 0.0f, 1);
    BrRbSetParams(((char *)&(car)->forcerec[7]), 0.0f, 0.0f, 0.0f, 1.5f, 1.0f, 0.0f, 1);
    BrRbSetParams(((char *)&(car)->forcerec[11]), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0);
    car->forcerec[4].next = &car->forcerec[6];
    car->forcerec[6].next = &car->forcerec[5];
    car->forcerec[5].next = &car->forcerec[7];
    car->forcerec[11].next = 0;
    *(unsigned char *)((char *)&(car)->pade44[0xD]) = 0;
    *(float *)((char *)&(car)->pade44[0x0]) = 0;
    *(float *)((char *)&(car)->pade44[0x8]) = 0;
    car->forcerec[7].next = &car->forcerec[11];
    *(unsigned char *)((char *)&(car)->body[0].tyres) = car->xe60;
  }
  *(unsigned char *)((char *)&(car)->pade44[0xC]) = 0;
  *(unsigned char *)((char *)&(car)->pade44[0x4]) = 0;
}

/* WHAT IT DOES: Build the car's draw matrices from its body state: the body
 * rolled by its spin angle; each wheel spun about its axle (the front two
 * also steered, at twice the steering angle), placed at its mount point in
 * the body's frame; the mount points are lowered 0.254 while this runs. */
/* @implements 0x80221E0C tgr BrCarBuildMatrices */
void BrCarBuildMatrices(BrCar *car)
{
  float spin[4][4];
  float m[4][4];
  float steer[4][4];

  guRotateF(m, car->body[0].angle, 1.0f, 0.0f, 0.0f);
  guMtxCatF(m, car->body[0].m, car->mtx0);
  car->body[1 + (0)].m[3][2] += 0.254f;
  car->body[1 + (1)].m[3][2] += 0.254f;
  car->body[1 + (2)].m[3][2] += 0.254f;
  car->body[1 + (3)].m[3][2] += 0.254f;
  guRotateF(m, car->body[1 + (0)].angle, 0.0f, 1.0f, 0.0f);
  guMtxCatF(m, car->body[0].m, car->wheelMtx[2]);
  BrMat3MulVecRows(car->wheelMtx[2][3], car->body[0].m, car->body[1 + (0)].m[3]);
  guRotateF(m, car->body[1 + (2)].angle, 0.0f, 1.0f, 0.0f);
  guMtxCatF(m, car->body[0].m, car->wheelMtx[3]);
  BrMat3MulVecRows(car->wheelMtx[3][3], car->body[0].m, car->body[1 + (2)].m[3]);
  guRotateF(spin, car->body[1 + (1)].angle, 0.0f, 1.0f, 0.0f);
  guRotateF(steer, car->body[1 + (1)].steer * 114.59155f, 0.0f, 0.0f, 1.0f);
  guMtxCatF(spin, steer, m);
  guMtxCatF(m, car->body[0].m, car->wheelMtx[1]);
  BrMat3MulVecRows(car->wheelMtx[1][3], car->body[0].m, car->body[1 + (1)].m[3]);
  guRotateF(spin, car->body[1 + (3)].angle, 0.0f, 1.0f, 0.0f);
  guRotateF(steer, car->body[1 + (3)].steer * 114.59155f, 0.0f, 0.0f, 1.0f);
  guMtxCatF(spin, steer, m);
  guMtxCatF(m, car->body[0].m, car->wheelMtx[0]);
  BrMat3MulVecRows(car->wheelMtx[0][3], car->body[0].m, car->body[1 + (3)].m[3]);
  car->body[1 + (0)].m[3][2] -= 0.254f;
  car->body[1 + (1)].m[3][2] -= 0.254f;
  car->body[1 + (2)].m[3][2] -= 0.254f;
  car->body[1 + (3)].m[3][2] -= 0.254f;
}
