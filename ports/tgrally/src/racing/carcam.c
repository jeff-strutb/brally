/* carcam.c -- choosing where the camera looks from for a car
 */
#include "tgr/common.h"
#include "tgr/car.h"
#include "tgr/track.h"

/* -- declarations -- */
void BrCarPlayerCtl(void *);
extern float D_8031B1D8[];
void BrVec3MulAdd(BrVec3 *pOut, BrVec3 *pA, BrVec3 *pB, float s);
void BrVec3MulAddTo(BrVec3 *pA, BrVec3 *pB, float s);
void BrVec3AddTo(BrVec3 *pA, BrVec3 *pB);
void BrVec3SubFrom(BrVec3 *pA, BrVec3 *pB);
extern int D_8026FF18;                  /* the game mode */
extern int D_80270788;
void BrVec3Sub(BrVec3 *out, BrVec3 *a, BrVec3 *b);
float BrVec3Length(BrVec3 *v);
void BrVec3Div(BrVec3 *out, BrVec3 *v, float d);
void BrVec3Cross(BrVec3 *out, BrVec3 *a, BrVec3 *b);
void BrVec3Scale(BrVec3 *out, BrVec3 *v, float s);
void BrVec3ScaleBy(BrVec3 *v, float s);
void BrVec3Lerp(BrVec3 *out, BrVec3 *a, BrVec3 *b, float t);
extern int D_8028AB0C;                  /* the close camera mode */
typedef struct BrCamPlane {     /* a collision triangle's plane (0x20 bytes) */
  BrVec3 n;                     /* its normal */
  float d;
  TgrAddr v0;                   /* BrVec3 * -- 0x10  its corners */
  TgrAddr v1;  /* BrVec3 * */
  TgrAddr v2;  /* BrVec3 * */
  int x1c;
} BrCamPlane;
extern BrCamPlane D_80379F80[][150];    /* each grid cell's collision planes */
extern unsigned short D_8037EA88[];     /* and how many */
extern int D_8028B710;                  /* the camera was pushed out of a wall */
short BrCollGridCellAcquire(float, float);
void BrVec3MulAdd(BrVec3 *pOut, BrVec3 *pA, BrVec3 *pB, float s);
float BrVec3Length(BrVec3 *v);
float BrVec3Dot(BrVec3 *a, BrVec3 *b);
float BrVec3Dist(BrVec3 *a, BrVec3 *b);
void BrVec3Add(BrVec3 *out, BrVec3 *a, BrVec3 *b);
int BrTriContainsPoint(BrVec3 *pPt, BrVec3 *pA, BrVec3 *pB, BrVec3 *pC, BrVec3 *pRef);
extern int D_8028B7F8;                  /* camera hold timers */
extern int D_8028B7FC;
extern float D_8028AAC0;                /* the lens scale */
void BrCarCamTargetStep(BrCar *car);
void BrCarCamWallPush(BrCar *car, BrCarCam *cam, BrVec3 *prev);
void BrCarCamPlaceBehind(BrCar *car, BrCarCam *cam, float t);
void BrCarCamLookAt(BrCar *car, BrCarCam *cam);
void BrVec3Negate(BrVec3 *out, BrVec3 *v);
void BrVec3Normalise(BrVec3 *v);
void BrVec3DivBy(BrVec3 *v, float d);
float cosf(float x);
typedef struct BrCamView {      /* the lens offsets in a car's model buffer */
  char pad00[0xB0];
  bef_t xb0;                    /* 0xB0 */
  bef_t xb4;                    /* 0xB4 */
  bef_t xb8;                    /* 0xB8 */
} BrCamView;                    /* (cartridge data: big-endian) */
/* -- end declarations -- */

/* WHAT IT DOES: Update the camera placement for one car: an out-of-line
 * entry to the camera routine, used by the race and by the front-end
 * screens that show a car. */
/* @implements 0x80226D7C tgr BrCarCamStep */
void BrCarCamStep(BrCar *car)
{
    BrCarPlayerCtl(car);
}

