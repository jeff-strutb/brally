/* carphys.c -- the car's physical state
 */
#include "tgr/common.h"
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
typedef struct BrCarWheel {     /* a wheel's rigid body */
  char pad00[0x18];
  TgrAddr forces;                 /* char * -- 0x18  its force list */
  char pad1c[0x78 - 0x1C];
  char state[0x44];             /* 0x78 */
  float mtx[4][4];              /* 0xBC */
} BrCarWheel;
#define CP_AT(car, off) ((char *)(car) + (off))
typedef struct BrPhysBody {     /* a rigid body as the car embeds it (0x208 bytes) */
  char pad00[0x1C];
  int shape;                    /* 0x1C  1: box; 2: wheel */
  float w, h, d;                /* 0x20  box size */
  float mass;                   /* 0x2C */
  char pad30[0x78 - 0x30];
  float pos[3];                 /* 0x78  state: position */
  float vel[3];                 /* 0x84 */
  float q[4];                   /* 0x90  orientation */
  float omega[3];               /* 0xA0 */
  float qdot[4];                /* 0xAC */
  float mtx[4][4];              /* 0xBC */
  char padfc[0x19C - 0xFC];
  int x19c;                     /* 0x19C */
  char pad1a0[0x1B8 - 0x1A0];
  float spring;                 /* 0x1B8 */
  float damper;                 /* 0x1BC */
  char pad1c0[0x1C4 - 0x1C0];
  float x1c4;                   /* 0x1C4 */
  char pad1c8[0x1D8 - 0x1C8];
  float x1d8;                   /* 0x1D8 */
  char pad1dc[0x208 - 0x1DC];
} BrPhysBody;
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
#define CP_BODY(car, off) ((BrPhysBody *)CP_AT(car, off))
/* -- end declarations -- */

/* WHAT IT DOES: Place a car at a new position: copies the 4x4 placement
 * matrix into the car, sets its position and velocity from it and rebuilds
 * the car's orientation. */
