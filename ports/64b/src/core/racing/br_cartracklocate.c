/* br_cartracklocate.c -- matching arm for BrCarTrackLocate (0x1006E5C0).
 *
 * Find which track cell/segment a car currently sits on: try the car's last
 * segment first, else scan every track cell, box-testing the car's grid coords
 * and picking the nearest segment whose facing agrees; on a hit, update the
 * car's cell/segment record and along-track progress and return 1, else 0.
 */
#include "br_race.h"   /* br_globals: its objects */
#include "br_match.h"      /* BR_THISCALL1 -- thiscall via __fastcall on VC5 */
#include "slice3_41.h"

/* 64-bit core: declared once, by its definition's header */
/* 64-bit core: declared once, by its definition's header */
/* 64-bit core: declared once, by its definition's header */
/* 64-bit core: declared once, by its definition's header */
/* br_dl_normalise: prototype in br_funcs.h */

/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* @t4-pass 0x1006E5C0 1 2026-09-21 probes 11 bytes 1038 insns 297 regions 9 rows 30 census yes  (hand: vec3 layout, z-order, int-vs-float z store, fn.py variants; frame 0x4c vs 0x48 + x87 fmul/faddp scheduling do not move) */
/* @t4-pass 0x1006E5C0 2 2026-09-21 probes 12 bytes 1038 insns 297 regions 9 rows 30 census yes  (slot census clean; register-allocation colouring wall -- this/edi vs ebp, zero/ebp vs ebx, frame slot packing) */
/* WHAT IT DOES: locate the track cell/segment under a car -- test the last
 * segment first, else scan all cells, box-testing the car's grid coords and
 * choosing the nearest forward-facing segment; on a hit write the car's
 * cell/segment record, lateral offsets and along-track progress and return 1,
 * otherwise return 0. */
/* @t3 0x1006E5C0 2026-09-21 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 1038/1044 insns 297/299 rows 16+14 regions 9 oracle EQUIVALENT
 * @t3-effort passes 2 zero-movement 1 2
 * Residue is a register-allocation colouring wall: `this` in edi vs ebp, the
 * zero reg in ebp vs ebx, and one extra 4-byte frame slot (0x4c vs 0x48), plus
 * x87 fmul/faddp operand-role scheduling in the distance dot-product.  A5
 * oracle EQUIVALENT (24 inputs: return + globals + car side effects).  Do not
 * reopen before the end-grind. */
