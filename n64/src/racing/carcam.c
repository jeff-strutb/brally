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