/* @implements 0x80220150 tgr BrCarPlaceAt */
void BrCarPlaceAt(char *car, float *mtx)
{
  float *q;
  float x;
  float y;
  float z;
  float w;

  memcpy(car, mtx, 0x40);
  BrMatToQuat(mtx, car + 0x1d8);
  q = (float *)(car + 0x1d8);
  x = q[0];
  y = q[1];
  z = q[2];
  w = q[3];
  ((float *)(car + 0x274))[0] = ((float *)(car + 0x2b8))[0] = x;
  ((float *)(car + 0x274))[1] = ((float *)(car + 0x2b8))[1] = y;
  ((float *)(car + 0x274))[2] = ((float *)(car + 0x2b8))[2] = z;
  ((float *)(car + 0x274))[3] = ((float *)(car + 0x2b8))[3] = w;
  BrQuatToMat(car + 0x204, car + 0x1c0);
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
  *(TgrAddr *)CP_AT(car, 0x160) = tgr_addr32(CP_AT(car, 0xB70));
  TGR_PTR(struct BrCarWheel *, car->wheel[0])->forces = tgr_addr32(CP_AT(car, 0xCF0));
  TGR_PTR(struct BrCarWheel *, car->wheel[1])->forces = tgr_addr32(CP_AT(car, 0xD30));
  TGR_PTR(struct BrCarWheel *, car->wheel[2])->forces = tgr_addr32(CP_AT(car, 0xD10));
  TGR_PTR(struct BrCarWheel *, car->wheel[3])->forces = tgr_addr32(CP_AT(car, 0xD50));
  *(float *)(TGR_PTR(char *, TGR_PTR(struct BrCarWheel *, car->wheel[0])->forces) + 0x08) = 0.0f;
  *(float *)(TGR_PTR(char *, TGR_PTR(struct BrCarWheel *, car->wheel[0])->forces) + 0x0C) = 0.0f;
  *(float *)(TGR_PTR(char *, TGR_PTR(struct BrCarWheel *, car->wheel[0])->forces) + 0x10) = 0.0f;
  *(float *)(TGR_PTR(char *, TGR_PTR(struct BrCarWheel *, car->wheel[1])->forces) + 0x08) = 0.0f;
  *(float *)(TGR_PTR(char *, TGR_PTR(struct BrCarWheel *, car->wheel[1])->forces) + 0x0C) = 0.0f;
  *(float *)(TGR_PTR(char *, TGR_PTR(struct BrCarWheel *, car->wheel[1])->forces) + 0x10) = 0.0f;
  *(float *)(TGR_PTR(char *, TGR_PTR(struct BrCarWheel *, car->wheel[2])->forces) + 0x08) = 0.0f;
  *(float *)(TGR_PTR(char *, TGR_PTR(struct BrCarWheel *, car->wheel[2])->forces) + 0x0C) = 0.0f;
  *(float *)(TGR_PTR(char *, TGR_PTR(struct BrCarWheel *, car->wheel[2])->forces) + 0x10) = 0.0f;
  *(float *)(TGR_PTR(char *, TGR_PTR(struct BrCarWheel *, car->wheel[3])->forces) + 0x08) = 0.0f;
  *(float *)(TGR_PTR(char *, TGR_PTR(struct BrCarWheel *, car->wheel[3])->forces) + 0x0C) = 0.0f;
  *(float *)(TGR_PTR(char *, TGR_PTR(struct BrCarWheel *, car->wheel[3])->forces) + 0x10) = 0.0f;
  BrTyreSprings(CP_AT(car, 0x148));
  if (*(int *)CP_AT(car, 0xE54) == 0) {
    *(float *)CP_AT(car, 0xE44) = 0.0f;
    *(float *)CP_AT(car, 0xE4C) = 0.0f;
    *(unsigned char *)CP_AT(car, 0xE50) = 0;
    BrWheelTyre(CP_AT(car, 0x148), TGR_PTR(struct BrCarWheel *, car->wheel[0]), (float *)CP_AT(car, 0xE4C), (unsigned char *)CP_AT(car, 0xE50), 0.033333335f);
    BrWheelTyre(CP_AT(car, 0x148), TGR_PTR(struct BrCarWheel *, car->wheel[1]), (float *)CP_AT(car, 0xE4C), (unsigned char *)CP_AT(car, 0xE50), 0.033333335f);
    BrWheelTyre(CP_AT(car, 0x148), TGR_PTR(struct BrCarWheel *, car->wheel[2]), (float *)CP_AT(car, 0xE44), (unsigned char *)CP_AT(car, 0xE48), 0.033333335f);
    BrWheelTyre(CP_AT(car, 0x148), TGR_PTR(struct BrCarWheel *, car->wheel[3]), (float *)CP_AT(car, 0xE44), (unsigned char *)CP_AT(car, 0xE48), 0.033333335f);
  }
  *(int *)CP_AT(car, 0xE54) = 0;
  *(float *)CP_AT(car, 0x244) = 0.0f;
  *(float *)CP_AT(car, 0x248) = 0.0f;
  *(float *)CP_AT(car, 0x24C) = 0.0f;
  *(float *)CP_AT(car, 0x250) = 0.0f;
  *(float *)CP_AT(car, 0x254) = 0.0f;
  *(float *)CP_AT(car, 0x258) = 0.0f;
  BrRbForcesClear(CP_AT(car, 0x148));
  BrRbIntegrate(&car->st, CP_AT(car, 0x148), 0.033333335f);
  BrCarAxleGrip(CP_AT(car, 0x148), 0.033333335f, (float *)CP_AT(car, 0xE4C), (float *)CP_AT(car, 0xE44),
                (unsigned char *)CP_AT(car, 0xE50), (unsigned char *)CP_AT(car, 0xE48));
  BrRbQuatDerivative(&car->st);
  *(TgrAddr *)CP_AT(car, 0x160) = tgr_addr32(CP_AT(car, 0xBF0));
  TGR_PTR(struct BrCarWheel *, car->wheel[0])->forces = 0;
  TGR_PTR(struct BrCarWheel *, car->wheel[2])->forces = 0;
  TGR_PTR(struct BrCarWheel *, car->wheel[1])->forces = 0;
  TGR_PTR(struct BrCarWheel *, car->wheel[3])->forces = 0;
  BrTyreSkidCheck(CP_AT(car, 0x148), CP_AT(car, 0xCD0));
  BrTyreLoads(CP_AT(car, 0x148));
  *(float *)CP_AT(car, 0x244) = 0.0f;
  *(float *)CP_AT(car, 0x248) = 0.0f;
  *(float *)CP_AT(car, 0x24C) = 0.0f;
  *(float *)CP_AT(car, 0x250) = 0.0f;
  *(float *)CP_AT(car, 0x254) = 0.0f;
  *(float *)CP_AT(car, 0x258) = 0.0f;
  BrRbForcesClear(CP_AT(car, 0x148));
  memcpy((char *)&car->stB, (char *)&car->st, sizeof(BrRbState));
  BrRbIntegrate(&car->stB, CP_AT(car, 0x148), 0.033333335f);
  for (i = 0; i < 3; i++) {
    if (((&car->st.angVel.x)[i] == 0.0f ? 0.0f : ((&car->st.angVel.x)[i] > 0.0f ? 1.0f : -1.0f)) !=
        ((&car->stB.angVel.x)[i] == 0.0f ? 0.0f : ((&car->stB.angVel.x)[i] > 0.0f ? 1.0f : -1.0f))) {
      (&car->st.angVel.x)[i] = 0.0f;
    } else {
      (&car->st.angVel.x)[i] = (&car->stB.angVel.x)[i];
    }
    if (((&car->st.vel.x)[i] == 0.0f ? 0.0f : ((&car->st.vel.x)[i] > 0.0f ? 1.0f : -1.0f)) !=
        ((&car->stB.vel.x)[i] == 0.0f ? 0.0f : ((&car->stB.vel.x)[i] > 0.0f ? 1.0f : -1.0f))) {
      (&car->st.vel.x)[i] = 0.0f;
    } else {
      (&car->st.vel.x)[i] = (&car->stB.vel.x)[i];
    }
  }
  memcpy((char *)&car->stA, (char *)&car->st, sizeof(BrRbState));
  BrRbQuatDerivative(&car->stA);
  BrRbQuatDerivative(&car->stA);
  BrCarPhysAdvance(CP_AT(car, 0x148));
  memcpy((char *)&car->st, (char *)&car->stB, sizeof(BrRbState));
  BrTyreDepthAll(CP_AT(car, 0x148));
  BrQuatToMat((float *)TGR_PTR(struct BrCarWheel *, car->wheel[0])->mtx, (float *)TGR_PTR(struct BrCarWheel *, car->wheel[0])->state);
  BrQuatToMat((float *)TGR_PTR(struct BrCarWheel *, car->wheel[1])->mtx, (float *)TGR_PTR(struct BrCarWheel *, car->wheel[1])->state);
  BrQuatToMat((float *)TGR_PTR(struct BrCarWheel *, car->wheel[2])->mtx, (float *)TGR_PTR(struct BrCarWheel *, car->wheel[2])->state);
  BrQuatToMat((float *)TGR_PTR(struct BrCarWheel *, car->wheel[3])->mtx, (float *)TGR_PTR(struct BrCarWheel *, car->wheel[3])->state);
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
void BrPadConsume(BrDrivePad *pad, unsigned int bit);
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
 * different tuning of the same idea).
 * RESIDUE (753): the ROM computes x / 2.0f as a divide where ours becomes
 * a multiply by 0.5, keeps the steering target in f14 (ours f18) and its
 * frame is 0x10 smaller; the sign tests and the gearbox follow the same
 * flow. */
