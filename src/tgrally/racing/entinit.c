/* entinit.c -- resetting car entities
 */
#include "tgr/common.h"
#include "tgr/car.h"

/* -- declarations -- */
void BrCarReset(BrCar *car);
extern int D_8028AE04;
void BrCarPickKind(BrCar *car);
extern int D_8026FF08;
extern int D_8026FF18;
void BrPadInit(unsigned int *param_1);
extern unsigned int D_8036A8E0[4][0x57];
void guMtxIdent(int *m);
void BrMat4ResetW(float m[4][4]);
void BrCarSetVel(BrCar *car, float x, float y, float z);
void *memcpy(void *dst, void *src, unsigned int n);
void BrQuatToMat(float m[4][4], BrRbState *st);
float sinf(float x);
float cosf(float x);
void BrQuatMul(float out[4], float a[4], float b[4]);
void BrVec4Normalise(float v[4]);
extern float D_80025C38;                /* the track's sky height */
void func_80222D54(BrCar *car);
void BrCarSetPos(BrCar *car, float x, float y, float z);
void BrCarSetHeading(BrCar *car, float h);
void BrCarSetAngVel(BrCar *car, float x, float y, float z);
float BrAtan2(float y, float x);
void BrCarCamPlaceBehind(BrCar *car, BrCarCam *cam, float t);
typedef struct BrCarWheel {     /* a wheel's rigid body (rbquat.c's BrRbBody) */
  char pad000[0x19C];
  int x19c;                     /* 0x19C */
  unsigned char x1a0;           /* 0x1A0 */
  char pad1a1[0x1B4 - 0x1A1];
  int x1b4;                     /* 0x1B4 */
} BrCarWheel;
typedef struct BrRestart {      /* a restart point on the car's route, 0x28 bytes */
  float pos[3];
  char pad0c[0x28 - 0x0C];
} BrRestart;
typedef struct BrRoute {
  char pad00[0x4C];
  BrRestart pt[1];              /* 0x4C */
} BrRoute;
/* -- end declarations -- */

/* WHAT IT DOES: Reset every car slot for a new session: runs the per-car
 * reset on each car record (0x2090 bytes apiece) and clears its matching
 * input record. */
/* @implements 0x80200154 tgr BrEntAllReset */
void BrEntAllReset(void)
{
  int i;

  for (i = 0; i < 4; i++) {
    BrCarReset(&D_8031B760[i]);
    BrPadInit(D_8036A8E0[i]);
  }
}

/* WHAT IT DOES: Reset a car before a race: the w column of its body, its
 * cameras' (each back to a 30 degree view) and its wheels' matrices; no
 * model; the first camera in use; placed at the origin; and its kind's
 * handling numbers copied in. */
/* @implements 0x80220620 tgr BrCarResetFrames */
void BrCarResetFrames(BrCar *car)
{
  int kind;

  BrMat4ResetW(car->mtx0);
  BrMat4ResetW(car->cams[0].mtx);
  car->cams[0].fov = 0.5235988f;
  BrMat4ResetW(car->cams[1].mtx);
  car->cams[1].fov = 0.5235988f;
  BrMat4ResetW(car->cams[2].mtx);
  car->cams[2].fov = 0.5235988f;
  BrMat4ResetW(car->cams[3].mtx);
  car->cams[3].fov = 0.5235988f;
  BrMat4ResetW(car->cam4.mtx);
  car->cam4.fov = 0.5235988f;
  car->cam = &car->cams[0];
  car->model = 0;
  BrMat4ResetW(car->wheelMtx[0]);
  BrMat4ResetW(car->wheelMtx[1]);
  BrMat4ResetW(car->wheelMtx[2]);
  BrMat4ResetW(car->wheelMtx[3]);
  BrCarSetVel(car, 0.0f, 0.0f, 0.0f);
  car->xf5c = 0;
  car->xf60 = 0;
  car->cam2 = 0;
  kind = car->kind;
  memcpy(car->xdf8, D_8028B330[kind].x00, 0x1c);
  car->xe14[0] = D_8028B330[kind].x1c[0];
  car->xe14[1] = D_8028B330[kind].x1c[1];
  car->xe14[2] = D_8028B330[kind].x1c[2];
  car->xe14[3] = D_8028B330[kind].x1c[3];
  car->xe14[4] = D_8028B330[kind].x1c[4];
  car->xe28[0] = D_8028B330[kind].x30[0];
  car->xe30 = car->xe6c;
  car->xe28[1] = D_8028B330[kind].x30[1];
  car->x324[0] = D_8028B330[kind].x38[0];
  car->x324[1] = D_8028B330[kind].x38[1];
  car->x324[2] = D_8028B330[kind].x38[2];
  car->x324[3] = D_8028B330[kind].x38[3];
  car->xe34 = D_8028B330[kind].x48;
}