/* WHAT IT DOES: Clear the car's camera-cut word (+0xF48), which the camera
 * routine sets when it switches to a new viewpoint. The race clears it at
 * the start. */
/* @implements 0x8022BCAC tgr BrCarCamClearCut */
void BrCarCamClearCut(BrCar *car)
{
  car->xf48 = 0;
}

/* WHAT IT DOES: Does nothing with its argument. The race still calls it
 * once per car when a race starts; its body was compiled out. */
/* @implements 0x8022BA98 tgr BrStub8022BA98 */
void BrStub8022BA98(int arg0)
{
}


/* WHAT IT DOES: Switch a car to its fourth camera and place it 6 units along
 * the body's first axis, 2 along its second and 1 along its third from the
 * car's position (camera mode 2).  The PC twin is BrVec3Predict. */
/* @implements 0x80221108 tgr BrCarCamPlaceChase */
void BrCarCamPlaceChase(BrCar *car)
{
  car->cam = tgr_addr32(&car->cams[3]);
  BrVec3MulAdd((BrVec3 *)car->cams[3].mtx[3], (BrVec3 *)car->mtx0[3], (BrVec3 *)car->mtx0[0], 6.0f);
  BrVec3MulAddTo((BrVec3 *)car->cams[3].mtx[3], (BrVec3 *)car->mtx0[1], 2.0f);
  BrVec3AddTo((BrVec3 *)car->cams[3].mtx[3], (BrVec3 *)car->mtx0[2]);
  car->xf48 = 2;
}

/* WHAT IT DOES: Set a car's cameras up for a new race: the chase camera
 * (the second one in mode 5) is the one in use and the one to return to;
 * its position starts 4 units up the car's up axis (and, when asked, 10
 * along its facing), relative to the car; the in-car camera and two saved
 * copies start there too. */
/* @implements 0x80221864 tgr BrCarCamInit */
void BrCarCamInit(BrCar *car)
{
  BrVec3 v;
  float x;
  float y;
  float z;

  if (D_8026FF18 == 5) {
    car->cam = tgr_addr32(&car->cams[1]);
    car->cam2 = tgr_addr32(&car->cams[1]);
  } else {
    car->cam = tgr_addr32(&car->cams[0]);
    car->cam2 = tgr_addr32(&car->cams[0]);
  }
  BrVec3MulAdd((BrVec3 *)car->cams[1].mtx[3], (BrVec3 *)car->mtx0[3], (BrVec3 *)car->mtx0[2], 4.0f);
  if (D_80270788 != 0) {
    BrVec3MulAddTo((BrVec3 *)car->cams[1].mtx[3], (BrVec3 *)car->mtx0[0], 10.0f);
  }
  BrVec3SubFrom((BrVec3 *)car->cams[1].mtx[3], (BrVec3 *)car->mtx0[0]);
  x = car->cams[1].mtx[3][0];
  y = car->cams[1].mtx[3][1];
  z = car->cams[1].mtx[3][2];
  car->camPosB.x = car->camPosA.x = car->cams[3].mtx[3][0] = x;
  car->camPosB.y = car->camPosA.y = car->cams[3].mtx[3][1] = y;
  car->camPosB.z = car->camPosA.z = car->cams[3].mtx[3][2] = z;
  car->x1fac = 0.0f;
  car->x1f90 = 2.0f;
}

/* WHAT IT DOES: Advance a car's chase camera by a frame: smooth the car's
 * spin and speed into the camera's damped copies, place it behind the car
 * lifted by how far the smoothed spin outruns the speed, step the target,
 * push it out of walls and settle its height, look at the car, then rebuild
 * the camera frame (a copy of the car's matrix when locked, else looking
 * back from 20 behind), its copy, the eye basis, the lens from the eye
 * distance, and the negated axis rows the renderer reads.  Ported from the
 * PC twin (BrCamChaseStep); pos is a dead copy the ROM keeps, u is unused.
 * RESIDUE (140): the ROM spills five matrix loads to its temp area for the
 * closing axis rows (frame 0xB8 vs ours 0x70) and keeps &cams[1] as a spilled
 * temp; structure, calls and named slots match. */