/* @t4-pass 0x80222050 1 2026-09-29 compiles 121 best 753 moved 13  (n64/tools/n64permute.py) */
/* @t4-pass 0x80222050 2 2026-09-29 compiles 121 best 753 moved 0  (n64/tools/n64permute.py) */
/* @t3 0x80222050 */
/* @implements 0x80222050 tgr BrCarDriveInput */
void BrCarDriveInput(BrCar *car)
{
  float v[3];
  float rate;
  float x;
  float old;
  float s;
  float a;
  float t;
  float sp;
  float lim;
  float hi;
  float lo;
  float tgt;
  float cur;
  float bonus;
  float torque;
  unsigned int flags;
  int k;
  int g;
  int tltc;
  short ch;
  int ctlt;

  s = ((BrDrivePad *)TGR_PTR(unsigned int *, car->pad))->steer;
  if (((BrDrivePad *)TGR_PTR(unsigned int *, car->pad))->kind == 4) {
    a = s < 0.0f ? -s : s;
    t = SGN(s);
    car->xdf0 = -(sqrtf(sqrtf(sqrtf(a) * a) * sqrtf(a)) * t) * 10.0f * 3.1415927f / 180.0f;
  } else {
    if (0.0f < s) {
      s = s - 0.07f;
      if (s < 0.0f) {
        s = 0.0f;
      }
    } else {
      s = s + 0.07f;
      if (0.0f < s) {
        s = 0.0f;
      }
    }
    sp = car->xfe4[0];
    sqrtf(car->st.vel.z * car->st.vel.z + (car->st.vel.x * car->st.vel.x + car->st.vel.y * car->st.vel.y));
    BrMat4RotateVec(v, car->stMtx, &car->st.vel);
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
      D_8028B728 = 6.0f;
      D_8028B72C = 10.0f;
      D_8028B724 = 0.01f;
      D_8028B730 = 0;
      k = D_8028B730;
      x = D_8028B728;
      break;
    case 1:
      D_8028B720 = 14.0f;
      D_8028B724 = 0.01f;
      D_8028B728 = 6.0f;
      D_8028B730 = 0;
      D_8028B72C = 10.0f;
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
    lim = D_8028B720 - t / 90.0f * D_8028B72C;
    rate = D_8028B724;
    a = s < 0.0f ? -s : s;
    if (a < 0.001f) {
      tgt = 0.0f;
      hi = lim * 3.1415927f / 180.0f;
      lo = -lim * 3.1415927f / 180.0f;
    } else if (s < -0.75f) {
      hi = lim * 3.1415927f / 180.0f;
      lo = -lim * 3.1415927f / 180.0f;
      tgt = hi;
    } else if (0.75f < s) {
      lo = -lim * 3.1415927f / 180.0f;
      hi = lim * 3.1415927f / 180.0f;
      tgt = lo;
    } else {
      x = x - k * (x / 2.0f) * (sp / 200.0f);
      tgt = -s * (x * 3.1415927f / 180.0f);
      hi = lim * 3.1415927f / 180.0f;
      lo = -lim * 3.1415927f / 180.0f;
    }
    if (hi < tgt) {
      tgt = hi;
    }
    if (tgt < lo) {
      tgt = lo;
    }
    cur = car->xdf0;
    ch = SGN(cur) != SGN(tgt) || (0.0f < cur && tgt < cur) || (cur < 0.0f && cur < tgt);
    if (cur == 0.0f) {
      ch = 0;
      *(signed char *)CP_AT(car, 0xE51) = 0;
      cur = car->xdf0;
    }
    tltc = tgt < cur;
    ctlt = cur < tgt;
    if (tltc && *(signed char *)CP_AT(car, 0xE51) < 0) {
      ch = 1;
    }
    if (ctlt && *(signed char *)CP_AT(car, 0xE51) > 0) {
      ch = 1;
    }
    if (ch) {
      *(signed char *)CP_AT(car, 0xE51) = tltc ? -1 : 1;
      cur = car->xdf0;
      rate = 1.0f;
      if (SGN(tgt) != SGN(cur)) {
        tgt = 0.0f;
      }
    } else {
      *(signed char *)CP_AT(car, 0xE51) = 0;
      cur = car->xdf0;
    }
    a = cur < tgt ? -(cur - tgt) : cur - tgt;
    if (a < rate) {
      car->xdf0 = tgt;
    } else if (tgt < cur) {
      car->xdf0 = cur - rate;
    } else {
      car->xdf0 = cur + rate;
    }
  }
  old = car->xdf4;
  if (old < 800.0f) {
    car->xdf4 = 800.0f;
  }
  if (car->xe30 != 0) {
    g = car->xe40;
    if (g >= 2 && car->xdf4 < 4000.0f) {
      car->xe40 = g - 1;
    } else if (g < car->xe28[0] && 6000.0f < car->xdf4 && !(((BrDrivePad *)TGR_PTR(unsigned int *, car->pad))->flags & 0x20000)) {
      car->xe40 = g + 1;
    }
    flags = ((BrDrivePad *)TGR_PTR(unsigned int *, car->pad))->flags;
  } else if (car->xe40 > 0 && (((BrDrivePad *)TGR_PTR(unsigned int *, car->pad))->flags & 0x200000)) {
    BrPadConsume((BrDrivePad *)TGR_PTR(unsigned int *, car->pad), 0x200000);
    car->xe40--;
    flags = ((BrDrivePad *)TGR_PTR(unsigned int *, car->pad))->flags;
  } else if (car->xe40 < car->xe28[0] && ((flags = ((BrDrivePad *)TGR_PTR(unsigned int *, car->pad))->flags) & 0x100000) &&
             !(flags & 0x20000)) {
    BrPadConsume((BrDrivePad *)TGR_PTR(unsigned int *, car->pad), 0x100000);
    car->xe40++;
    flags = ((BrDrivePad *)TGR_PTR(unsigned int *, car->pad))->flags;
  } else {
    flags = ((BrDrivePad *)TGR_PTR(unsigned int *, car->pad))->flags;
  }
  bonus = 0.0f;
  if (D_8026FF18 == 1 && car->xfac == 1) {
    a = D_8031B760[car->slot ^ 1].xfa8 < car->xfa8 ? -(D_8031B760[car->slot ^ 1].xfa8 - car->xfa8)
                                                   : D_8031B760[car->slot ^ 1].xfa8 - car->xfa8;
    a = a * 0.5f;
    bonus = a - 18.0f;
    if (a < 18.0f) {
      bonus = 0.0f;
    } else if (30.0f < bonus) {
      bonus = 30.0f;
    }
  }
  torque = car->xe14[0] * car->xdf4 * car->xdf4 * car->xdf4 + car->xe14[1] * car->xdf4 * car->xdf4 +
           car->xe14[2] * car->xdf4 + car->xe14[3] + bonus;
  if (car->xe30 == 0) {
    torque = torque * 1.03;
  }
  if (!(flags & 0x10000)) {
    torque = 0.0f;
  }
  g = 0;
  car->xe38 = torque * 7.0f;
  if (TGR_PTR(struct BrCarLink *, car->link)->flags & 1) {
    car->xe40 = 0;
  } else {
    g = car->xe40;
    if (g == 0) {
      car->xe40 = 1;
      g = 1;
    }
  }
  if (g != 0) {
    car->xdf4 = car->xdf8[g] * (-(*(float *)CP_AT(car, 0x71C) / 6.2831855f) * 60.0f * car->xe14[4]);
  } else {
    if (((BrDrivePad *)TGR_PTR(unsigned int *, car->pad))->flags & 0x10000) {
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
  t = car->xdf4 - old;
  a = t < 0.0f ? -t : t;
  if (400.0f < a) {
    t = SGN(t) * 400.0f;
  }
  car->xe3c = 0.0f;
  car->xdf4 = old + t;
  if (((BrDrivePad *)TGR_PTR(unsigned int *, car->pad))->flags & 0x40000) {
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
 * RESIDUE (243): the ROM keeps 2.0 in f22 across the chassis init call (for
 * the start height) and 1.0 in f24, one frame slot more; ours reloads 2.0,
 * which shifts the register numbering and scheduling of the store runs. */
/* @t3 0x80222D54 */
/* @t4-pass 0x80222D54 1 2026-09-29 compiles 231 best 243 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x80222D54 2 2026-09-29 compiles 231 best 243 moved 0  (n64/tools/n64permute.py) */
/* @implements 0x80222D54 tgr BrCarPhysInit */
void BrCarPhysInit(BrCar *car)
{
  *(int *)CP_AT(car, 0xE54) = 1;
  *(int *)CP_AT(car, 0xE40) = 0;
  *(float *)CP_AT(car, 0xDF0) = 0.0f;
  *(float *)CP_AT(car, 0xDF4) = 0.0f;
  if (car->model != 0) {
    CP_BODY(car, 0x148)->shape = 1;
    CP_BODY(car, 0x148)->h = 2.0f;
    CP_BODY(car, 0x148)->mass = 1000.0f;
    CP_BODY(car, 0x148)->w = 3.5f;
    CP_BODY(car, 0x148)->d = 1.5f;
    BrRbBodyInit(CP_BODY(car, 0x148));
    CP_BODY(car, 0x148)->pos[0] = 0.0f;
    CP_BODY(car, 0x148)->pos[1] = 0.0f;
    CP_BODY(car, 0x148)->pos[2] = 2.0f;
    CP_BODY(car, 0x148)->vel[0] = 0.0f;
    CP_BODY(car, 0x148)->vel[1] = 0.0f;
    CP_BODY(car, 0x148)->vel[2] = 0.0f;
    CP_BODY(car, 0x148)->q[1] = 0.0f;
    CP_BODY(car, 0x148)->q[2] = 0.0f;
    CP_BODY(car, 0x148)->q[3] = 0.0f;
    CP_BODY(car, 0x148)->omega[0] = 0.0f;
    CP_BODY(car, 0x148)->omega[1] = 0.0f;
    CP_BODY(car, 0x148)->omega[2] = 0.0f;
    CP_BODY(car, 0x148)->q[0] = 1.0f;
    BrQuatToMat((float *)CP_BODY(car, 0x148)->mtx, CP_BODY(car, 0x148)->pos);
    CP_BODY(car, 0x148)->spring = (car->xe64 * 4.0f + 20.0f) * 16000.0f;
    CP_BODY(car, 0x148)->damper = -3000.0f;
    CP_BODY(car, 0x350)->x19c = 0;
    CP_BODY(car, 0x350)->shape = 2;
    CP_BODY(car, 0x350)->mass = 0.0f;
    CP_BODY(car, 0x350)->w = 0.0f;
    CP_BODY(car, 0x350)->h = 0.0f;
    CP_BODY(car, 0x350)->d = 0.0f;
    CP_BODY(car, 0x350)->x1d8 = 0.0f;
    CP_BODY(car, 0x350)->x1c4 = 0.0f;
    BrRbBodyInit(CP_BODY(car, 0x350));
    CP_BODY(car, 0x350)->pos[0] = BEF(((BrCarMounts *)TGR_PTR(char *, car->model))->frontX);
    CP_BODY(car, 0x350)->pos[1] = BEF(((BrCarMounts *)TGR_PTR(char *, car->model))->frontY);
    CP_BODY(car, 0x350)->vel[2] = 0.0f;
    CP_BODY(car, 0x350)->vel[1] = 0.0f;
    CP_BODY(car, 0x350)->vel[0] = 0.0f;
    CP_BODY(car, 0x350)->q[3] = 0.0f;
    CP_BODY(car, 0x350)->q[2] = 0.0f;
    CP_BODY(car, 0x350)->q[1] = 0.0f;
    CP_BODY(car, 0x350)->q[0] = 1.0f;
    CP_BODY(car, 0x350)->omega[2] = 0.0f;
    CP_BODY(car, 0x350)->omega[1] = 0.0f;
    CP_BODY(car, 0x350)->omega[0] = 0.0f;
    CP_BODY(car, 0x350)->pos[2] = -0.1f;
    BrQuatToMat((float *)CP_BODY(car, 0x350)->mtx, CP_BODY(car, 0x350)->pos);
    CP_BODY(car, 0x760)->x19c = 0;
    CP_BODY(car, 0x760)->shape = 2;
    CP_BODY(car, 0x760)->mass = 0.0f;
    CP_BODY(car, 0x760)->w = 0.0f;
    CP_BODY(car, 0x760)->h = 0.0f;
    CP_BODY(car, 0x760)->d = 0.0f;
    CP_BODY(car, 0x760)->x1d8 = 0.0f;
    CP_BODY(car, 0x760)->x1c4 = 0.0f;
    BrRbBodyInit(CP_BODY(car, 0x760));
    CP_BODY(car, 0x760)->pos[0] = BEF(((BrCarMounts *)TGR_PTR(char *, car->model))->frontX);
    CP_BODY(car, 0x760)->pos[1] = -BEF(((BrCarMounts *)TGR_PTR(char *, car->model))->frontY);
    CP_BODY(car, 0x760)->vel[2] = 0.0f;
    CP_BODY(car, 0x760)->vel[1] = 0.0f;
    CP_BODY(car, 0x760)->vel[0] = 0.0f;
    CP_BODY(car, 0x760)->q[3] = 0.0f;
    CP_BODY(car, 0x760)->q[2] = 0.0f;
    CP_BODY(car, 0x760)->q[1] = 0.0f;
    CP_BODY(car, 0x760)->q[0] = 1.0f;
    CP_BODY(car, 0x760)->omega[2] = 0.0f;
    CP_BODY(car, 0x760)->omega[1] = 0.0f;
    CP_BODY(car, 0x760)->omega[0] = 0.0f;
    CP_BODY(car, 0x760)->pos[2] = -0.1f;
    BrQuatToMat((float *)CP_BODY(car, 0x760)->mtx, CP_BODY(car, 0x760)->pos);
    CP_BODY(car, 0x558)->x19c = 0;
    CP_BODY(car, 0x558)->shape = 2;
    CP_BODY(car, 0x558)->mass = 0.0f;
    CP_BODY(car, 0x558)->w = 0.0f;
    CP_BODY(car, 0x558)->h = 0.0f;
    CP_BODY(car, 0x558)->d = 0.0f;
    CP_BODY(car, 0x558)->x1d8 = 0.0f;
    CP_BODY(car, 0x558)->x1c4 = 0.0f;
    BrRbBodyInit(CP_BODY(car, 0x558));
    CP_BODY(car, 0x558)->pos[0] = BEF(((BrCarMounts *)TGR_PTR(char *, car->model))->rearX);
    CP_BODY(car, 0x558)->pos[1] = BEF(((BrCarMounts *)TGR_PTR(char *, car->model))->rearY);
    CP_BODY(car, 0x558)->vel[2] = 0.0f;
    CP_BODY(car, 0x558)->vel[1] = 0.0f;
    CP_BODY(car, 0x558)->vel[0] = 0.0f;
    CP_BODY(car, 0x558)->q[3] = 0.0f;
    CP_BODY(car, 0x558)->q[2] = 0.0f;
    CP_BODY(car, 0x558)->q[1] = 0.0f;
    CP_BODY(car, 0x558)->q[0] = 1.0f;
    CP_BODY(car, 0x558)->omega[2] = 0.0f;
    CP_BODY(car, 0x558)->omega[1] = 0.0f;
    CP_BODY(car, 0x558)->omega[0] = 0.0f;
    CP_BODY(car, 0x558)->pos[2] = -0.1f;
    BrQuatToMat((float *)CP_BODY(car, 0x558)->mtx, CP_BODY(car, 0x558)->pos);
    CP_BODY(car, 0x968)->x19c = 0;
    CP_BODY(car, 0x968)->shape = 2;
    CP_BODY(car, 0x968)->mass = 0.0f;
    CP_BODY(car, 0x968)->w = 0.0f;
    CP_BODY(car, 0x968)->h = 0.0f;
    CP_BODY(car, 0x968)->d = 0.0f;
    CP_BODY(car, 0x968)->x1d8 = 0.0f;
    CP_BODY(car, 0x968)->x1c4 = 0.0f;
    BrRbBodyInit(CP_BODY(car, 0x968));
    CP_BODY(car, 0x968)->pos[0] = BEF(((BrCarMounts *)TGR_PTR(char *, car->model))->rearX);
    CP_BODY(car, 0x968)->pos[1] = -BEF(((BrCarMounts *)TGR_PTR(char *, car->model))->rearY);
    CP_BODY(car, 0x968)->vel[2] = 0.0f;
    CP_BODY(car, 0x968)->vel[1] = 0.0f;
    CP_BODY(car, 0x968)->vel[0] = 0.0f;
    CP_BODY(car, 0x968)->q[3] = 0.0f;
    CP_BODY(car, 0x968)->q[2] = 0.0f;
    CP_BODY(car, 0x968)->q[1] = 0.0f;
    CP_BODY(car, 0x968)->q[0] = 1.0f;
    CP_BODY(car, 0x968)->omega[2] = 0.0f;
    CP_BODY(car, 0x968)->omega[1] = 0.0f;
    CP_BODY(car, 0x968)->omega[0] = 0.0f;
    CP_BODY(car, 0x968)->pos[2] = -0.1f;
    BrQuatToMat((float *)CP_BODY(car, 0x968)->mtx, CP_BODY(car, 0x968)->pos);
    car->wheel[0] = tgr_addr32((struct BrCarWheel *)CP_AT(car, 0x350));
    car->wheel[1] = tgr_addr32((struct BrCarWheel *)CP_AT(car, 0x760));
    car->wheel[2] = tgr_addr32((struct BrCarWheel *)CP_AT(car, 0x558));
    car->wheel[3] = tgr_addr32((struct BrCarWheel *)CP_AT(car, 0x968));
    BrRbSetParams(CP_AT(car, 0xB70), 0.0f, 0.0f, 0.0f, -1.5f, -1.0f, 0.0f, 1);
    BrRbSetParams(CP_AT(car, 0xBB0), 0.0f, 0.0f, 0.0f, -1.5f, 1.0f, 0.0f, 1);
    BrRbSetParams(CP_AT(car, 0xB90), 0.0f, 0.0f, 0.0f, 1.5f, -1.0f, 0.0f, 1);
    BrRbSetParams(CP_AT(car, 0xBD0), 0.0f, 0.0f, 0.0f, 1.5f, 1.0f, 0.0f, 1);
    BrRbSetParams(CP_AT(car, 0xC70), 0.0f, 0.0f, -15450.75f, 0.0f, 0.0f, 0.0f, 0);
    BrRbSetParams(CP_AT(car, 0xC90), 0.0f, 0.0f, -103.005005f, 0.0f, 0.0f, 0.0f, 0);
    BrRbSetParams(CP_AT(car, 0xCB0), 0.0f, 0.0f, -103.005005f, 0.0f, 0.0f, 0.0f, 0);
    *(TgrAddr *)CP_AT(car, 0xB70) = tgr_addr32(CP_AT(car, 0xBB0));
    *(TgrAddr *)CP_AT(car, 0xBB0) = tgr_addr32(CP_AT(car, 0xB90));
    *(TgrAddr *)CP_AT(car, 0xB90) = tgr_addr32(CP_AT(car, 0xBD0));
    *(TgrAddr *)CP_AT(car, 0xC70) = 0;
    *(TgrAddr *)CP_AT(car, 0xBD0) = tgr_addr32(CP_AT(car, 0xC70));
    *(TgrAddr *)CP_AT(car, 0x160) = tgr_addr32(CP_AT(car, 0xB70));
    BrRbSetParams(CP_AT(car, 0xCF0), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1);
    BrRbSetParams(CP_AT(car, 0xD30), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1);
    BrRbSetParams(CP_AT(car, 0xD10), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1);
    BrRbSetParams(CP_AT(car, 0xD50), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1);
    *(TgrAddr *)CP_AT(car, 0x368) = tgr_addr32(CP_AT(car, 0xCF0));
    *(TgrAddr *)CP_AT(car, 0x778) = tgr_addr32(CP_AT(car, 0xD30));
    *(TgrAddr *)CP_AT(car, 0x570) = tgr_addr32(CP_AT(car, 0xD10));
    *(TgrAddr *)CP_AT(car, 0xCB0) = 0;
    *(TgrAddr *)CP_AT(car, 0xC90) = 0;
    *(TgrAddr *)CP_AT(car, 0xCF0) = tgr_addr32(CP_AT(car, 0xCB0));
    *(TgrAddr *)CP_AT(car, 0xD30) = tgr_addr32(CP_AT(car, 0xCB0));
    *(TgrAddr *)CP_AT(car, 0xD10) = tgr_addr32(CP_AT(car, 0xC90));
    *(TgrAddr *)CP_AT(car, 0xD50) = tgr_addr32(CP_AT(car, 0xC90));
    *(TgrAddr *)CP_AT(car, 0x980) = tgr_addr32(CP_AT(car, 0xD50));
    BrRbSetParams(CP_AT(car, 0xBF0), 0.0f, 0.0f, 0.0f, -1.5f, -1.0f, 0.0f, 1);
    BrRbSetParams(CP_AT(car, 0xC30), 0.0f, 0.0f, 0.0f, -1.5f, 1.0f, 0.0f, 1);
    BrRbSetParams(CP_AT(car, 0xC10), 0.0f, 0.0f, 0.0f, 1.5f, -1.0f, 0.0f, 1);
    BrRbSetParams(CP_AT(car, 0xC50), 0.0f, 0.0f, 0.0f, 1.5f, 1.0f, 0.0f, 1);
    BrRbSetParams(CP_AT(car, 0xCD0), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0);
    *(TgrAddr *)CP_AT(car, 0xBF0) = tgr_addr32(CP_AT(car, 0xC30));
    *(TgrAddr *)CP_AT(car, 0xC30) = tgr_addr32(CP_AT(car, 0xC10));
    *(TgrAddr *)CP_AT(car, 0xCD0) = 0;
    *(unsigned char *)CP_AT(car, 0xE51) = 0;
    *(float *)CP_AT(car, 0xE44) = 0.0f;
    *(float *)CP_AT(car, 0xE4C) = 0.0f;
    *(TgrAddr *)CP_AT(car, 0xC50) = tgr_addr32(CP_AT(car, 0xCD0));
    *(unsigned char *)CP_AT(car, 0x345) = car->xe60;
    *(TgrAddr *)CP_AT(car, 0xC10) = tgr_addr32(CP_AT(car, 0xC50));
  }
  *(unsigned char *)CP_AT(car, 0xE50) = 0;
  *(unsigned char *)CP_AT(car, 0xE48) = 0;
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

  guRotateF(m, car->spin, 1.0f, 0.0f, 0.0f);
  guMtxCatF(m, car->stMtx, car->mtx0);
  car->wheels[0].pos[2] += 0.254f;
  car->wheels[1].pos[2] += 0.254f;
  car->wheels[2].pos[2] += 0.254f;
  car->wheels[3].pos[2] += 0.254f;
  guRotateF(m, car->wheels[0].spin, 0.0f, 1.0f, 0.0f);
  guMtxCatF(m, car->stMtx, car->wheelMtx[2]);
  BrMat3MulVecRows(car->wheelMtx[2][3], car->stMtx, car->wheels[0].pos);
  guRotateF(m, car->wheels[2].spin, 0.0f, 1.0f, 0.0f);
  guMtxCatF(m, car->stMtx, car->wheelMtx[3]);
  BrMat3MulVecRows(car->wheelMtx[3][3], car->stMtx, car->wheels[2].pos);
  guRotateF(spin, car->wheels[1].spin, 0.0f, 1.0f, 0.0f);
  guRotateF(steer, car->wheels[1].steer * 114.59155f, 0.0f, 0.0f, 1.0f);
  guMtxCatF(spin, steer, m);
  guMtxCatF(m, car->stMtx, car->wheelMtx[1]);
  BrMat3MulVecRows(car->wheelMtx[1][3], car->stMtx, car->wheels[1].pos);
  guRotateF(spin, car->wheels[3].spin, 0.0f, 1.0f, 0.0f);
  guRotateF(steer, car->wheels[3].steer * 114.59155f, 0.0f, 0.0f, 1.0f);
  guMtxCatF(spin, steer, m);
  guMtxCatF(m, car->stMtx, car->wheelMtx[0]);
  BrMat3MulVecRows(car->wheelMtx[0][3], car->stMtx, car->wheels[3].pos);
  car->wheels[0].pos[2] -= 0.254f;
  car->wheels[1].pos[2] -= 0.254f;
  car->wheels[2].pos[2] -= 0.254f;
  car->wheels[3].pos[2] -= 0.254f;
}
