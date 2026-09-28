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
 * matching half-angle quaternion.  m only holds the ROM's frame size.
 * RESIDUE (46): scheduling only -- the ROM loads the saved sin/cos just
 * before each store and the quaternion copies use f18/f16; same
 * instructions and store order. */
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
  car->mtx0[0][2] = 0.0f;
  car->mtx0[0][1] = s;
  car->mtx0[1][1] = s2;
  car->mtx0[1][2] = 0.0f;
  car->mtx0[2][1] = 0.0f;
  car->mtx0[2][0] = 0.0f;
  car->mtx0[2][2] = 1.0f;
  car->mtx0[1][0] = c2;
  car->st.q[0] = cosf(a * 0.5f);
  car->st.q[2] = 0.0f;
  car->st.q[1] = 0.0f;
  car->stA.q[3] = car->stB.q[3] = car->st.q[3] = sinf(a * 0.5f);
  car->stA.q[0] = car->stB.q[0] = car->st.q[0];
  car->stA.q[1] = car->stB.q[1] = car->st.q[1];
  car->stA.q[2] = car->stB.q[2] = car->st.q[2];
  BrQuatToMat(car->stMtx, &car->st);
}

/* WHAT IT DOES: Set a car's velocity: its three rigid-body states and the
 * spare copy at 0xFD8 all get (x, y, z). */
/* @implements 0x802201C8 tgr BrCarSetVel */
void BrCarSetVel(BrCar *car, float x, float y, float z)
{
  BrVec3 *v;

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