/* @implements 0x1006E5C0 glide BrCarTrackLocate */
unsigned int BR_THISCALL1 BrCarTrackLocate(BrDriverCar *param_1)
{
  BrVec3 *iVar1;
  unsigned short uVar2;
  float fVar3;
  float fVar4;
  float fVar5;
  int iVar6;
  float fVar7;
  float fVar8;
  float fVar9;
  float fVar10;
  float fVar16;
  int bVar11;
  BrAiPathNode *iVar12;
  BrAiPathNode *iVar13;
  float *pfVar14;
  int iVar15;
  float local_34;
  BrAiPathNode *local_30;
  int local_3c;
  float local_18[3];
  float local_c[3];
  unsigned char local_46;
  unsigned char local_45;

  local_3c = -1;
  fVar3 = param_1->pos.x;
  fVar4 = param_1->pos.y;
  fVar5 = param_1->pos.z;
  iVar1 = ((void *)&param_1->pos.x);
  local_46 = param_1->f29BC;
  local_45 = param_1->f29BD;
  local_30 = 0;
  bVar11 = 1;
  if ((((((*(int *)&g_Br0B380C) == 3) || ((*(int *)&g_Br0B380C) == 9)) && (0x38 <= local_46)) &&
       ((local_46 <= 0x3a && (0x17 <= local_45)))) && (local_45 <= 0x1b)) {
    local_46 = 0x39;
    local_45 = 0x19;
    bVar11 = 0;
  }
  fVar7 = param_1->fFF4 -
          (float)param_1->lapB * *(float *)(g_pBrRaceLapRec + 100);
  if (bVar11) {
    iVar12 = *(int *)&param_1->pNode + *(int *)&param_1->iPt * 0x28;
    iVar12 = BrSeg2SideTest(iVar12 + 0x40, iVar12 + 0x58, ((void *)&param_1->posPrev.x), iVar1);
    if (((iVar12 == 0) &&
         (iVar12 = *(int *)&param_1->pNode + *(int *)&param_1->iPt * 0x28,
          iVar12 = BrSeg2SideTest(iVar12 + 0x68, iVar12 + 0x80, ((void *)&param_1->posPrev.x), iVar1), iVar12 == 0)) &&
        ((float)BrVec3Dist(iVar1, *(int *)&param_1->pNode + 0x4c + *(int *)&param_1->iPt * 0x28) <
         DAT_10077c30)) {
      iVar12 = *(int *)&param_1->pNode;
      local_3c = *(int *)&param_1->iPt;
      goto LAB_found;
    }
  }
  iVar13 = 0;
  local_34 = (DAT_106eed04 - DAT_106eed00) * (DAT_106eed04 - DAT_106eed00);
  iVar12 = g_pBrRaceLapRec;
  if (0 < DAT_106eed54) {
    do {
      if ((((((param_1->f140 < (*(int *)&g_brRaceNEntrant)) || ((*(int *)&g_brRaceRules.mode) == 2)) ||
             ((*(unsigned char *)(*(int *)(DAT_106eed50 + iVar13 * 4) + 0x16) & 1) == 0)) &&
            ((iVar6 = *(int *)(DAT_106eed50 + iVar13 * 4), local_46 >= *(unsigned char *)(iVar6 + 0x10) &&
              (local_46 <= *(unsigned char *)(iVar6 + 0x12))))) &&
           ((local_45 >= *(unsigned char *)(iVar6 + 0x11) && (local_45 <= *(unsigned char *)(iVar6 + 0x13))))) &&
          ((bVar11 == 0 ||
            (((*(float *)(iVar12 + 100) - *(float *)(iVar6 + 100)) - fVar7 <= DAT_10077c34 &&
              (fVar7 - (*(float *)(iVar12 + 100) -
                        *(float *)(iVar6 + 100 + (unsigned int)*(unsigned short *)(iVar6 + 0x14) * 0x28)) <=
               DAT_10077c34)))))) {
        iVar15 = 0;
        uVar2 = *(unsigned short *)(iVar6 + 0x14);
        if (uVar2 != 0) {
          pfVar14 = (float *)(iVar6 + 0x54);
          do {
            fVar9 = pfVar14[-2] - fVar3;
            fVar8 = pfVar14[-1] - fVar4;
            fVar10 = *pfVar14 - fVar5;
            fVar8 = fVar10 * fVar10 + fVar9 * fVar9 + fVar8 * fVar8;
            if (fVar8 < local_34) {
              local_18[2] = 0.0f;
              local_18[0] = pfVar14[-4] - pfVar14[2];
              local_18[1] = pfVar14[1] - pfVar14[-5];
              BrVec3Sub(local_c, iVar1, (int)(pfVar14 + -2));
              if (DAT_10077c38 <= (float)BrVec3Dot(local_18, local_c)) {
                local_3c = iVar15;
                local_34 = fVar8;
                local_30 = iVar13;
              }
            }
            iVar15 = iVar15 + 1;
            pfVar14 = pfVar14 + 10;
            iVar12 = g_pBrRaceLapRec;
          } while (iVar15 < (int)(unsigned int)uVar2);
        }
      }
      iVar13 = iVar13 + 1;
    } while (iVar13 < DAT_106eed54);
  }
  if (local_3c == -1) {
    return 0;
  }
  iVar12 = *(int *)(DAT_106eed50 + local_30 * 4);
LAB_found:
  local_18[0] = *(float *)(iVar12 + 0x44 + local_3c * 0x28) -
                *(float *)(iVar12 + 0x5c + local_3c * 0x28);
  iVar13 = iVar12 + local_3c * 0x28;
  local_18[1] = *(float *)(iVar13 + 0x58) - *(float *)(iVar13 + 0x40);
  local_18[2] = 0.0f;
  br_dl_normalise(local_18);
  BrVec3Sub(local_c, iVar1, iVar13 + 0x4c);
  fVar16 = BrVec3Dot(local_18, local_c);
  fVar3 = (float)((float)(param_1->lapB + 1) * *(float *)(g_pBrRaceLapRec + 100) -
                  *(float *)(iVar13 + 100) + fVar16 - param_1->fFF4);
  if ((bVar11 == 0) || ((fVar3 > DAT_10077c3c && (fVar3 < DAT_10077c34)))) {
    param_1->fFF4 = fVar3 + param_1->fFF4;
  }
  param_1->f0F94 = local_18[0];
  param_1->f0F9C = local_18[2];
  *(int *)&param_1->pNode = iVar12;
  *(int *)&param_1->iPt = local_3c;
  param_1->f0F98 = local_18[1];
  return 1;
}