/* WHAT IT DOES: Point a car record (0x2090 bytes) at the pad record of the
 * same slot and reset the car's matrix at +0x1D88 to identity. */
/* @implements 0x802207A4 tgr BrCarBindPad */
void BrCarBindPad(BrCar *car)
{
  car->pad = D_8036A8E0[car - D_8031B760];
  guMtxIdent(car->mtx);
}

/* WHAT IT DOES: Choose a car's kind (+0x205C) for the coming race from the
 * round table (the round is player 1's season round). Player slots take the
 * round's first kind in a championship and are left alone otherwise; in
 * arcade mode the other cars copy player 1; in a championship the first
 * computer car takes the round's first kind and the rest the lowest kind
 * allowed by the round's mask (rounds past 3 use round 3's mask; no bit set
 * means kind 5). */
/* @implements 0x80225F88 tgr BrCarPickKind */
void BrCarPickKind(BrCar *car)
{
  int i;
  int r;
  short mask;

  r = D_8031B760[0].season->round;
  if (r > 3) {
    r = 3;
  }
  if (car->slot >= D_8026FF08) {
    if (D_8026FF18 == 1) {
      car->kind = D_8031B760[0].kind;
      return;
    }
    mask = D_8028B944[r].kindMask;
    for (i = 0; i < 16; i++) {
      if (mask & 1) {
        break;
      }
      mask >>= 1;
    }
    if (i == 16) {
      i = 5;
    }
    if (car->slot > D_8026FF08) {
      car->kind = i;
    } else {
      car->kind = D_8028B944[D_8031B760[0].season->round].kinds[car->slot - D_8026FF08];
    }
  } else {
    if (D_8026FF18 == 0) {
      car->kind = D_8028B944[D_8031B760[0].season->round].kinds[0];
    }
  }
}

/* WHAT IT DOES: Reset one car record: bind it to its pad, store its slot
 * index, and for the two player slots start a fresh season record (car 1 and
 * car 8 unlocked, per-car values cleared); other slots get no season. */
/* @implements 0x80226100 tgr BrCarReset */
void BrCarReset(BrCar *car)
{
  int i;

  BrCarBindPad(car);
  car->slot = car - D_8031B760;
  if (car->slot < 2) {
    car->season = &D_80324300[car->slot];
    car->season->state = 0;
    car->season->round = 0;
    car->season->points[0] = 0;
    car->season->race = 0;
    car->season->unlocked = 0x102;
    car->season->xce = 4;
    car->season->xd0 = 1;
    car->season->xd8 = 1;
    car->season->xd4 = 0;
    car->season->xe0 = 1;
    car->season->xdc = 2;
    car->season->xe4 = 0;
    for (i = 0; i < D_8028AE04; i++) {
      car->season->x8c[i] = 0.0f;
      car->season->xe8[i] = 0.0f;
    }
  } else {
    car->season = 0;
  }
  BrCarPickKind(car);
  car->xe58 = 0;
}

/* WHAT IT DOES: Put a car that has left the track (flagged, or fallen 10
 * below the sky height) back on it: at its current restart point, 2 above
 * the road (0.01 sideways per slot outside mode 2), facing along the
 * route, stopped, wheels reset, and the chase camera moved in behind. */
