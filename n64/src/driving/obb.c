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
int func_8025C2C8(float *tri);
int BrPolyIntersectsCube(float verts[3][3], float polynormal[3]);
int BrObbFaceClip();
int BrObbSegmentHits();
#define DOT3(a, b) ((a)[0] * (b)[0] + (a)[1] * (b)[1] + (a)[2] * (b)[2])
#define SIGN_NONZERO(x) ((x) < 0 ? -1 : 1)
#define IN_CLOSED_INTERVAL(a, x, b) (((x) - (a)) * ((x) - (b)) <= 0)
#define SXV3(result, s, v) ((result)[0] = (s) * (v)[0], (result)[1] = (s) * (v)[1], (result)[2] = (s) * (v)[2])
/* -- end declarations -- */

/* WHAT IT DOES: Find the face of a box most facing a direction and clip it
 * against the other box: picks the axis with the largest component, then
 * walks that face's corners. */
/* @t4-pass 0x8025C8DC 1 2026-09-26 compiles 17 best 196 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x8025C8DC 2 2026-09-26 compiles 17 best 196 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x8025C8DC 3 2026-09-26 compiles 16 best 196 moved 0  (n64/tools/n64permute.py) */
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
    if (fVar12 < 0.0f) {
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
  if (param_2[iVar4] < 0.0f) {
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
/* @t4-pass 0x8025CBF8 1 2026-09-26 compiles 17 best 127 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x8025CBF8 2 2026-09-26 compiles 16 best 127 moved 0  (n64/tools/n64permute.py) */
/* @t4-pass 0x8025CBF8 3 2026-09-26 compiles 17 best 127 moved 0  (n64/tools/n64permute.py) */
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

/* WHAT IT DOES: Graphics Gems III (Voorhies) polygon_intersects_cube for a
 * triangle against the unit cube at the origin: any edge through the cube
 * (0x8025CBF8) is a hit; otherwise pick the cube diagonal nearest the
 * normal, find where it meets the triangle's plane, and if that is inside
 * the cube ask 0x8025C8DC whether the point is inside the triangle. */
/* @implements 0x8025CE28 tgr BrPolyIntersectsCube */
int BrPolyIntersectsCube(float verts[3][3], float polynormal[3])
{
  int i, best_diagonal[3];
  float p[3], t;

  for (i = 0; i < 3; ++i)
    if (BrObbSegmentHits(verts[i], verts[(i + 1) % 3]))
      return 1;

  for (i = 0; i < 3; i++) best_diagonal[i] = SIGN_NONZERO(polynormal[i]);

  t = DOT3(polynormal, verts[0]) / DOT3(polynormal, best_diagonal);
  if (!IN_CLOSED_INTERVAL(-.5, t, .5))
    return 0;
  SXV3(p, t, best_diagonal);
  return BrObbFaceClip(verts, polynormal, p);
}

/* WHAT IT DOES: Triangle-against-unit-cube test: the vertex outcode pass
 * decides most cases; when it cannot (-1), fall back to the edge and plane
 * test with the triangle's normal. */
/* @implements 0x8025D018 tgr BrTriCubeTest */
int BrTriCubeTest(float *tri, float *norm)
{
  int r;

  r = func_8025C2C8(tri);
  if (r == -1) {
    return BrPolyIntersectsCube((float (*)[3])tri, norm);
  }
  return r;
}
