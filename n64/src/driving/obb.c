/* obb.c -- box-against-box collision
 */
#include "tgr/common.h"

/* -- declarations -- */
extern int D_8025C9E8;
extern int D_8025C9F8;
extern int D_8025CA00;
extern int D_8025CA14;
extern int D_8025CA24;
extern int D_8025CA2C;
extern int D_8025CA50;
extern int D_8025CA60;
extern int D_8025CA68;
extern int D_8025CA7C;
extern int D_8025CA8C;
extern int D_8025CA94;
extern int D_8025CAC0;
extern int D_8025CAD0;
extern int D_8025CAD8;
extern int D_8025CD68;
extern int D_8025CD78;
extern int D_8025CD80;
extern int D_8025CDA4;
extern int D_8025CDB4;
extern int D_8025CDBC;
/* -- end declarations -- */

/* WHAT IT DOES: Find the face of a box most facing a direction and clip it
 * against the other box: picks the axis with the largest component, then
 * walks that face's corners. */
/* @implements 0x8025C8DC tgr BrObbFaceClip */
int BrObbFaceClip(int param_1,float *param_2,int param_3)
{
  int iVar1;
  int iVar2;
  float *pfVar3;
  int iVar4;
  int iVar5;
  int iVar6;
  float *pfVar7;
  int iVar8;
  int iVar9;
  unsigned int uVar10;
  unsigned int uVar11;
  float fVar12;
  float fVar13;
  float fVar14;
  float fVar15;
  float fVar16;
  float fVar17;
  float afStackX_0 [4];
  float local_c [3];
  
  pfVar7 = local_c;
  pfVar3 = param_2;
  do {
    fVar12 = *pfVar3;
    if (fVar12 < 0.0) {
      *pfVar7 = -fVar12;
    }
    else {
      *pfVar7 = fVar12;
    }
    pfVar7 = pfVar7 + 1;
    pfVar3 = pfVar3 + 1;
  } while (pfVar7 < afStackX_0);
  iVar6 = 0;
  if (local_c[2] < local_c[0]) {
    iVar4 = 0;
    if (local_c[0] <= local_c[1]) {
      iVar4 = 1;
    }
  }
  else {
    iVar4 = (local_c[1] <= local_c[2]) + 1;
  }
  iVar1 = 0;
  if (param_2[iVar4] < 0.0) {
    uVar10 = iVar4 + 2;
    uVar11 = iVar4 + 1;
  }
  else {
    uVar10 = iVar4 + 1;
    uVar11 = iVar4 + 2;
  }
  iVar9 = (uVar10 % 3) * 4;
  fVar12 = *(float *)(param_3 + iVar9);
  iVar4 = param_1;
  pfVar3 = (float *)(param_1 + iVar9);
  do {
    iVar1 = iVar1 + 1;
    fVar14 = *pfVar3;
    iVar2 = (uVar11 % 3) * 4;
    iVar5 = param_1 + (iVar1 % 3) * 0xc;
    fVar13 = *(float *)(iVar5 + iVar9);
    if (fVar12 < fVar14) {
      iVar8 = -1;
    }
    else {
      iVar8 = 0;
    }
    iVar8 = (unsigned int)(fVar12 < fVar13) + iVar8;
    if (iVar8 != 0) {
      fVar17 = *(float *)(iVar5 + iVar2);
      fVar15 = *(float *)(param_3 + iVar2);
      fVar16 = *(float *)(iVar4 + iVar2);
      if (fVar15 < fVar16) {
        iVar2 = -1;
      }
      else {
        iVar2 = 0;
      }
      if ((unsigned int)(fVar15 < fVar17) + iVar2 == 0) {
        if (fVar16 <= fVar15) {
          iVar6 = iVar6 + iVar8;
        }
      }
      else if ((float)iVar8 * (fVar12 - fVar14) * (fVar17 - fVar16) <=
               (fVar13 - fVar14) * (float)iVar8 * (fVar15 - fVar16)) {
        iVar6 = iVar6 + iVar8;
      }
    }
    iVar4 = iVar4 + 0xc;
    pfVar3 = pfVar3 + 3;
  } while (iVar1 != 3);
  return iVar6;
}

/* WHAT IT DOES: Tell whether a segment crosses a unit box: rejects it when
 * both ends lie beyond the same face, then tests the crossings on each
 * axis. */
/* @implements 0x8025CBF8 tgr BrObbSegmentHits */
int BrObbSegmentHits(float *param_1,float *param_2)
{
  float fVar1;
  float *pfVar2;
  int iVar3;
  int *piVar4;
  int iVar5;
  float *pfVar6;
  float fVar7;
  int iVar8;
  float local_24 [3];
  int local_18 [6];
  
  pfVar2 = local_24;
  local_24[0] = *param_2 - *param_1;
  piVar4 = local_18;
  local_24[1] = param_2[1] - param_1[1];
  local_24[2] = param_2[2] - param_1[2];
  do {
    if (*pfVar2 < 0.0) {
      *piVar4 = -1;
    }
    else {
      *piVar4 = 1;
    }
    piVar4 = piVar4 + 1;
    pfVar2 = pfVar2 + 1;
  } while (piVar4 < local_18 + 3);
  iVar3 = 0;
  piVar4 = local_18;
  pfVar2 = param_1;
  while( 1 ) {
    iVar5 = *piVar4;
    piVar4 = piVar4 + 1;
    pfVar6 = (float *)((int)param_2 + iVar3);
    iVar3 = iVar3 + 4;
    if (0.5 < (float)iVar5 * *pfVar2) {
      return 0;
    }
    if ((float)iVar5 * *pfVar6 < -0.5) break;
    pfVar2 = pfVar2 + 1;
    if (local_18 + 3 <= piVar4) {
      iVar3 = 0;
      do {
        iVar5 = (iVar3 + 2) % 3;
        iVar3 = iVar3 + 1;
        iVar8 = iVar3 % 3;
        fVar7 = local_24[iVar5] * param_1[iVar8] - param_1[iVar5] * local_24[iVar8];
        fVar1 = ((float)local_18[iVar5] * local_24[iVar8] + local_24[iVar5] * (float)local_18[iVar8]
                ) * 0.5;
        if (fVar1 * fVar1 < fVar7 * fVar7) {
          return 0;
        }
      } while (iVar3 != 3);
      return 1;
    }
  }
  return 0;
}