/* @implements 0x80226248 tgr BrCarRespawn */
void BrCarRespawn(BrCar *car)
{
  if (car->x340 < 0 || car->mtx0[3][2] < D_80025C38 - 10.0f) {
    func_80222D54(car);
    if (D_8026FF18 == 2) {
      BrCarSetPos(car, ((BrRoute *)car->xf5c)->pt[car->xf60].pos[0],
                  ((BrRoute *)car->xf5c)->pt[car->xf60].pos[1],
                  ((BrRoute *)car->xf5c)->pt[car->xf60].pos[2] + 2.0);
    } else {
      BrCarSetPos(car, ((BrRoute *)car->xf5c)->pt[car->xf60].pos[0] + car->slot * 0.01f,
                  ((BrRoute *)car->xf5c)->pt[car->xf60].pos[1],
                  ((BrRoute *)car->xf5c)->pt[car->xf60].pos[2] + 2.0);
    }
    BrCarSetHeading(car, BrAtan2(car->xf64, car->xf68));
    BrCarSetVel(car, 0.0f, 0.0f, 0.0f);
    BrCarSetAngVel(car, 0.0f, 0.0f, 0.0f);
    car->wheel[0]->x19c = 0;
    car->wheel[0]->x1b4 = 0;
    car->wheel[0]->x1a0 = 2;
    car->wheel[1]->x19c = 0;
    car->wheel[1]->x1b4 = 0;
    car->wheel[1]->x1a0 = 2;
    car->wheel[3]->x19c = 0;
    car->wheel[3]->x1b4 = 0;
    car->wheel[3]->x1a0 = 2;
    car->wheel[2]->x19c = 0;
    car->wheel[2]->x1b4 = 0;
    car->wheel[2]->x1a0 = 2;
    car->xe70[0] = 0;
    car->xe70[1] = 0;
    car->xe70[2] = 0;
    car->xe70[3] = -180;
    car->x340 = 0;
    car->colour[3] = 2;
    car->x2064 = 0.1f;
    BrCarCamPlaceBehind(car, &car->cams[1], 1.0f);
    car->xf48 = 1;
    car->camPosA.x = car->cams[1].mtx[3][0];
    car->camPosA.y = car->cams[1].mtx[3][1];
    car->camPosA.z = car->cams[1].mtx[3][2];
  }
}

/* -- declarations: BrCarPlayerCtl -- */
typedef struct BrCarInput {     /* the pad record as a car reads it */
  unsigned int flags;           /* 0x00  buttons; bits 24-27 pick a camera */
  char pad04[0x1C - 0x04];
  float throttle;               /* 0x1C */
  float steer;                  /* 0x20 */
} BrCarInput;
typedef struct BrViewRec { int x; int y; int w; int h; int car; } BrViewRec;
extern BrViewRec D_8031B2C8[2];         /* the views and the car each follows */
extern int D_8028AB0C;                  /* views on screen */
extern int D_8028A8AC;                  /* the picture is mirrored */
extern int D_8028B7F8;                  /* frames the camera keeps its checkpoint target */
extern int D_8028B9EC;                  /* which fallback camera is next */
extern int D_8028B7FC;
extern int D_80270788;
extern BrVec3 *D_80025C84;              /* the camera checkpoints */
extern int D_80025C88;                  /* and their count */
void BrPadConsume(BrCarInput *pad, unsigned int bit);
void BrCarCamPlaceChase(BrCar *car);
void BrVec3MulAddTo(BrVec3 *pV, float *pD, float t);
void BrCarLineFit(BrCar *car);
void BrVec3Scale(BrVec3 *pOut, BrVec3 *pV, float s);
float BrVec3Dot(BrVec3 *pA, float *pB);
void BrCarPhysTick(BrCar *car);
void BrVec3Add(BrVec3 *pOut, BrVec3 *pA, BrVec3 *pB);
void BrVec3AddTo(BrVec3 *pV, float *pB);
void BrVec3Sub(BrVec3 *pOut, BrVec3 *pA, BrVec3 *pB);
float BrVec3Length(BrVec3 *pV);
void BrVec3ScaleBy(BrVec3 *pV, float s);
float BrVec3Dist(BrVec3 *pA, BrVec3 *pB);
float sqrtf(float x);
void BrCamShakeAdd(int slot, float s);
/* -- end declarations -- */

#define PAD ((BrCarInput *)car->pad)
#define VIEWED (car->slot == D_8031B2C8[0].car || (D_8028AB0C >= 2 && car->slot == D_8031B2C8[1].car))