/* @t4-pass 0x80221170 1 2026-09-29 compiles 13 best 140 moved 0  (tools/tgrally/n64permute.py) */
/* @t4-pass 0x80221170 2 2026-09-29 compiles 13 best 140 moved 0  (tools/tgrally/n64permute.py) */
/* @t3 0x80221170 */
/* @implements 0x80221170 tgr BrCamChaseStep */
void BrCamChaseStep(BrCar *car)
{
  float pos[3];
  float len2;
  float spin;
  float k;
  float s;
  float l;
  float dist;
  BrVec3 prev;
  BrCamView *pv;
  int u;
  float speed;

  prev.x = car->cams[1].mtx[3][0];
  prev.y = car->cams[1].mtx[3][1];
  prev.z = car->cams[1].mtx[3][2];
  speed = BrVec3Length(&car->st.angVel);
  len2 = BrVec3Length(&car->st.vel);
  spin = len2 * 0.3f;
  if (speed > 2.5f) {
    speed = speed - 2.5f;
  } else {
    speed = 0.0f;
  }
  if (speed > 31.415928f) {
    speed = 31.415928f;
  }
  if (spin > 31.415928f) {
    spin = 31.415928f;
  }
  if (car->camSpeed < speed) {
    car->camSpeed = speed;
  } else {
    car->camSpeed = car->camSpeed * 0.95f + speed * 0.05f;
  }
  car->camSpin = car->camSpin * 0.95f + spin * 0.05f;
  car->camSpin = car->camSpin * (1.0f - car->x1fac * 18.181818f);
  car->cams[1].mtx[3][0] = car->camPosA.x;
  car->cams[1].mtx[3][2] = car->camPosA.z;
  car->cams[1].mtx[3][1] = car->camPosA.y;
  k = (D_80270788 != 0 ? 0.5f : 0.15f) / 31.415928f;
  s = car->camSpeed * 0.009549296f;
  l = car->camSpin * k;
  if (car->x1fac + (s - l) > 0.07f) {
    BrCarCamPlaceBehind(car, &car->cams[1], 0);
  } else {
    BrCarCamPlaceBehind(car, &car->cams[1], 0.07f - car->x1fac - s + l);
  }
  BrCarCamTargetStep(car);
  car->camPosA.x = car->cams[1].mtx[3][0];
  car->camPosA.y = car->cams[1].mtx[3][1];
  car->camPosA.z = car->cams[1].mtx[3][2];
  if (car->xf4c == 0) {
    BrCarCamWallPush(car, &car->cams[1], &prev);
    if (D_8028B710 != 0) {
      if (D_80270788 != 0) {
        D_8028B7FC = 30;
        if (&car->cams[1] == TGR_PTR(BrCarCam *, car->cam)) {
          car->cam = tgr_addr32(&car->cams[0]);
          car->xf48 = 2;
          D_8028B7F8 = 60;
        }
      }
      if (car->x1fac < 0.02f) {
        car->x1fac = 0.02f;
      } else {
        car->x1fac = car->x1fac + 0.01f;
        if (car->x1fac > 0.055f) {
          car->x1fac = 0.055f;
        }
      }
      car->x1fac = 0.05f;
    } else {
      if (D_8028B7FC != 0) {
        D_8028B7FC--;
      }
      car->x1fac = car->x1fac - 0.005f;
      if (car->x1fac < 0.0f) {
        car->x1fac = 0.0f;
      }
    }
  }
  BrCarCamLookAt(car, &car->cams[1]);
  pos[0] = car->cams[0].mtx[3][0];
  pos[1] = car->cams[0].mtx[3][1];
  pos[2] = car->cams[0].mtx[3][2];
  if (car->xf4c != 0) {
    memcpy(&car->cams[0], car, sizeof(BrCarCam));
  } else {
    pv = (BrCamView *)TGR_PTR(char *, car->model);
    car->cams[0].mtx[3][0] = car->mtx0[3][0] + car->mtx0[0][0] * BEF(pv->xb0) + BEF(pv->xb8) * car->mtx0[2][0];
    car->cams[0].mtx[3][1] = car->mtx0[3][1] + car->mtx0[0][1] * BEF(pv->xb0) + BEF(pv->xb8) * car->mtx0[2][1];
    car->cams[0].mtx[3][2] = car->mtx0[3][2] + car->mtx0[0][2] * BEF(pv->xb0) + BEF(pv->xb8) * car->mtx0[2][2];
    BrVec3MulAddTo((BrVec3 *)&car->cams[0], (BrVec3 *)car, -20.0f);
    BrVec3Negate((BrVec3 *)&car->cams[0], (BrVec3 *)&car->cams[0]);
    BrVec3Normalise((BrVec3 *)&car->cams[0]);
    car->cams[0].mtx[1][0] = car->mtx0[1][0];
    car->cams[0].mtx[1][1] = car->mtx0[1][1];
    car->cams[0].mtx[1][2] = car->mtx0[1][2];
    BrVec3Cross((BrVec3 *)car->cams[0].mtx[2], (BrVec3 *)&car->cams[0], (BrVec3 *)car->cams[0].mtx[1]);
  }
  memcpy(&car->cams[2], &car->cams[0], sizeof(BrCarCam));
  BrVec3MulAddTo((BrVec3 *)car->cams[2].mtx[3], (BrVec3 *)car, 0.5f);
  car->cams[3].mtx[2][0] = 0.0f;
  car->cams[3].mtx[2][1] = 0.0f;
  car->cams[3].mtx[2][2] = 1.0f;
  BrVec3Add((BrVec3 *)car->cams[3].mtx[0], (BrVec3 *)car->mtx0[3], (BrVec3 *)car->mtx0[2]);
  BrVec3SubFrom((BrVec3 *)car->cams[3].mtx[0], (BrVec3 *)car->cams[3].mtx[3]);
  dist = BrVec3Length((BrVec3 *)car->cams[3].mtx[0]);
  BrVec3DivBy((BrVec3 *)car->cams[3].mtx[0], dist);
  BrVec3Cross((BrVec3 *)car->cams[3].mtx[1], (BrVec3 *)car->cams[3].mtx[2], (BrVec3 *)car->cams[3].mtx[0]);
  BrVec3Cross((BrVec3 *)car->cams[3].mtx[2], (BrVec3 *)car->cams[3].mtx[0], (BrVec3 *)car->cams[3].mtx[1]);
  if (dist <= 1.0f) {
    dist = 0.0f;
  } else if (dist >= 101.0f) {
    dist = 100.0f;
  } else {
    dist = dist - 1.0f;
  }
  car->cams[3].fov = ((cosf((dist - 1.0f) * 0.041887902f) * 0.1f + 0.7f) + (51.0f - dist) * 0.004f) * D_8028AAC0;
  car->cams[2].fov = D_8028AAC0;
  car->cams[1].fov = D_8028AAC0;
  car->cams[0].fov = D_8028AAC0;
  car->cam4.fov = D_8028AAC0;
  pv = (BrCamView *)TGR_PTR(char *, car->model);
  car->cam4.mtx[3][0] = BEF(pv->xb8) * car->mtx0[2][0] + (car->mtx0[3][0] + car->mtx0[0][0] * BEF(pv->xb4) * 2.0f);
  car->cam4.mtx[3][1] = BEF(pv->xb8) * car->mtx0[2][1] + (car->mtx0[3][1] + car->mtx0[0][1] * BEF(pv->xb4) * 2.0f);
  car->cam4.mtx[3][2] = BEF(pv->xb8) * car->mtx0[2][2] + (car->mtx0[3][2] + car->mtx0[0][2] * BEF(pv->xb4) * 2.0f);
  car->cam4.mtx[0][0] = -car->mtx0[0][0];
  car->cam4.mtx[0][1] = -car->mtx0[0][1];
  car->cam4.mtx[0][2] = -car->mtx0[0][2];
  car->cam4.mtx[1][0] = -car->mtx0[1][0];
  car->cam4.mtx[1][1] = -car->mtx0[1][1];
  car->cam4.mtx[1][2] = -car->mtx0[1][2];
  car->cam4.mtx[2][0] = car->mtx0[2][0];
  car->cam4.mtx[2][1] = car->mtx0[2][1];
  car->cam4.mtx[2][2] = car->mtx0[2][2];
  car->cam2 = car->cam;
}

