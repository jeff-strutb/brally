/* br_ctlinput.c -- driving: apply this frame's player input to one car.
 *
 *   0x1005AFF0  3210 B   BrCtlInputApply   (D3D 0x10061F70, shared by prefix)
 *
 * Neighbours: BrCarPhysStep 0x1005A7A0, BrCtlHumanBody 0x1005C8B0.
 * Matching arm only; the port TU is empty.
 *
 */

#define _CRTIMP __declspec(dllimport)

#include "br_race.h"   /* br_globals: its objects */
#include <math.h>
#include "slice3_41.h"

#include "br_match.h"


/* 64-bit core: declared once, by its definition's header */
/* BrBitLatchTake: prototype in br_funcs.h */

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
/* @t4-pass 0x1005AFF0 3 2026-09-19 probes 11 bytes 3444 insns 857 regions 14 rows 326 census no  (the A5 oracle found a REAL transcription bug the byte-grind had buried: the steering-recenter test was inverted -- `if (local[0] == local[4]) local[5] = 0` should be `!=` (the original zeros the target only when the sign of the current steering and the sign of the target DIFFER). Fixing it flipped rows 328->326. The residue is register-allocation/commutative-operand-order scheduling: +234 B of extra spills, no behavioural effect.) */
/* @t4-pass 0x1005AFF0 4 2026-09-19 probes 10 bytes 3444 insns 857 regions 14 rows 326 census yes  (write-slot census + variant sweep confirm the remaining residue is allocation/scheduling, not missing/wrong code; the A5 oracle proves same-in/same-out across the deadzone, the per-gear response curves incl. pow(), the steering slew and gear/throttle step; numbers unmoved.) */
/* WHAT IT DOES: apply this frame's player input to one car. Deadzones the
 * stick, picks handling coefficients for the controller mode, slews steering
 * toward the stick (or a speed-shaped curve when a digital button is held),
 * then steps gear, engine force and the throttle slew. */
/* @t3 0x1005AFF0 2026-09-19 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 3444/3210 insns 857/783 rows 126+200 regions 14 oracle EQUIVALENT
 * @t3-effort passes 4 zero-movement 3 4
 * A5 EQUIVALENT with teeth (negative-controlled: the inverted-compare bug, a
 * wrong rate constant, and other perturbations all DIFF).  Residue is
 * register-allocation + commutative-operand-order scheduling (+234 B of spills,
 * no behavioural effect); byte-exact is that colouring wall.  Certification
 * required an oracle fix too -- the x87 compare handler was not setting C3
 * (equal); see tools/x87emu.py.  Do not reopen before the end-grind.
 */