/* WHAT IT DOES: One frame of a car's control from its pad: the steering and
 * throttle axes squared past their dead zones (steering mirrored with the
 * picture), and for a car on screen the camera: the nearest checkpoint below
 * it within 150 becomes the chase camera's target (the camera redrawn when
 * that changes), or with none near the camera alternates between the two
 * fallback views for 60 frames; the camera buttons pick a view, and the
 * d-pad and the brake drive the free-flying camera instead while the car is
 * on it.  In the demo mode the car drives itself along the racing line to
 * the finish.  Then the physics step, the head bob spring between the two
 * probe frames (a hard knock shakes the camera), and the respawn check.
 * The PC twin is FUN_1005c8b0.  The unused arrays place the named locals
 * where the ROM frame has them (b 0xC4, la 0x90, v 0x7C).
 * RESIDUE (546): every slot is 8 above the ROM's (its spill area is 8
 * smaller), the locked-camera test branches to the scan where the ROM
 * branches (likely) to the buttons, the fallback-camera toggle is formed
 * after the camera stores, and the +-1.0 constants are materialised per
 * use. */
/* @t4-pass 0x80226488 1 2026-09-29 compiles 185 best 546 moved 0  (tools/tgrally/n64permute.py) */
/* @t4-pass 0x80226488 2 2026-09-29 compiles 185 best 546 moved 0  (tools/tgrally/n64permute.py) */
/* @t3 0x80226488 */
/* @implements 0x80226488 tgr BrCarPlayerCtl */
void BrCarPlayerCtl(BrCar *car)
{
  float b[3];
  int u0[10];
  BrVec3 la;
  int u1[2];
  BrVec3 v;
  BrVec3 prev;
  BrVec3 bb;
  BrVec3 a;
  BrVec3 *p;
  float best;
  float dx;
  float dy;
  float dz;
  float d;
  float s;
  int i;

  if (D_8028A8AC != 0) {
    PAD->steer = -PAD->steer;
  }
  if (PAD->steer > 0.1f) {
    car->x1dd4 = (PAD->steer - 0.1f) / 0.9f;
    car->x1dd4 = car->x1dd4 * -car->x1dd4;
  } else if (PAD->steer < -0.1f) {
    car->x1dd4 = (PAD->steer + 0.1f) / 0.9f;
    car->x1dd4 = car->x1dd4 * car->x1dd4;
  } else {
    car->x1dd4 = 0.0f;
  }
  if ((car->link->flags & 2) && VIEWED) {
    if (D_8028B7F8 != 0) {
      goto buttons;
    }
    goto scan;
  }
  if ((D_8026FF18 == 4 || D_8026FF18 == 5 || D_80270788 != 0) && VIEWED) {
    if (D_8028B7F8 == 0) {
scan:
      best = 16777216.0f;
      for (i = 0; i < D_80025C88; i++) {
        dx = car->mtx0[3][0] - D_80025C84[i].x;
        dy = car->mtx0[3][1] - D_80025C84[i].y;
        dx = dx * dx;
        dz = car->mtx0[3][2] - D_80025C84[i].z;
        dy = dy * dy;
        if (!(dz > 10.0f) && (d = dx + dy + dz * dz) < best) {
          b[0] = D_80025C84[i].x;
          b[1] = D_80025C84[i].y;
          b[2] = D_80025C84[i].z;
          best = d;
        }
      }
      if (best < 22500.0f) {
        if (car->cam != &car->cams[3] || car->cams[3].mtx[3][0] != b[0] || car->cams[3].mtx[3][1] != b[1] ||
            car->cams[3].mtx[3][2] != b[2]) {
          car->xf48 = 1;
        }
        car->cams[3].mtx[3][0] = b[0];
        car->cams[3].mtx[3][1] = b[1];
        car->cams[3].mtx[3][2] = b[2];
        car->cam = &car->cams[3];
      } else if (car->cam == &car->cams[3]) {
        car->xf48 = 1;
        if (D_8028B9EC == 0 && D_8028B7FC == 0) {
          car->cam = &car->cams[1];
        } else {
          car->cam = &car->cams[0];
        }
        D_8028B7F8 = 60;
        D_8028B9EC = D_8028B9EC == 0;
      }
    } else {
      D_8028B7F8--;
    }
    if (PAD->flags & 0xF000000) {
      D_8028B7F8 = 450;
    }
  } else if (VIEWED) {
    D_8028B7F8 = 0;
  }
buttons:
  if (PAD->flags & 0x1000000) {
    car->cam = &car->cams[0];
    car->xf48 = 1;
    BrPadConsume(PAD, 0x1000000);
  }
  if (PAD->flags & 0x2000000) {
    BrCarCamPlaceChase(car);
    car->xf48 = 1;
    BrPadConsume(PAD, 0x2000000);
  }
  if (PAD->flags & 0x4000000) {
    car->cam = &car->cams[1];
    car->xf48 = 1;
    BrPadConsume(PAD, 0x4000000);
  }
  if (D_8028AB0C == 1 && (PAD->flags & 0x8000000)) {
    car->cam = &car->cams[2];
    car->xf48 = 1;
    BrPadConsume(PAD, 0x8000000);
  }
  if (PAD->throttle > 0.25f) {
    car->x1ddc = (PAD->throttle - 0.25f) / 0.75f;
    car->x1ddc = car->x1ddc * car->x1ddc;
  } else if (PAD->throttle < -0.25f) {
    car->x1ddc = (PAD->throttle + 0.25f) / 0.75f;
    car->x1ddc = car->x1ddc * -car->x1ddc;
  } else {
    car->x1ddc = 0.0f;
  }
  if (car->xf4c != 0) {
    car->x1de0 = 0.0f;
  }
  if (PAD->flags & 0x8000) {
    if (car->xf4c == 0) {
      car->x1ddc = 1.0f;
    }
  } else if (car->xf4c != 0) {
    car->x1dd8 = car->x1ddc;
    car->x1ddc = 0.0f;
  }
  if (PAD->flags & 8) {
    car->x1ddc = 1.0f;
  }
  if (PAD->flags & 2) {
    car->x1ddc = -1.0f;
  }
  if (PAD->flags & 1) {
    if (car->xf4c != 0) {
      car->x1de0 = 1.0f;
    } else {
      car->x1dd4 = -1.0f;
    }
  }
  if (PAD->flags & 4) {
    if (car->xf4c != 0) {
      car->x1de0 = -1.0f;
    } else {
      car->x1dd4 = 1.0f;
    }
  }
  if (D_8026FF18 == 5) {
    BrVec3MulAddTo((BrVec3 *)car->mtx0[3], car->mtx0[0], 15.0f);
    BrCarLineFit(car);
    la.x = car->lineDir.x;
    la.y = car->lineDir.y;
    la.z = car->lineDir.z;
    BrVec3MulAddTo((BrVec3 *)car->mtx0[3], car->mtx0[0], -15.0f);
    BrCarLineFit(car);
    PAD->flags &= 0xF0C0FFFF;
    if (car->xfa8 < 114.5f) {
      BrVec3Scale(&v, &car->lineDir, 27.0f);
    } else {
      BrVec3Scale(&v, &car->lineDir, (168.5f - car->xfa8) * 0.5f);
      if (167.5f < car->xfa8) {
        PAD->flags |= 0x40000;
        if (168.4f < car->xfa8) {
          v.y = v.x = v.z = 0.0f;
        }
      } else {
        PAD->flags |= 0x80000;
      }
    }
    car->xdf0 = BrVec3Dot(&la, car->mtx0[1]) * 3.0f;
    v.x = v.x - car->lineRel.x * 0.2f;
    v.y = v.y - car->lineRel.y * 0.2f;
    BrCarSetVel(car, v.x, v.y, v.z);
  }
  BrCarPhysTick(car);
  if (car->xf4c == 0) {
    BrVec3Add(&a, (BrVec3 *)car->mtx0[3], (BrVec3 *)car->mtx0[0]);
    BrVec3AddTo(&a, car->mtx0[1]);
    BrVec3Add(&bb, (BrVec3 *)&car->xfe4[2], (BrVec3 *)&car->xfe4[5]);
    prev.x = car->xfe4[8];
    prev.y = car->xfe4[9];
    prev.z = car->xfe4[10];
    BrVec3Sub((BrVec3 *)&car->xfe4[8], (BrVec3 *)&car->xfe4[2], &a);
    d = BrVec3Length((BrVec3 *)&car->xfe4[8]);
    if (d != 0.0f) {
      BrVec3ScaleBy((BrVec3 *)&car->xfe4[8], d / (d + 1.0f) / d);
    }
    s = sqrtf(BrVec3Dist((BrVec3 *)&car->xfe4[8], &prev));
    s = s / (BrVec3Length((BrVec3 *)&car->xfe4[5]) + 1.0f);
    if (0.025f < s) {
      BrCamShakeAdd(car->slot, s + s);
    }
    car->xfe4[2] = a.x;
    car->xfe4[3] = a.y;
    car->xfe4[4] = a.z;
    BrVec3Sub((BrVec3 *)&car->xfe4[5], (BrVec3 *)&car->xfe4[2], &bb);
    car->xfe4[7] -= 0.32666668f;
  }
  BrCarRespawn(car);
}

