/* br_ctlinput.c -- driving: apply this frame's player input to one car.
 *
 *   0x1005AFF0  3210 B   BrCtlInputApply   (D3D 0x10061F70, shared by prefix)
 *
 * Neighbours: BrCarPhysStep 0x1005A7A0, BrCtlHumanBody 0x1005C8B0.
 * Matching arm only; the port TU is empty.
 *
 */

#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#endif

#include <math.h>

#include "br_match.h"

#ifdef BR_MATCHING_BUILD

void BrMat4MulVec3(float *pOut, float *pM, float *pV);
void __fastcall BrBitLatchTake(void *pThis, void *_edx, unsigned mask);

extern unsigned short *DAT_10b71534;
extern int DAT_100b2e6c;
extern float DAT_100b2e70;
extern float DAT_100b2e74;
extern float DAT_100b2e78;
extern short DAT_10ac67cc;
extern int DAT_100a9360;
extern int DAT_100b2f00;
extern int DAT_10af0858[];

extern float DAT_10077780;
extern float DAT_10077784;
extern float DAT_10077788;
extern float DAT_10077798;
extern float DAT_1007779c;
extern float DAT_100777a0;
extern float DAT_100777a4;
extern float DAT_100777a8;
extern float DAT_100777ac;
extern float DAT_100777b8;
extern float DAT_100777c0;
extern float DAT_100777c8;
extern float DAT_100777d0;
extern float DAT_100777e4;
extern float DAT_100777e8;
extern float DAT_100777ec;
extern float DAT_100777f0;
extern float DAT_100777f4;
extern float DAT_100777f8;
extern float DAT_100777fc;
extern float DAT_10077800;
extern float DAT_10077804;
extern double DAT_10077810;
extern float DAT_10077818;
extern float DAT_1007781c;
extern float DAT_10077820;
extern double DAT_10077828;
extern float DAT_10077830;
extern float DAT_10077834;
extern float DAT_10077838;
extern float DAT_1007783c;
extern float DAT_10077840;
extern float DAT_10077844;
extern float DAT_1007784c;
extern float DAT_10077850;
extern float DAT_10077854;
extern float DAT_10077858;
extern float DAT_1007785c;
extern float DAT_10077860;
extern float DAT_10077864;
extern float DAT_10077868;
extern float DAT_1007786c;
extern float DAT_10077870;
extern float DAT_10077874;
extern float DAT_10077878;
extern float DAT_1007787c;
extern float DAT_10077880;
extern float DAT_10077884;
extern float DAT_10077888;

/* Residue vs orig 3210 B / 783 insns: recomp 3444 B / 857 insns (+234 / +74).
 * Frame is sub esp,0x18 and the analogue deadzone is the orig polarity
 * (fcom 0; test ah,0x41; jne neg; fsub 0.07; fst; fcomp 0; test ah,1; je keep).
 * Remaining structural gap is the mode-switch pin of DAT_100777a0 on x87
 * (orig flds 10.0 before clamping local_c[0] and fstp's it into DAT_100b2e78;
 * we mov the globals) and the 0x1030 speed clamp orig parks on the rate slot
 * then overwrites -- DCE'd here because the slot is dead after the switch.
 * Temps live in local[4]/local[5] of the address-taken 6-float frame.
 * Colouring and operand-source rows sit on top of that. Not Gate A.
 */
/* @t4-pass 0x1005AFF0 1 2026-09-09 probes 12 bytes 3444 insns 857 regions 14 rows 328 census yes */
/* @t4-pass 0x1005AFF0 2 2026-09-09 probes 12 bytes 3444 insns 857 regions 14 rows 328 census yes */
/* WHAT IT DOES: apply this frame's player input to one car. Deadzones the
 * stick, picks handling coefficients for the controller mode, slews steering
 * toward the stick (or a speed-shaped curve when a digital button is held),
 * then steps gear, engine force and the throttle slew. */
