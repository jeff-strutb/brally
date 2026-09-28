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