/* WHAT IT DOES: Keep the chase camera out of walls: cast a ray from the
 * car's camera anchor to the camera against every collision triangle in the
 * grid cells of both ends (one if they share a cell), 0.1 of slack; on a
 * hit, a second ray from the camera's previous position finds the wall it
 * came through, the camera goes 0.1 in front of that wall along its normal,
 * and if that brought it nearer the anchor it is pushed back out to its old
 * distance along the new line.  Flags that the camera was moved.  Ported
 * from the PC twin; u0-u3 are declared and unused (the ROM frame keeps
 * their slots). */
/* @implements 0x80220810 tgr BrCarCamWallPush */
void BrCarCamWallPush(BrCar *car, BrCarCam *cam, BrVec3 *prev)
{
  BrVec3 dir;
  BrVec3 hit;
  BrVec3 toV0;
  int c;
  int cells[2];
  int nCells;
  BrCamPlane *pPlane;
  BrCamPlane *pEnd;
  BrCamPlane *pBest;
  float tBest;
  float denom;
  float t;
  BrVec3 hitOut;
  int u0;
  int u1;
  int u2;
  int u3;
  float len;
  float dist;

  D_8028B710 = 0;
  cells[0] = BrCollGridCellAcquire(car->camTarget.x, car->camTarget.y);
  cells[1] = BrCollGridCellAcquire(cam->mtx[3][0], cam->mtx[3][1]);
  if (cells[0] == cells[1]) {
    nCells = 1;
  } else {
    nCells = 2;
  }
  BrVec3Sub(&dir, (BrVec3 *)cam->mtx[3], &car->camTarget);
  pBest = 0;
  len = BrVec3Length(&dir);
  if (len != 0) {
    tBest = (len + 0.1f) / len;
  } else {
    tBest = 1.0f;
  }
  for (c = 0; c < nCells; c++) {
    pPlane = D_80379F80[cells[c]];
    pEnd = pPlane + D_8037EA88[cells[c]];
    for (; pPlane != pEnd; pPlane++) {
      denom = BrVec3Dot(&dir, &pPlane->n);
      if (denom < 0.0f) {
        BrVec3Sub(&toV0, BRV(TGR_PTR(BrVec3be *, pPlane->v0)), &car->camTarget);
        t = BrVec3Dot(&toV0, &pPlane->n) / denom;
        if (t > 0.0f && t < tBest) {
          BrVec3MulAdd(&hit, &car->camTarget, &dir, t);
          if (BrTriContainsPoint(&hit, BRV(TGR_PTR(BrVec3be *, pPlane->v0)), BRV(TGR_PTR(BrVec3be *, pPlane->v1)), BRV(TGR_PTR(BrVec3be *, pPlane->v2)), &pPlane->n)) {
            tBest = t;
            pBest = pPlane;
            hitOut.x = hit.x;
            hitOut.y = hit.y;
            hitOut.z = hit.z;
          }
        }
      }
    }
  }
  if (pBest == 0) {
    return;
  }
  BrVec3Sub(&dir, (BrVec3 *)cam->mtx[3], prev);
  pBest = 0;
  tBest = 1.0f;
  for (c = 0; c < nCells; c++) {
    pPlane = D_80379F80[cells[c]];
    pEnd = pPlane + D_8037EA88[cells[c]];
    for (; pPlane != pEnd; pPlane++) {
      denom = BrVec3Dot(&dir, &pPlane->n);
      if (denom < 0.0f) {
        BrVec3Sub(&toV0, BRV(TGR_PTR(BrVec3be *, pPlane->v0)), prev);
        t = BrVec3Dot(&toV0, &pPlane->n) / denom;
        if (t > 0.0f && t < tBest) {
          BrVec3MulAdd(&hit, prev, &dir, t);
          if (BrTriContainsPoint(&hit, BRV(TGR_PTR(BrVec3be *, pPlane->v0)), BRV(TGR_PTR(BrVec3be *, pPlane->v1)), BRV(TGR_PTR(BrVec3be *, pPlane->v2)), &pPlane->n)) {
            tBest = t;
            pBest = pPlane;
            hitOut.x = hit.x;
            hitOut.y = hit.y;
            hitOut.z = hit.z;
          }
        }
      }
    }
  }
  if (pBest == 0) {
    return;
  }
  dist = BrVec3Dist((BrVec3 *)cam->mtx[3], &car->camTarget);
  BrVec3MulAdd((BrVec3 *)cam->mtx[3], &hitOut, &pBest->n, 0.1f);
  {
    BrVec3 v;
    float l;

    BrVec3Sub(&v, (BrVec3 *)cam->mtx[3], &car->camTarget);
    l = BrVec3Length(&v);
    if (dist < l && l != 0.0f) {
      BrVec3ScaleBy(&v, dist / l);
      BrVec3Add((BrVec3 *)cam->mtx[3], &car->camTarget, &v);
    }
  }
  D_8028B710 = 1;
}