#undef PAD
#undef VIEWED

/* WHAT IT DOES: Put a car at (x, y, z): the body matrix's translation, the
 * spare copy at 0x1D78 and the three rigid-body states, then rebuild the
 * state's matrix.  Same store order as the PC twin BrEntSetPos
 * (br_entpos.c). */
/* @implements 0x8021FE04 tgr BrCarSetPos */
void BrCarSetPos(BrCar *car, float x, float y, float z)
{
  car->mtx0[3][0] = x;
  car->mtx0[3][1] = y;
  car->mtx0[3][2] = z;
  car->pos1d78.x = x;
  car->pos1d78.y = y;
  car->pos1d78.z = z;
  car->st.pos.x = x;
  car->st.pos.y = y;
  car->st.pos.z = z;
  car->stB.pos.x = x;
  car->stB.pos.y = y;
  car->stB.pos.z = z;
  car->stA.pos.x = x;
  car->stA.pos.y = y;
  car->stA.pos.z = z;
  BrQuatToMat(car->stMtx, &car->st);
}

/* WHAT IT DOES: Turn a car to face heading a (radians, about z): the body
 * matrix gets the z rotation, and all three rigid-body states get the
 * matching half-angle quaternion.  m only holds the ROM's frame size. */
/* @implements 0x8021FE80 tgr BrCarSetHeading */
void BrCarSetHeading(BrCar *car, float a)
{
  float c;
  float s;
  float c2;
  float s2;
  float m[10];                   /* unused: holds the frame size */

  c = cosf(a);
  s = sinf(a);
  c2 = cosf(a + 1.5707964f);
  s2 = sinf(a + 1.5707964f);
  car->mtx0[0][0] = c;
  car->mtx0[0][1] = s;
  car->mtx0[0][2] = 0.0f;
  car->mtx0[1][0] = c2;
  car->mtx0[1][2] = 0.0f;
  car->mtx0[2][1] = 0.0f;
  car->mtx0[2][0] = 0.0f;
  car->mtx0[1][1] = s2;
  car->mtx0[2][2] = 1.0f;
  car->st.q[0] = cosf(a * 0.5f);
  car->st.q[2] = 0.0f;
  car->st.q[1] = 0.0f;
  car->stA.q[3] = car->stB.q[3] = car->st.q[3] = sinf(a * 0.5f);
  car->stB.q[0] = car->st.q[0];
  car->stB.q[1] = car->st.q[1];
  car->stB.q[2] = car->st.q[2];
  car->stA.q[0] = car->st.q[0];
  car->stA.q[1] = car->st.q[1];
  car->stA.q[2] = car->st.q[2];
  BrQuatToMat(car->stMtx, &car->st);
}

