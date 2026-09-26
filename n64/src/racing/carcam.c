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
