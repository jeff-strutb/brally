/* carcam.c -- choosing where the camera looks from for a car
 */
#include "tgr/common.h"
#include "tgr/car.h"

/* -- declarations -- */
void func_80226488(int ent);
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
  BrVec3 *v0;                   /* 0x10  its corners */
  BrVec3 *v1;
  BrVec3 *v2;
  int x1c;
} BrCamPlane;
extern BrCamPlane D_80379F80[][150];    /* each grid cell's collision planes */
extern unsigned short D_8037EA88[];     /* and how many */
extern int D_8028B710;                  /* the camera was pushed out of a wall */
int func_8025F18C(float x, float y);
void BrVec3MulAdd(BrVec3 *pOut, BrVec3 *pA, BrVec3 *pB, float s);
float BrVec3Length(BrVec3 *v);
float BrVec3Dot(BrVec3 *a, BrVec3 *b);
float BrVec3Dist(BrVec3 *a, BrVec3 *b);
void BrVec3Add(BrVec3 *out, BrVec3 *a, BrVec3 *b);
int BrTriContainsPoint(BrVec3 *pPt, BrVec3 *pA, BrVec3 *pB, BrVec3 *pC, BrVec3 *pRef);
/* -- end declarations -- */

/* WHAT IT DOES: Update the camera placement for one car: an out-of-line
 * entry to the camera routine, used by the race and by the front-end
 * screens that show a car. */
/* @implements 0x80226D7C tgr BrCarCamStep */
void BrCarCamStep(int ent)
{
    func_80226488(ent);
}

/* WHAT IT DOES: Clear the car's camera-cut word (+0xF48), which the camera
 * routine sets when it switches to a new viewpoint. The race clears it at
 * the start. */
/* @implements 0x8022BCAC tgr BrCarCamClearCut */
void BrCarCamClearCut(int param_1)
{
  *(int *)(param_1 + 0xf48) = 0;
}

/* WHAT IT DOES: Does nothing with its argument. The race still calls it
 * once per car when a race starts; its body was compiled out. */
/* @implements 0x8022BA98 tgr BrStub8022BA98 */
void BrStub8022BA98(int arg0)
{
}

/* WHAT IT DOES: Add to the camera shake for view n: at most 2.5 per call,
 * and the total never goes above 5. */
/* @implements 0x8021BE28 tgr BrCamShakeAdd */
void BrCamShakeAdd(int cam, float amount)
{
  if (amount > 2.5f) {
    amount = 2.5f;
  }
  D_8031B1D8[cam] += amount;
  if (D_8031B1D8[cam] > 5.0f) {
    D_8031B1D8[cam] = 5.0f;
  }
}


/* WHAT IT DOES: Switch a car to its fourth camera and place it 6 units along
 * the body's first axis, 2 along its second and 1 along its third from the
 * car's position (camera mode 2).  The PC twin is BrVec3Predict. */
/* @implements 0x80221108 tgr BrCarCamPlaceChase */
void BrCarCamPlaceChase(BrCar *car)
{
  car->cam = &car->cams[3];
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
    car->cam = &car->cams[1];
    car->cam2 = &car->cams[1];
  } else {
    car->cam = &car->cams[0];
    car->cam2 = &car->cams[0];
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
  cells[0] = func_8025F18C(car->camTarget.x, car->camTarget.y);
  cells[1] = func_8025F18C(cam->mtx[3][0], cam->mtx[3][1]);
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
        BrVec3Sub(&toV0, pPlane->v0, &car->camTarget);
        t = BrVec3Dot(&toV0, &pPlane->n) / denom;
        if (t > 0.0f && t < tBest) {
          BrVec3MulAdd(&hit, &car->camTarget, &dir, t);
          if (BrTriContainsPoint(&hit, pPlane->v0, pPlane->v1, pPlane->v2, &pPlane->n)) {
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
        BrVec3Sub(&toV0, pPlane->v0, prev);
        t = BrVec3Dot(&toV0, &pPlane->n) / denom;
        if (t > 0.0f && t < tBest) {
          BrVec3MulAdd(&hit, prev, &dir, t);
          if (BrTriContainsPoint(&hit, pPlane->v0, pPlane->v1, pPlane->v2, &pPlane->n)) {
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