/* WHAT IT DOES: Turn a rotation matrix into a unit quaternion (w, x, y, z)
 * by the largest-diagonal method: pick the branch whose component is biggest
 * (w when the trace is non-negative, else x, y or z), build the four
 * unnormalised components from the diagonal sum and the off-diagonal sums and
 * differences, then normalise. */
/* @implements 0x8021FF90 tgr BrMatToQuat */
void BrMatToQuat(float m[4][4], float q[4])
{
  if (m[0][0] >= 0.0f) {
    if (m[1][1] + m[2][2] >= 0.0f) {
      q[0] = 1.0f + m[0][0] + m[1][1] + m[2][2];
      q[1] = m[1][2] - m[2][1];
      q[2] = m[2][0] - m[0][2];
      q[3] = m[0][1] - m[1][0];
    } else {
      q[0] = m[1][2] - m[2][1];
      q[1] = 1.0f + m[0][0] - m[1][1] - m[2][2];
      q[2] = m[0][1] + m[1][0];
      q[3] = m[2][0] + m[0][2];
    }
  } else if (m[1][1] >= m[2][2]) {
    q[0] = m[2][0] - m[0][2];
    q[1] = m[0][1] + m[1][0];
    q[2] = 1.0f - m[0][0] + m[1][1] - m[2][2];
    q[3] = m[1][2] + m[2][1];
  } else {
    q[0] = m[0][1] - m[1][0];
    q[1] = m[2][0] + m[0][2];
    q[2] = m[1][2] + m[2][1];
    q[3] = 1.0f - m[0][0] - m[1][1] + m[2][2];
  }
  BrVec4Normalise(q);
}

