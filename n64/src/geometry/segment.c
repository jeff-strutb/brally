/* segment.c -- 2-D segment tests
 */
#include "tgr/common.h"

/* -- declarations -- */

/* -- end declarations -- */

/* WHAT IT DOES: Tell whether two line segments overlap on the ground plane:
 * a bounding-box rejection on x and y, then a side-of-line test of each
 * segment's ends against the other. */
/* @t4-pass 0x80225B64 1 2026-09-26 compiles 16 best 174 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x80225B64 2 2026-09-26 compiles 17 best 174 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x80225B64 3 2026-09-26 compiles 17 best 173 moved 1  (n64/tools/n64permute.py) */
/* @t4-pass 0x80225B64 4 2026-09-26 compiles 41 best 173 moved 0  (n64/tools/n64permute.py) */
/* @implements 0x80225B64 tgr BrSegmentsOverlapXY */
int BrSegmentsOverlapXY(float *param_1,float *param_2,float *param_3,float *param_4)
{
  int uVar1;
  float fVar2;
  float fVar3;
  float fVar4;
  float fVar5;
  float fVar6;
  float fVar7;
  float fVar8;
  float fVar9;
  float fVar10;
  float fVar11;
  
  fVar3 = *param_1;
  fVar2 = *param_2;
  fVar5 = fVar3;
  if (fVar3 <= fVar2) {
    fVar5 = fVar2;
  }
  fVar4 = *param_3;
  fVar6 = *param_4;
  fVar10 = fVar4;
  if (fVar6 <= fVar4) {
    fVar10 = fVar6;
  }
  if (fVar5 < fVar10) {
    uVar1 = 0;
  }
  else {
    fVar5 = fVar4;
    if (fVar4 <= fVar6) {
      fVar5 = fVar6;
    }
    fVar10 = fVar3;
    if (fVar2 <= fVar3) {
      fVar10 = fVar2;
    }
    if (fVar5 < fVar10) {
      uVar1 = 0;
    }
    else {
      fVar11 = param_2[1];
      fVar10 = param_1[1];
      fVar5 = fVar10;
      if (fVar10 <= fVar11) {
        fVar5 = fVar11;
      }
      fVar9 = param_3[1];
      fVar8 = param_4[1];
      fVar7 = fVar9;
      if (fVar8 <= fVar9) {
        fVar7 = fVar8;
      }
      if (fVar5 < fVar7) {
        uVar1 = 0;
      }
      else {
        fVar5 = fVar9;
        if (fVar9 <= fVar8) {
          fVar5 = fVar8;
        }
        fVar7 = fVar10;
        if (fVar11 <= fVar10) {
          fVar7 = fVar11;
        }
        if (fVar5 < fVar7) {
          uVar1 = 0;
        }
        else {
          fVar7 = (fVar4 - fVar3) * (fVar11 - fVar10) - (fVar2 - fVar3) * (fVar9 - fVar10);
          fVar5 = (fVar6 - fVar3) * (fVar11 - fVar10) - (fVar2 - fVar3) * (fVar8 - fVar10);
          if (((fVar7 == 0.0) || (fVar5 == 0.0)) || (fVar7 * fVar5 <= 0.0)) {
            fVar3 = (fVar3 - fVar4) * (fVar8 - fVar9) - (fVar6 - fVar4) * (fVar10 - fVar9);
            fVar2 = (fVar2 - fVar4) * (fVar8 - fVar9) - (fVar6 - fVar4) * (fVar11 - fVar9);
            if (((fVar3 == 0.0) || (fVar2 == 0.0)) || (fVar3 * fVar2 <= 0.0)) {
              uVar1 = 1;
              if (fVar7 == fVar5) {
                uVar1 = 2;
              }
            }
            else {
              uVar1 = 0;
            }
          }
          else {
            uVar1 = 0;
          }
        }
      }
    }
  }
  return uVar1;
}