/* WHAT IT DOES: Place a car's camera behind it (the camera position is the
 * matrix's last row).  When the camera keeps the car's up axis: 2.4 up and
 * back along the car 11 in the close mode (0x8028AB0C == 1), else 19.8.
 * Otherwise: the old position, lowered 2.4, as an offset from the car scaled
 * to that distance, blended by t toward an ideal offset 11 long -- behind
 * the car, beside it when 0x80270788 is set, or below and behind in game
 * mode 5 -- then put back on the car and raised 2.4.  The old position is
 * also copied into a local that is never read. */
/* @implements 0x80220C58 tgr BrCarCamPlaceBehind */
void BrCarCamPlaceBehind(BrCar *car, BrCarCam *cam, float t)
{
  BrVec3 d;
  float old[3];
  float len;
  BrVec3 *pos;
  float unused;                 /* holds its frame slot */

  pos = (BrVec3 *)cam->mtx[3];
  if (car->xf4c != 0) {
    BrVec3MulAdd(pos, (BrVec3 *)car->mtx0[3], (BrVec3 *)car->mtx0[2], 2.4f);
    BrVec3MulAdd(pos, pos, (BrVec3 *)car->mtx0[0], D_8028AB0C == 1 ? -11.0f : -19.8f);
  } else {
    old[0] = cam->mtx[3][0];
    old[1] = cam->mtx[3][1];
    old[2] = cam->mtx[3][2];
    cam->mtx[3][2] -= 2.4f;
    BrVec3Sub(&d, pos, (BrVec3 *)car->mtx0[3]);
    len = BrVec3Length(&d);
    if (len != 0.0f) {
      if (D_8028AB0C == 1) {
        BrVec3ScaleBy(&d, 11.0f / len);
      } else {
        BrVec3ScaleBy(&d, 19.8f / len);
      }
    }
    if (D_80270788 != 0) {
      BrVec3Scale(pos, (BrVec3 *)car->mtx0[0], 11.0f);
    } else if (D_8026FF18 == 5) {
      BrVec3Scale(pos, (BrVec3 *)car->mtx0[1], -11.0f);
      BrVec3MulAddTo(pos, (BrVec3 *)car->mtx0[0], -13.0f);
    } else {
      BrVec3Scale(pos, (BrVec3 *)car->mtx0[0], -11.0f);
    }
    len = BrVec3Length(pos);
    if (len != 0.0f) {
      BrVec3ScaleBy(pos, 11.0f / len);
    }
    BrVec3Lerp(pos, pos, &d, t);
    BrVec3AddTo(pos, (BrVec3 *)car->mtx0[3]);
    cam->mtx[3][2] += 2.4f;
  }
}