/* WHAT IT DOES: Set a car's velocity: its three rigid-body states and the
 * spare copy at 0xFD8 all get (x, y, z).  The copy's pointer is taken twice
 * (first and again before use); with only either one the last two stores
 * swap. */
/* @implements 0x802201C8 tgr BrCarSetVel */
void BrCarSetVel(BrCar *car, float x, float y, float z)
{
  BrVec3 *v;

  v = &car->velfd8;
  car->st.vel.x = x;
  car->st.vel.y = y;
  car->st.vel.z = z;
  car->stB.vel.x = x;
  car->stB.vel.y = y;
  car->stB.vel.z = z;
  car->stA.vel.x = x;
  car->stA.vel.y = y;
  car->stA.vel.z = z;
  v = &car->velfd8;
  v->x = x;
  v->y = y;
  v->z = z;
}

/* WHAT IT DOES: Turn a car by angles about z, y and x in that order (each
 * as a half-angle quaternion multiplied onto its orientation), renormalise,
 * and copy the orientation into the two spare states.
 *
 * NEVER RUN IN THE RETAIL GAME: its one caller, BrCarPhysTick, reaches it
 * only while car->xf4c (0xF4C) is non-zero; no instruction in the ROM
 * stores to offset 0xF4C, and a write watch on all four cars' 0xF4C over
 * all 64 box scripts saw no store. */
/* @implements 0x8022021C tgr BrCarRotate */
void BrCarRotate(BrCar *car, float az, float ay, float ax)
{
  float q[4];
  float s;

  az *= 0.5f;
  ay *= 0.5f;
  ax *= 0.5f;
  s = sinf(az);
  q[0] = cosf(az);
  q[1] = 0.0f;
  q[2] = 0.0f;
  q[3] = s;
  BrQuatMul(car->st.q, car->st.q, q);
  s = sinf(ay);
  q[0] = cosf(ay);
  q[1] = 0.0f;
  q[2] = s;
  q[3] = 0.0f;
  BrQuatMul(car->st.q, car->st.q, q);
  s = sinf(ax);
  q[0] = cosf(ax);
  q[1] = s;
  q[2] = 0.0f;
  q[3] = 0.0f;
  BrQuatMul(car->st.q, car->st.q, q);
  BrVec4Normalise(car->st.q);
  car->stB.q[0] = car->st.q[0];
  car->stB.q[1] = car->st.q[1];
  car->stB.q[2] = car->st.q[2];
  car->stB.q[3] = car->st.q[3];
  car->stA.q[0] = car->st.q[0];
  car->stA.q[1] = car->st.q[1];
  car->stA.q[2] = car->st.q[2];
  car->stA.q[3] = car->st.q[3];
}

/* WHAT IT DOES: Set a car's angular velocity in all three rigid-body
 * states.  The PC twin is BrEntSetAngVel (br_entstate.c). */
/* @implements 0x80220358 tgr BrCarSetAngVel */
void BrCarSetAngVel(BrCar *car, float x, float y, float z)
{
  car->st.angVel.x = x;
  car->st.angVel.y = y;
  car->st.angVel.z = z;
  car->stB.angVel.x = x;
  car->stB.angVel.y = y;
  car->stB.angVel.z = z;
  car->stA.angVel.x = x;
  car->stA.angVel.y = y;
  car->stA.angVel.z = z;
}