/* @implements 0x1005AFF0 glide BrCtlInputApply */
void BR_THISCALL1 BrCtlInputApply(BrDriverCar *pCar)
{
  float fVar1;
  float local[6];
  BrDriverCar *iVar4;
  int iVar9;
  BrDriver *piVar8;
  unsigned *puVar5;
  int bVar7;
  double dX, dAcc, dM, dL; /* x87 registers the original never spills */
  int l1Bits, limBits;     /* float images (32-bit int on every target) */

  fVar1 = *(float *)(*(unsigned char * *)&pCar->pCtl + 0x20);
  if ((((*(unsigned short * *)&g_BrPadModeBytes)[0] & 0x8000) == 0) && (((*(unsigned short * *)&g_BrPadModeBytes)[3] & 0x8000) == 0)) {
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
    local[2] = pCar->f1030;
    BrMat4MulVec3(&local[3], (float *)&pCar->aBody[0].rb.m.m[0], &pCar->aBody[0].rb.st.vel.x);
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
    switch (pCar->fE98) {
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
    /* 0x1005B266..0x1005B349: local[1] is stored and every later use
     * reloads it from its float slot; the steering target (dL) lives in an
     * x87 register from here to the store into [+0xE20] -- only the two
     * clamp limits are rounded (stored, then compared from memory).  The
     * int images force the stores VC5 would otherwise forward away; the
     * double is the register (whole-image run, quick race finish frame 973:
     * 1 ulp in [+0xE20]). */
    l1Bits = *(int *)&local[1];
    local[5] = local[0];
    if (local[0] < DAT_10077780) {
      local[5] = -local[0];
    }
    if (DAT_100777ec <= local[5]) {
      if (DAT_100777f0 <= local[0]) {
        if (local[0] <= DAT_100777f8) {
          dL = -((double)local[4] * DAT_100777f4 * local[0]);
        }
        else {
          dL = (double)*(float *)&l1Bits * DAT_100777fc;
        }
      }
      else {
        dL = (double)*(float *)&l1Bits * DAT_100777f4;
      }
    }
    else {
      dL = DAT_10077780;
    }
    local[3] = *(float *)&l1Bits * DAT_100777f4;
    limBits = *(int *)&local[3];
    if (!(dL <= *(float *)&limBits)) {
      dL = *(float *)&limBits;
    }
    local[3] = *(float *)&l1Bits * DAT_100777fc;
    limBits = *(int *)&local[3];
    if (dL < *(float *)&limBits) {
      dL = *(float *)&limBits;
    }
    local[0] = DAT_10077780;
    if ((dL != DAT_10077780) && (local[0] = DAT_10077788, DAT_10077780 < dL)) {
      local[0] = DAT_10077784;
    }
    local[4] = DAT_10077780;
    if ((pCar->f0E20 != DAT_10077780) &&
       (local[4] = DAT_10077788, DAT_10077780 < pCar->f0E20)) {
      local[4] = DAT_10077784;
    }
    /* the target and the current value point different ways: a change of
     * direction (0x1005B390: `je` on C3 clear -- NOT equal -- sets it) */
    if (local[0] != local[4]) {
      bVar7 = 1;
    }
    else if ((DAT_10077780 < pCar->f0E20) &&
             (dL < pCar->f0E20)) {
      bVar7 = 1;
    }
    else if ((pCar->f0E20 < DAT_10077780) &&
             (pCar->f0E20 < dL)) {
      bVar7 = 1;
    }
    else {
      bVar7 = 0;
    }
    if (pCar->f0E20 == DAT_10077780) {
      bVar7 = 0;
      pCar->f0E81 = 0;
    }
    if ((dL < pCar->f0E20) && (pCar->f0E81 < '\0')) {
      bVar7 = 1;
    }
    if ((pCar->f0E20 < dL) && ('\0' < pCar->f0E81)) {
      bVar7 = 1;
    }
    if (bVar7) {
      if (pCar->f0E20 <= dL) {
        pCar->f0E81 = 1;
      }
      else {
        pCar->f0E81 = (char)0xff;
      }
      local[2] = 1.0f;
      local[0] = DAT_10077780;
      if ((pCar->f0E20 != DAT_10077780) &&
         (local[0] = DAT_10077788, DAT_10077780 < pCar->f0E20)) {
        local[0] = DAT_10077784;
      }
      local[4] = DAT_10077780;
      if ((dL != DAT_10077780) && (local[4] = DAT_10077788, DAT_10077780 < dL)) {
        local[4] = DAT_10077784;
      }
      if (local[0] != local[4]) {
        dL = DAT_10077780;
      }
    }
    else {
      pCar->f0E81 = 0;
    }
    dX = pCar->f0E20 - dL;
    if (dX < DAT_10077780) {
      dX = -dX;
    }
    if (dX < local[2]) {
      pCar->f0E20 = (float)dL;
    }
    else if (dL < pCar->f0E20) {
      pCar->f0E20 = pCar->f0E20 - local[2];
    }
    else {
      pCar->f0E20 = local[2] + pCar->f0E20;
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
    if (DAT_10077800 < pCar->f1030) {
      if (pCar->f1030 < DAT_10077804) {
        local[0] = DAT_10077784 - (pCar->f1030 - DAT_10077800) * DAT_100777ec;
      }
      else {
        local[0] = 0.95f;
      }
    }
    else {
      local[0] = 1.0f;
    }
  }
  switch (pCar->fE98) {
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
    pCar->f0E20 =
         (float)((pow(DAT_10077810, (double)local[1]) - (double)DAT_10077784) *
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
    local[4] = (float)((pow(DAT_10077810, (double)local[1]) - (double)DAT_10077784) *
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
    local[4] = (float)((pow(DAT_10077828, (double)local[1]) - (double)DAT_10077784) *
                    (double)local[2]);
LAB_1005b6f3:
    pCar->f0E20 = local[4] * local[0] * DAT_10077820;
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
    pCar->f0E20 =
         (float)((pow(DAT_10077828, (double)local[1]) - (double)DAT_10077784) *
                 (double)local[2] * (double)local[0] * (double)DAT_10077830);
    break;
  case 4:
    pCar->f0E20 = local[0] * fVar1 * DAT_10077820;
    break;
  case 5:
    pCar->f0E20 = local[0] * fVar1 * DAT_10077834;
    break;
  case 6:
    pCar->f0E20 = local[0] * fVar1 * DAT_10077838;
    break;
  default:
    local[4] = DAT_10077780;
    if ((fVar1 != DAT_10077780) && (local[4] = DAT_10077788, DAT_10077780 < fVar1)) {
      local[4] = DAT_10077784;
    }
    pCar->f0E20 = fVar1 * fVar1 * local[4] * local[0] * DAT_1007783c;
  }
LAB_1005b7f2:
  local[1] = pCar->f0E24;
  if (pCar->f0E24 < DAT_10077840) {
    pCar->f0E24 = 800.0f;
  }
  if (pCar->f0E60 == 0) {
    if ((pCar->fE70 < 1) ||
        ((**(unsigned int * *)&pCar->pCtl & 0x200000) == 0)) {
      if ((pCar->f0E58 <= pCar->fE70) ||
         (((**(unsigned int * *)&pCar->pCtl & 0x100000) == 0 ||
          ((**(unsigned int * *)&pCar->pCtl & 0x20000) != 0)))) goto LAB_1005b92f;
      BrBitLatchTake(pCar->pCtl, 0, 0x100000);
      iVar9 = pCar->fE70 + 1;
    }
    else {
      BrBitLatchTake(pCar->pCtl, 0, 0x200000);
      iVar9 = pCar->fE70 + -1;
    }
    pCar->fE70 = iVar9;
  }
  else {
    local[2] = 6000.0f;
    local[4] = DAT_10077844;
    if ((((*(unsigned short * *)&g_BrPadModeBytes)[6] & 0x8000) != 0) &&
       (local[5] = *(float *)(*(unsigned char * *)&pCar->pCtl + 0x1c),
        DAT_1007784c < local[5])) {
      local[2] = DAT_10077844 - local[5] * DAT_10077850;
      local[4] = local[2] - DAT_10077854;
    }
    iVar9 = pCar->fE70;
    if ((iVar9 < 2) || (local[4] <= pCar->f0E24)) {
      if ((iVar9 < pCar->f0E58) &&
         ((local[2] < pCar->f0E24 &&
          ((**(unsigned int * *)&pCar->pCtl & 0x20000) == 0)))) {
        pCar->fE70 = iVar9 + 1;
      }
    }
    else {
      pCar->fE70 = iVar9 + -1;
    }
  }
LAB_1005b92f:
  local[4] = DAT_10077780;
  if ((*(int *)&g_brRaceRules.mode) == 1) {
    if (pCar->fFF8 == 0) goto LAB_1005b9e9;
    local[5] = DAT_10077780;
    if (0 < (*(int *)&g_brRaceNDriver)) {
      piVar8 = g_aBrRaceDriver;   /* the walker addresses whole driver records */
      iVar9 = (*(int *)&g_brRaceNDriver);
      do {
        iVar4 = piVar8->pCar;
        if (iVar4 == 0) {
          if (local[5] < piVar8->f50) {
            local[5] = piVar8->f50;
          }
        }
        else if (local[5] < iVar4->fFF4) {
          local[5] = iVar4->fFF4;
        }
        piVar8 = piVar8 + 1;
        iVar9 = iVar9 + -1;
      } while (iVar9 != 0);
    }
    /* from here to the store at [+0xE68] the original works in x87
     * registers (0x1005B99C..0x1005BA72): nothing is rounded to float until
     * that store */
    dX = (double)local[5] - pCar->fFF4;
    if (dX < DAT_10077780) {
      dX = -dX;
    }
    dX = dX * DAT_10077858 - DAT_1007785c;
    if (dX < DAT_10077780) goto LAB_1005b9e9;
    if (dX <= DAT_10077860) goto LAB_have_x;
  }
  else if ((((*(int *)&g_brRaceRules.mode) != 6) || (pCar->fFF8 == 0)) ||
          (local[4] = DAT_10077864, pCar->fFF8 == 1)) {
    goto LAB_1005b9e9;
  }
  local[4] = DAT_10077860;
LAB_1005b9e9:
  dX = local[4];
LAB_have_x:
  dAcc = (((double)pCar->f0E44 * pCar->f0E24 + pCar->f0E48)
          * pCar->f0E24 + pCar->f0E4C) *
         pCar->f0E24 + pCar->f0E50 + dX;
  if (pCar->f0E60 == 0) {
    dAcc = dAcc * DAT_10077868;
  }
  puVar5 = *(unsigned int * *)&pCar->pCtl;
  if ((*puVar5 & 0x10000) == 0) {
    dAcc = DAT_10077780;
  }
  dM = DAT_1007786c;
  if ((((*(unsigned short * *)&g_BrPadModeBytes)[6] & 0x8000) != 0) && (DAT_10077780 < ((float *)puVar5)[7])) {
    dM = (double)((float *)puVar5)[7] * DAT_1007786c;
  }
  local[4] = (float)dAcc;
  local[5] = (float)dM;
  pCar->fE68 = (float)(dM * dAcc);
  if ((*(unsigned char *)(*(unsigned char * *)&pCar->pProfile + 0x68) & 1) == 0) {
    if (pCar->fE70 == 0) {
      pCar->fE70 = 1;
    }
  }
  else {
    pCar->fE70 = 0;
  }
  iVar9 = pCar->fE70;
  if (iVar9 == 0) {
    local[4] = DAT_1007787c;
    if ((*puVar5 & 0x10000) != 0) {
      local[4] = DAT_10077878;
    }
    pCar->f0E24 = pCar->f0E24 - local[4];
    *(int *)&pCar->fE68 = 0;
  }
  else {
    /* the product stays in an x87 register (0x1005BAC1 / 0x1005BADE) and
     * is never rounded to a float slot; the double is that register */
    double prod;
    if ((*puVar5 & 0x20000) == 0) {
      local[4] = pCar->aBody[2].rb.f1C4;
      prod = (double)*(float *)(pCar + 0xe28 + iVar9 * 4) * pCar->f0E54;
    }
    else {
      local[4] = pCar->aBody[2].rb.f1C4;
      prod = (double)pCar->f0E54 * pCar->f0E2C;
    }
    local[5] = (float)prod;
    pCar->f0E24 = (float)((double)local[4] * DAT_10077870 * prod * DAT_10077874);
  }
  if (pCar->f0E24 < DAT_10077780) {
    pCar->f0E24 = -pCar->f0E24;
  }
  if (pCar->f0E24 < DAT_10077880) {
    pCar->f0E24 = 900.0f;
  }
  if (DAT_10077884 < pCar->f0E24) {
    pCar->f0E24 = 8000.0f;
  }
  if ((((*(unsigned short * *)&g_BrPadModeBytes)[6] & 0x8000) != 0) && (iVar9 == 0)) {
    local[4] = ((float *)puVar5)[7] * DAT_10077884;
    if (((float *)puVar5)[7] * DAT_10077884 < DAT_10077880) {
      local[4] = DAT_10077880;
    }
    if (local[4] < pCar->f0E24) {
      pCar->f0E24 = local[4];
    }
  }
  local[4] = pCar->f0E24 - local[1];
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
  *(int *)&pCar->f0E6C = 0;
  pCar->f0E24 = local[4] + local[1];
  if ((*puVar5 & 0x40000) != 0) {
    pCar->f0E6C = -140000.0f;
  }
}