/* WHAT IT DOES: Move a car's camera target: with the car's up axis kept,
 * 1.1 up that axis from the car; otherwise 0.66 above the car, pushed
 * ahead along its forward axis by a lead that eases (0.1 a frame) toward
 * 2 when the car turns slowly, down to 0 as it spins faster (none in
 * mode 5). */
/* @implements 0x80220E84 tgr BrCarCamTargetStep */
void BrCarCamTargetStep(BrCar *car)
{
  float spin;
  float want;

  if (car->xf4c != 0) {
    BrVec3MulAdd(&car->camTarget, (BrVec3 *)car->mtx0[3], (BrVec3 *)car->mtx0[2], 1.1f);
  } else {
    car->camTarget.x = car->mtx0[3][0];
    car->camTarget.y = car->mtx0[3][1];
    car->camTarget.z = car->mtx0[3][2] + 0.66f;
    spin = BrVec3Length(&car->st.angVel);
    if (D_8026FF18 != 5) {
      want = 0.0f;
      if (spin < 3.5f) {
        want = 2.0f;
      } else if (spin < 7.0f) {
        want = 4.0f - spin * 0.5714286f;
      }
      if (car->x1f90 < want) {
        car->x1f90 += 0.1f;
        if (car->x1f90 > want) {
          car->x1f90 = want;
        }
      } else if (car->x1f90 > want) {
        car->x1f90 -= 0.1f;
        if (car->x1f90 < want) {
          car->x1f90 = want;
        }
      }
      BrVec3MulAddTo(&car->camTarget, (BrVec3 *)car->mtx0[0], car->x1f90);
    }
  }
}

