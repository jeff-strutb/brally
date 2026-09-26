/* tyre.c -- tyre grip, skids and surface effects
 */
#include "tgr/common.h"

/* -- declarations -- */
float sqrtf(float param_1);
extern int D_8025E8FC;
extern int D_8025E900;
extern int D_8028C800;
/* -- end declarations -- */

/* WHAT IT DOES: Decide whether the car's tyres are skidding: from the
 * body's sideways speed and each wheel's surface, and start or stop the
 * skid sound and marks. */
/* @implements 0x8025E820 tgr BrTyreSkidCheck */
void BrTyreSkidCheck(int param_1,int param_2)
{
  char cVar1;
  char cVar2;
  char cVar3;
  char cVar4;
  float fVar5;
  float fVar6;
  
  *(float *)(param_2 + 8) = *(float *)(param_1 + 0x84) * -110.0;
  *(float *)(param_2 + 0xc) = *(float *)(param_1 + 0x88) * -110.0;
  *(float *)(param_2 + 0x10) = *(float *)(param_1 + 0x8c) * -110.0;
  cVar1 = *(char *)(*(int *)(param_1 + 4) + 0x1a0);
  cVar2 = *(char *)(*(int *)(param_1 + 8) + 0x1a0);
  cVar3 = *(char *)(*(int *)(param_1 + 0xc) + 0x1a0);
  cVar4 = *(char *)(*(int *)(param_1 + 0x10) + 0x1a0);
  fVar5 = (float)sqrtf(*(float *)(param_1 + 0x8c) * *(float *)(param_1 + 0x8c) +
                              *(float *)(param_1 + 0x84) * *(float *)(param_1 + 0x84) +
                              *(float *)(param_1 + 0x88) * *(float *)(param_1 + 0x88));
  if (((4.0 < fVar5) && (D_8028C800 != 3)) &&
     ((cVar1 == '\x04' || (((cVar2 == '\x04' || (cVar3 == '\x04')) || (cVar4 == '\x04')))))) {
    fVar6 = *(float *)(param_1 + 0x88);
    fVar5 = *(float *)(param_1 + 0x8c);
    *(float *)(param_2 + 8) = *(float *)(param_2 + 8) + *(float *)(param_1 + 0x84) * -220.0;
    *(float *)(param_2 + 0xc) = *(float *)(param_2 + 0xc) + fVar6 * -220.0;
    *(float *)(param_2 + 0x10) = *(float *)(param_2 + 0x10) + fVar5 * -220.0;
  }
}
