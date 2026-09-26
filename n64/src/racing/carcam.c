/* carcam.c -- choosing where the camera looks from for a car
 */
#include "tgr/common.h"

/* -- declarations -- */
void func_80226488(int ent);
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
/* @t4-pass 0x8021BE28 1 2026-09-26 compiles 17 best 35 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x8021BE28 2 2026-09-26 compiles 17 best 35 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x8021BE28 3 2026-09-26 compiles 15 best 35 moved 0  (n64/tools/n64permute.py) */
/* @implements 0x8021BE28 tgr BrCamShakeAdd */
void BrCamShakeAdd(int param_1,float param_2)
{
  float *pfVar1;
  float fVar2;
  
  pfVar1 = (float *)(param_1 * 4 + -0x7fce4e28);
  if (2.5 < param_2) {
    param_2 = 2.5;
    fVar2 = *pfVar1;
  }
  else {
    fVar2 = *pfVar1;
  }
  *pfVar1 = fVar2 + param_2;
  if (5.0 < *pfVar1) {
    *pfVar1 = 5.0;
  }
}