/* WHAT IT DOES: Aim a car's camera at its target: the forward row points
 * from the camera to the target (kept, or taken from the car's forward
 * axis if it is zero, when they coincide); the up row is the car's up axis
 * (or world z) crossed with it, and the third row completes the frame. */
/* @implements 0x80220FF4 tgr BrCarCamLookAt */
void BrCarCamLookAt(BrCar *car, BrCarCam *cam)
{
  BrVec3 v;
  float len;
  float spare[2];                 /* unused: it only holds stack slots */

  BrVec3Sub(&v, &car->camTarget, (BrVec3 *)cam->mtx[3]);
  len = BrVec3Length(&v);
  if (len != 0.0f) {
    BrVec3Div((BrVec3 *)cam->mtx[0], &v, len);
  } else if (BrVec3Length((BrVec3 *)cam->mtx[0]) == 0.0f) {
    cam->mtx[0][0] = car->mtx0[0][0];
    cam->mtx[0][1] = car->mtx0[0][1];
    cam->mtx[0][2] = car->mtx0[0][2];
  }
  if (car->xf4c != 0) {
    BrVec3Cross((BrVec3 *)cam->mtx[1], (BrVec3 *)car->mtx0[2], (BrVec3 *)cam->mtx[0]);
  } else {
    v.x = 0.0f;
    v.y = 0.0f;
    v.z = 1.0f;
    BrVec3Cross((BrVec3 *)cam->mtx[1], &v, (BrVec3 *)cam->mtx[0]);
  }
  BrVec3Cross((BrVec3 *)cam->mtx[2], (BrVec3 *)cam->mtx[0], (BrVec3 *)cam->mtx[1]);
}