/* @implements 0x1005AFF0 glide BrCtlInputApply */
void BR_THISCALL1 BrCtlInputApply(unsigned char *pCar)
{
  float fVar1;
  float local[6];
  int iVar4;
  int iVar9;
  int *piVar8;
  unsigned *puVar5;
  int bVar7;

  fVar1 = *(float *)(*(unsigned char **)(pCar + 0x29c0) + 0x20);
  if (((DAT_10b71534[0] & 0x8000) == 0) && ((DAT_10b71534[3] & 0x8000) == 0)) {
    if (fVar1 > DAT_10077780) {
      local[0] = fVar1 - DAT_10077798;
      if (local[0] >= DAT_10077780) goto LAB_keep_stick;
      goto LAB_zero_stick;
    }
    local[0] = fVar1 - DAT_1007779c;
    if (local[0] <= DAT_10077780) goto LAB_keep_stick;
LAB_zero_stick:
    local[0] = 0.0f;
LAB_keep_stick:
    local[2] = *(float *)(pCar + 0x1030);
    BrMat4MulVec3(&local[3], (float *)(pCar + 0x220), (float *)(pCar + 0x1e8));
    local[1] = local[3];
    if (local[3] < DAT_100777a0) {
      local[1] = DAT_100777a0;
    }
    if (DAT_100777a4 < local[1]) {
      local[1] = 70.0f;
    }
    if (local[2] < DAT_100777a8) {
      local[2] = DAT_100777a8;
    }
    if (DAT_100777ac < local[2]) {
      local[2] = DAT_100777ac;
    }
    local[2] = local[2] - DAT_100777a8;
    local[4] = DAT_100777d0;
    switch (*(int *)(pCar + 0xe98)) {
    case 0:
      DAT_100b2e78 = DAT_100777a0;
      DAT_100b2e70 = 14.0f;
      DAT_100b2e74 = 0.008f;
      local[4] = DAT_100777b8;
      break;
    case 1:
      DAT_100b2e78 = DAT_100777a0;
      DAT_100b2e70 = 14.0f;
      DAT_100b2e74 = 0.01f;
      local[4] = DAT_100777c0;
      break;
    case 2:
      DAT_100b2e78 = DAT_100777a0;
      DAT_100b2e70 = 14.0f;
      DAT_100b2e74 = 0.013f;
      local[4] = DAT_100777c8;
      break;
    case 3:
      DAT_100b2e78 = DAT_100777a0;
      DAT_100b2e70 = 14.0f;
      DAT_100b2e74 = 0.017f;
      break;
    case 4:
      DAT_100b2e78 = DAT_100777a0;
      DAT_100b2e70 = 14.0f;
      DAT_100b2e74 = 0.022f;
      break;
    case 5:
      DAT_100b2e78 = DAT_100777a0;
      DAT_100b2e70 = 14.0f;
      DAT_100b2e74 = 0.03f;
      break;
    case 6:
      DAT_100b2e78 = DAT_100777a0;
      DAT_100b2e70 = 14.0f;
      DAT_100b2e74 = 0.05f;
      break;
    default:
      DAT_100b2e70 = 17.0f;
      DAT_100b2e74 = 0.07f;
      DAT_100b2e78 = 14.0f;
      local[4] = DAT_100777e4;
    }
    local[2] = DAT_100b2e74;
    DAT_10ac67cc = 0;
    local[1] = DAT_100b2e70 - local[1] * DAT_100777e8 * DAT_100b2e78;
    local[5] = local[0];
    if (local[0] < DAT_10077780) {
      local[5] = -local[0];
    }
    if (DAT_100777ec <= local[5]) {
      if (DAT_100777f0 <= local[0]) {
        if (local[0] <= DAT_100777f8) {
          local[5] = -(local[4] * DAT_100777f4 * local[0]);
        }
        else {
          local[5] = local[1] * DAT_100777fc;
        }
      }
      else {
        local[5] = local[1] * DAT_100777f4;
      }
    }
    else {
      local[5] = DAT_10077780;
    }
    if (local[1] * DAT_100777f4 < local[5]) {
      local[5] = local[1] * DAT_100777f4;
    }
    if (local[5] < local[1] * DAT_100777fc) {
      local[5] = local[1] * DAT_100777fc;
    }
    local[0] = DAT_10077780;
    if ((local[5] != DAT_10077780) && (local[0] = DAT_10077788, DAT_10077780 < local[5])) {
      local[0] = DAT_10077784;
    }
    local[4] = DAT_10077780;
    if ((*(float *)(pCar + 0xe20) != DAT_10077780) &&
       (local[4] = DAT_10077788, DAT_10077780 < *(float *)(pCar + 0xe20))) {
      local[4] = DAT_10077784;
    }
    if (local[0] == local[4]) {
      bVar7 = 1;
    }
    else if ((DAT_10077780 < *(float *)(pCar + 0xe20)) &&
             (local[5] < *(float *)(pCar + 0xe20))) {
      bVar7 = 1;
    }
    else if ((*(float *)(pCar + 0xe20) < DAT_10077780) &&
             (*(float *)(pCar + 0xe20) < local[5])) {
      bVar7 = 1;
    }
    else {
      bVar7 = 0;
    }
    if (*(float *)(pCar + 0xe20) == DAT_10077780) {
      bVar7 = 0;
      *(char *)(pCar + 0xe81) = 0;
    }
    if ((local[5] < *(float *)(pCar + 0xe20)) && (*(char *)(pCar + 0xe81) < '\0')) {
      bVar7 = 1;
    }
    if ((*(float *)(pCar + 0xe20) < local[5]) && ('\0' < *(char *)(pCar + 0xe81))) {
      bVar7 = 1;
    }
    if (bVar7) {
      if (*(float *)(pCar + 0xe20) <= local[5]) {
        *(char *)(pCar + 0xe81) = 1;
      }
      else {
        *(char *)(pCar + 0xe81) = (char)0xff;
      }
      local[2] = 1.0f;
      local[0] = DAT_10077780;
      if ((*(float *)(pCar + 0xe20) != DAT_10077780) &&
         (local[0] = DAT_10077788, DAT_10077780 < *(float *)(pCar + 0xe20))) {
        local[0] = DAT_10077784;
      }
      local[4] = DAT_10077780;
      if ((local[5] != DAT_10077780) && (local[4] = DAT_10077788, DAT_10077780 < local[5])) {
        local[4] = DAT_10077784;
      }
      if (local[0] == local[4]) {
        local[5] = DAT_10077780;
      }
    }
    else {
      *(char *)(pCar + 0xe81) = 0;
    }
    local[4] = *(float *)(pCar + 0xe20) - local[5];
    if (local[4] < DAT_10077780) {
      local[4] = -local[4];
    }
    if (local[4] < local[2]) {
      *(float *)(pCar + 0xe20) = local[5];
    }
    else if (local[5] < *(float *)(pCar + 0xe20)) {
      *(float *)(pCar + 0xe20) = *(float *)(pCar + 0xe20) - local[2];
    }
    else {
      *(float *)(pCar + 0xe20) = local[2] + *(float *)(pCar + 0xe20);
    }
    goto LAB_1005b7f2;
  }
  if (fVar1 < DAT_10077780) {
    local[1] = -fVar1;
  }
  else {
    local[1] = fVar1;
  }
  local[0] = 1.0f;
  if (DAT_100b2e6c != 0) {
    if (DAT_10077800 < *(float *)(pCar + 0x1030)) {
      if (*(float *)(pCar + 0x1030) < DAT_10077804) {
        local[0] = DAT_10077784 - (*(float *)(pCar + 0x1030) - DAT_10077800) * DAT_100777ec;
      }
      else {
        local[0] = 0.95f;
      }
    }
    else {
      local[0] = 1.0f;
    }
  }
  switch (*(int *)(pCar + 0xe98)) {
  case 0:
    if (fVar1 == DAT_10077780) {
      local[2] = 0.0f;
    }
    else {
      local[2] = 1.0f;
      if (fVar1 <= DAT_10077780) {
        local[2] = -1.0f;
      }
    }
    *(float *)(pCar + 0xe20) =
         (float)((pow((double)local[1], DAT_10077810) - (double)DAT_10077784) *
                 (double)local[2] * (double)DAT_10077818 *
                 (double)local[0] * (double)DAT_1007781c);
    break;
  case 1:
    if (fVar1 == DAT_10077780) {
      local[2] = 0.0f;
    }
    else {
      local[2] = 1.0f;
      if (fVar1 <= DAT_10077780) {
        local[2] = -1.0f;
      }
    }
    local[4] = (float)((pow((double)local[1], DAT_10077810) - (double)DAT_10077784) *
                    (double)local[2] * (double)DAT_10077818);
    goto LAB_1005b6f3;
  case 2:
    if (fVar1 == DAT_10077780) {
      local[2] = 0.0f;
    }
    else {
      local[2] = 1.0f;
      if (fVar1 <= DAT_10077780) {
        local[2] = -1.0f;
      }
    }
    local[4] = (float)((pow((double)local[1], DAT_10077828) - (double)DAT_10077784) *
                    (double)local[2]);
LAB_1005b6f3:
    *(float *)(pCar + 0xe20) = local[4] * local[0] * DAT_10077820;
    break;
  case 3:
    if (fVar1 == DAT_10077780) {
      local[2] = 0.0f;
    }
    else {
      local[2] = 1.0f;
      if (fVar1 <= DAT_10077780) {
        local[2] = -1.0f;
      }
    }
    *(float *)(pCar + 0xe20) =
         (float)((pow((double)local[1], DAT_10077828) - (double)DAT_10077784) *
                 (double)local[2] * (double)local[0] * (double)DAT_10077830);
    break;
  case 4:
    *(float *)(pCar + 0xe20) = local[0] * fVar1 * DAT_10077820;
    break;
  case 5:
    *(float *)(pCar + 0xe20) = local[0] * fVar1 * DAT_10077834;
    break;
  case 6:
    *(float *)(pCar + 0xe20) = local[0] * fVar1 * DAT_10077838;
    break;
  default:
    local[4] = DAT_10077780;
    if ((fVar1 != DAT_10077780) && (local[4] = DAT_10077788, DAT_10077780 < fVar1)) {
      local[4] = DAT_10077784;
    }
    *(float *)(pCar + 0xe20) = fVar1 * fVar1 * local[4] * local[0] * DAT_1007783c;
  }
LAB_1005b7f2:
  local[1] = *(float *)(pCar + 0xe24);
  if (*(float *)(pCar + 0xe24) < DAT_10077840) {
    *(float *)(pCar + 0xe24) = 800.0f;
  }
  if (*(int *)(pCar + 0xe60) == 0) {
    if ((*(int *)(pCar + 0xe70) < 1) ||
        ((**(unsigned **)(pCar + 0x29c0) & 0x200000) == 0)) {
      if ((*(int *)(pCar + 0xe58) <= *(int *)(pCar + 0xe70)) ||
         (((**(unsigned **)(pCar + 0x29c0) & 0x100000) == 0 ||
          ((**(unsigned **)(pCar + 0x29c0) & 0x20000) != 0)))) goto LAB_1005b92f;
      BrBitLatchTake(*(void **)(pCar + 0x29c0), 0, 0x100000);
      iVar9 = *(int *)(pCar + 0xe70) + 1;
    }
    else {
      BrBitLatchTake(*(void **)(pCar + 0x29c0), 0, 0x200000);
      iVar9 = *(int *)(pCar + 0xe70) + -1;
    }
    *(int *)(pCar + 0xe70) = iVar9;
  }
  else {
    local[2] = 6000.0f;
    local[4] = DAT_10077844;
    if (((DAT_10b71534[6] & 0x8000) != 0) &&
       (local[5] = *(float *)(*(unsigned char **)(pCar + 0x29c0) + 0x1c),
        DAT_1007784c < local[5])) {
      local[2] = DAT_10077844 - local[5] * DAT_10077850;
      local[4] = local[2] - DAT_10077854;
    }
    iVar9 = *(int *)(pCar + 0xe70);
    if ((iVar9 < 2) || (local[4] <= *(float *)(pCar + 0xe24))) {
      if ((iVar9 < *(int *)(pCar + 0xe58)) &&
         ((local[2] < *(float *)(pCar + 0xe24) &&
          ((**(unsigned **)(pCar + 0x29c0) & 0x20000) == 0)))) {
        *(int *)(pCar + 0xe70) = iVar9 + 1;
      }
    }
    else {
      *(int *)(pCar + 0xe70) = iVar9 + -1;
    }
  }
LAB_1005b92f:
  local[4] = DAT_10077780;
  if (DAT_100a9360 == 1) {
    if (*(int *)(pCar + 0xff8) == 0) goto LAB_1005b9e9;
    local[5] = DAT_10077780;
    if (0 < DAT_100b2f00) {
      piVar8 = DAT_10af0858;
      iVar9 = DAT_100b2f00;
      do {
        iVar4 = *piVar8;
        if (iVar4 == 0) {
          if (local[5] < *(float *)(piVar8 - 4)) {
            local[5] = *(float *)(piVar8 - 4);
          }
        }
        else if (local[5] < *(float *)(iVar4 + 0xff4)) {
          local[5] = *(float *)(iVar4 + 0xff4);
        }
        piVar8 = piVar8 + 0x20;
        iVar9 = iVar9 + -1;
      } while (iVar9 != 0);
    }
    local[5] = local[5] - *(float *)(pCar + 0xff4);
    if (local[5] < DAT_10077780) {
      local[5] = -local[5];
    }
    local[5] = local[5] * DAT_10077858 - DAT_1007785c;
    if (local[5] < DAT_10077780) goto LAB_1005b9e9;
    local[4] = local[5];
    if (local[5] <= DAT_10077860) goto LAB_1005b9e9;
  }
  else if (((DAT_100a9360 != 6) || (*(int *)(pCar + 0xff8) == 0)) ||
          (local[4] = DAT_10077864, *(int *)(pCar + 0xff8) == 1)) {
    goto LAB_1005b9e9;
  }
  local[4] = DAT_10077860;
LAB_1005b9e9:
  local[4] = ((*(float *)(pCar + 0xe44) * *(float *)(pCar + 0xe24) + *(float *)(pCar + 0xe48))
           * *(float *)(pCar + 0xe24) + *(float *)(pCar + 0xe4c)) *
          *(float *)(pCar + 0xe24) + *(float *)(pCar + 0xe50) + local[4];
  if (*(int *)(pCar + 0xe60) == 0) {
    local[4] = local[4] * DAT_10077868;
  }
  puVar5 = *(unsigned **)(pCar + 0x29c0);
  if ((*puVar5 & 0x10000) == 0) {
    local[4] = DAT_10077780;
  }
  local[5] = DAT_1007786c;
  if (((DAT_10b71534[6] & 0x8000) != 0) && (DAT_10077780 < ((float *)puVar5)[7])) {
    local[5] = ((float *)puVar5)[7] * DAT_1007786c;
  }
  *(float *)(pCar + 0xe68) = local[5] * local[4];
  if ((*(unsigned char *)(*(unsigned char **)(pCar + 0xf00) + 0x68) & 1) == 0) {
    if (*(int *)(pCar + 0xe70) == 0) {
      *(int *)(pCar + 0xe70) = 1;
    }
  }
  else {
    *(int *)(pCar + 0xe70) = 0;
  }
  iVar9 = *(int *)(pCar + 0xe70);
  if (iVar9 == 0) {
    local[4] = DAT_1007787c;
    if ((*puVar5 & 0x10000) != 0) {
      local[4] = DAT_10077878;
    }
    *(float *)(pCar + 0xe24) = *(float *)(pCar + 0xe24) - local[4];
    *(int *)(pCar + 0xe68) = 0;
  }
  else {
    if ((*puVar5 & 0x20000) == 0) {
      local[4] = *(float *)(pCar + 0x740);
      local[5] = *(float *)(pCar + 0xe28 + iVar9 * 4) * *(float *)(pCar + 0xe54);
    }
    else {
      local[4] = *(float *)(pCar + 0x740);
      local[5] = *(float *)(pCar + 0xe54) * *(float *)(pCar + 0xe2c);
    }
    *(float *)(pCar + 0xe24) = local[4] * DAT_10077870 * local[5] * DAT_10077874;
  }
  if (*(float *)(pCar + 0xe24) < DAT_10077780) {
    *(float *)(pCar + 0xe24) = -*(float *)(pCar + 0xe24);
  }
  if (*(float *)(pCar + 0xe24) < DAT_10077880) {
    *(float *)(pCar + 0xe24) = 900.0f;
  }
  if (DAT_10077884 < *(float *)(pCar + 0xe24)) {
    *(float *)(pCar + 0xe24) = 8000.0f;
  }
  if (((DAT_10b71534[6] & 0x8000) != 0) && (iVar9 == 0)) {
    local[4] = ((float *)puVar5)[7] * DAT_10077884;
    if (((float *)puVar5)[7] * DAT_10077884 < DAT_10077880) {
      local[4] = DAT_10077880;
    }
    if (local[4] < *(float *)(pCar + 0xe24)) {
      *(float *)(pCar + 0xe24) = local[4];
    }
  }
  local[4] = *(float *)(pCar + 0xe24) - local[1];
  local[5] = local[4];
  if (local[4] < DAT_10077780) {
    local[5] = -local[4];
  }
  if (DAT_10077888 < local[5]) {
    local[5] = DAT_10077780;
    if ((local[4] != DAT_10077780) && (local[5] = DAT_10077788, DAT_10077780 < local[4])) {
      local[5] = DAT_10077784;
    }
    local[4] = local[5] * DAT_10077888;
  }
  *(int *)(pCar + 0xe6c) = 0;
  *(float *)(pCar + 0xe24) = local[4] + local[1];
  if ((*puVar5 & 0x40000) != 0) {
    *(float *)(pCar + 0xe6c) = -140000.0f;
  }
}

#endif /* BR_MATCHING_BUILD */
