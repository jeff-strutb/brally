/* br_cartracklocate.c -- matching arm for BrCarTrackLocate (0x1006E5C0).
 *
 * Find which track cell/segment a car currently sits on: try the car's last
 * segment first, else scan every track cell, box-testing the car's grid coords
 * and picking the nearest segment whose facing agrees; on a hit, update the
 * car's cell/segment record and along-track progress and return 1, else 0.
 */
#ifdef BR_MATCHING_BUILD
#include "br_match.h"      /* BR_THISCALL1 -- thiscall via __fastcall on VC5 */

int   BrSeg2SideTest(int a, int b, int c, int d);
float BrVec3Dist(int a, int b);
void  BrVec3Sub(void *pOut, int a, int b);
float BrVec3Dot(const void *pA, const void *pB);
void  br_dl_normalise(void *pV);

extern int DAT_100b3014;
extern int DAT_106eed48;
extern int DAT_106eed50;
extern int DAT_106eed54;
extern float DAT_106eed00;
extern float DAT_106eed04;
extern int DAT_100b3858;
extern int DAT_100a9360;
extern float DAT_10077c30;
extern float DAT_10077c34;
extern float DAT_10077c38;
extern float DAT_10077c3c;

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
 * reopen before the end-grind (CLAUDE.md rule 12). */
/* @implements 0x1006E5C0 glide BrCarTrackLocate */
unsigned int BR_THISCALL1 BrCarTrackLocate(int param_1)
{
  int iVar1;
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
  int iVar12;
  int iVar13;
  float *pfVar14;
  int iVar15;
  float local_34;
  int local_30;
  int local_3c;
  float local_18[3];
  float local_c[3];
  unsigned char local_46;
  unsigned char local_45;

  local_3c = -1;
  fVar3 = *(float *)(param_1 + 0x30);
  fVar4 = *(float *)(param_1 + 0x34);
  fVar5 = *(float *)(param_1 + 0x38);
  iVar1 = param_1 + 0x30;
  local_46 = *(unsigned char *)(param_1 + 0x29bc);
  local_45 = *(unsigned char *)(param_1 + 0x29bd);
  local_30 = 0;
  bVar11 = 1;
  if (((((DAT_100b3014 == 3) || (DAT_100b3014 == 9)) && (0x38 <= local_46)) &&
       ((local_46 <= 0x3a && (0x17 <= local_45)))) && (local_45 <= 0x1b)) {
    local_46 = 0x39;
    local_45 = 0x19;
    bVar11 = 0;
  }
  fVar7 = *(float *)(param_1 + 0xff4) -
          (float)*(int *)(param_1 + 0xfac) * *(float *)(DAT_106eed48 + 100);
  if (bVar11) {
    iVar12 = *(int *)(param_1 + 0xf8c) + *(int *)(param_1 + 0xf90) * 0x28;
    iVar12 = BrSeg2SideTest(iVar12 + 0x40, iVar12 + 0x58, param_1 + 0xf80, iVar1);
    if (((iVar12 == 0) &&
         (iVar12 = *(int *)(param_1 + 0xf8c) + *(int *)(param_1 + 0xf90) * 0x28,
          iVar12 = BrSeg2SideTest(iVar12 + 0x68, iVar12 + 0x80, param_1 + 0xf80, iVar1), iVar12 == 0)) &&
        ((float)BrVec3Dist(iVar1, *(int *)(param_1 + 0xf8c) + 0x4c + *(int *)(param_1 + 0xf90) * 0x28) <
         DAT_10077c30)) {
      iVar12 = *(int *)(param_1 + 0xf8c);
      local_3c = *(int *)(param_1 + 0xf90);
      goto LAB_found;
    }
  }
  iVar13 = 0;
  local_34 = (DAT_106eed04 - DAT_106eed00) * (DAT_106eed04 - DAT_106eed00);
  iVar12 = DAT_106eed48;
  if (0 < DAT_106eed54) {
    do {
      if ((((((*(int *)(param_1 + 0x140) < DAT_100b3858) || (DAT_100a9360 == 2)) ||
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
            iVar12 = DAT_106eed48;
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
  fVar3 = (float)((float)(*(int *)(param_1 + 0xfac) + 1) * *(float *)(DAT_106eed48 + 100) -
                  *(float *)(iVar13 + 100) + fVar16 - *(float *)(param_1 + 0xff4));
  if ((bVar11 == 0) || ((fVar3 > DAT_10077c3c && (fVar3 < DAT_10077c34)))) {
    *(float *)(param_1 + 0xff4) = fVar3 + *(float *)(param_1 + 0xff4);
  }
  *(float *)(param_1 + 0xf94) = local_18[0];
  *(float *)(param_1 + 0xf9c) = local_18[2];
  *(int *)(param_1 + 0xf8c) = iVar12;
  *(int *)(param_1 + 0xf90) = local_3c;
  *(float *)(param_1 + 0xf98) = local_18[1];
  return 1;
}

#endif /* BR_MATCHING_BUILD */
