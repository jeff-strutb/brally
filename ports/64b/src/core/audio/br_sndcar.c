/* br_sndcar.c -- audio.
 *
 * Per-car engine loops and one-shot dispatch, once a frame, thiscall on the
 * car.  Neighbour 0x10061310 (race bring-up) lives in br_sndrace.c.
 */
/* The original is /MD: CRT calls go through the import
 * table (FF 15). */
#define _CRTIMP __declspec(dllimport)


#include "br_trkhdr.h"   /* g_brTrkHdr, the loaded track header */
#include "br_sfxsrc.h"   /* br_globals: its objects */
#include "slice2_24.h"   /* br_globals: its objects */
#include <stdint.h>
#include "slice3_41.h"

#include "br_match.h"

#define BR_K_00778D8   0.0f
#define BR_K_00778CC   0.5f
#define BR_K_0077904   (-0.5f)
#define BR_K_00779F8   15.714285850524902f
#define BR_K_00779FC   100000.0f
#define BR_K_00779E4   9.09090886125341e-05f
#define BR_K_00779E8   4294967296.0
#define BR_K_0077A00   22050.0f
#define BR_K_0077A04   2.370370388031006f
#define BR_K_00779F0   11000.0f
#define BR_K_0077A08   20000.0f

/* 64-bit core: declared once, by its definition's header */
/* 64-bit core: declared once, by its definition's header */
/* BrSndPlaySimple: prototype in br_funcs.h */
/* BrFfbSetDurationShort: prototype in br_funcs.h */
/* BrFfbCommitDuration: prototype in br_funcs.h */
/* BrSfxSrcPlaySilent: prototype in br_funcs.h */
/* 64-bit core: declared once, by its definition's header */
/* BrPodNop: prototype in br_funcs.h */
/* BrSndSetVolumePairF: prototype in br_funcs.h */
/* FUN_1006ba80: prototype in br_funcs.h */
/* FUN_1006baf0: prototype in br_funcs.h */
/* FUN_1006bb10: prototype in br_funcs.h */

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

/* WHAT IT DOES: one frame of one car's sound: retunes the engine loops from
 * RPM and Doppler, packs the stereo level pair, and fires the one-shots.
 *
 * Early-out when the race is paused or the car is gone, zeroing that car's
 * engine ratio and packed pair.  The listener is the viewed car in cockpit
 * mode, otherwise the object at car+0x2734; a remote-listener flag skips
 * writing the engine and surface voices.  Engine hertz fold the RPM about
 * zero, scale by 110/7 and Doppler, and clamp out of [0, 100000]; the 32.32
 * ratio multiplies by 1/11000 then 2^32.  Packed levels are
 * (gainA*vol)<<16 | (gainB*vol) through _ftol.  Car 0 also latches 11000 Hz
 * into the channel-0 override when a track flag is set.
 *
 * Seven one-shots, each gated on a per-car byte that is then cleared.
 * Hits at +0x362 / +0x363 (and bottom-out at +0x36C) use BrSndPan's two
 * gains and a three-tier ladder at 0xAB / 0xD5.  The four taunts at
 * +0x366..+0x369 ignore the gains and threshold the integer volume:
 * cmp eax,2 / jle keep / mov eax,0x20, then * 0x10001, so a taunt is
 * centre and either near-silent or loud.  +0x36A / +0x36B drive the high
 * engine layer (group 24): Doppler * 22050 into the paired-slot setter
 * when Doppler is not below zero, then a start or stop on that slot.
 *
 * The rest is the surface-loop state: group 4..12 from the car's surface
 * byte, a 12-frame fade of the impact byte at +0x36D, and -- on car 0 --
 * a silent-start of the new group plus the packed surface pair.  The
 * listener's last position is copied back onto the car for next frame. */
/* Residue: extra 4-byte frame (sub esp,0x30 vs 0x2c) plus jump-table dwords
 * after the last ret (outside the orig span). Packed-pair x87 is
 * fld-mem/fmul-st at three sites and fld-st/fmul-mem at two. DAT_117a5f08
 * stores as [M] vs [A]. f68=7 from a register vs immediate. Colouring of
 * the live zero and car*2 (lea vs shl). Do not reopen for permutation.
 * @t4-pass 0x10061470 1 2026-09-09 probes 11 bytes 2796 insns 765 regions 23 rows 101 census yes  (decl order, corpus at pack)
 * @t4-pass 0x10061470 2 2026-09-09 probes 11 bytes 2796 insns 765 regions 23 rows 101 census yes  (locals + comment probes) */
/* @t3 0x10061470 2026-09-19 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 2796/2757 insns 765/742 rows 35+58 regions 23 oracle EQUIVALENT
 * @t3-effort passes 2 zero-movement 1 2
 * A5 EQUIVALENT on 48 seeds (return + globals + side effects); byte residue is
 * the +4 frame and x87 packed-pair scheduling colouring in the header above.
 * Behaviourally verified with a seeded valid world: the cockpit-camera table
 * (DAT_10af393c -> a scratch transform) so BrSndDoppler reads a real listener
 * position, and g_BrAnimDt (glide copy 0x106e9d8c) nonzero so its per-frame
 * velocity divide is finite -- both unseeded gave a NaN Doppler that poisoned
 * fVar10 and zeroed every engine-hertz write (a masked false EQUIVALENT).  The
 * fixed-point outputs (eef48/54/60/6c, 1184c454) are exact regions so a scale
 * bug is not waved through as rounding.  BrSndPlayEx's mixer stays gated OFF:
 * seeding its voice gates on drives its real path, whose stdcall voice
 * callbacks fire through NULL-voice slots that the emulator black-boxes with
 * esp UNCHANGED, leaking arg bytes into a 0x20 esp drift that moved only the
 * ORIGINAL's esp-relative Doppler-copyback target out of the compared buffer
 * (the recompile's ebp-relative locals were immune) -- a pure emulator/seeding
 * artifact, not a code diff.  Negative-controlled: a copyback-offset mutation
 * (0xf5c->0xf58) and an engine-scale mutation (110/7 -> the 100000 clamp) both
 * DIFF; correct code stays EQUIVALENT.  Do not reopen before the end-grind.
 */
/* @implements 0x10061470 glide BrSndCarStep */

void BR_THISCALL1 BrSndCarStep(BrDriverCar *pCar)

{
  unsigned char bVar1;
  short sVar2;
  int iVar3;
  int iVar4;
  int iVar5;
  int iVar6;
  unsigned int uVar7;
  int iVar8;
  int iVar9;
  float fVar10;
  float local_28;
  int local_24;
  int local_20;
  int local_1c;
  float local_18;
  float local_14;
  int *local_10;
  float local_c;
  float local_8;
  int local_4;

  iVar6 = pCar->f140;
  iVar9 = iVar6 << 1;
  local_4 = (*(int *)&BrG_6C1628[4]) * 0x2b68;
  if (((*(int *)&g_BrX06909B4) != 0) || (pCar->pProfile == 0)) {
    (&(*(int *)((char *)&g_aBrSfxChan + 0x14)))[iVar6 * 0xc] = 0;
    (&(*(int *)((char *)&g_aBrSfxChan + 0x8)))[iVar6 * 0xc] = 0;
    (&(*(int *)((char *)&g_aBrSfxChan + 0xC)))[iVar6 * 0xc] = 0;
    return;
  }
  local_24 = 0;
  if (g_brMode0AA8B4 == 1) {
    iVar8 = *(int *)((char *)&(*(int *)&g_aBrRaceCar[0].pMatA) + local_4);
    if (pCar->fF78 != 0) goto LAB_10061526;
    iVar3 = *(int *)((char *)&(*(int *)&g_aBrRaceCar[0].fF78) + local_4);
LAB_j10061524:
    if (iVar3 != 0) goto LAB_10061526;
  }
  else {
    iVar8 = BR_LP64_PTR_AS_INT(pCar->pMatA);
    if ((pCar->fF78 == 0) && (*(int *)((char *)&(*(int *)&g_aBrRaceCar[0].fF78) + local_4) == 0)) {
      iVar3 = g_aBrRaceCar[(*(int *)&g_brRaceBegin6E8720)].fF78;
      goto LAB_j10061524;
    }
LAB_10061526:
    local_24 = 1;
  }
  local_10 = &pCar->f0F5C;
  iVar3 = (int)(((void *)&pCar->pos.x));
  pCar->f0F74 =
      BrSndDoppler((void *)iVar3, &pCar->posPrev.x, (char *)iVar8 + 0x30, local_10);
  BrSndPan((void *)iVar3, (void *)iVar8, &local_18, &local_14, &local_1c, 0);
  if (pCar->f0E24 > BR_K_00778D8) {
    fVar10 = pCar->f0E24 * BR_K_00778CC;
  }
  else {
    fVar10 = pCar->f0E24 * BR_K_0077904;
  }
  fVar10 = fVar10 * BR_K_00779F8;
  fVar10 = fVar10 * pCar->f0F74;
  if (fVar10 > BR_K_00779FC) {
    fVar10 = BR_K_00778D8;
  }
  else if (fVar10 < BR_K_00778D8) {
    fVar10 = BR_K_00778D8;
  }
  if (pCar->f140 == 0) {
    g_184C454 = (*(unsigned char *)(BR_PTR32(void *, g_brTrkHdr.aInstances) + 0x4c +
                     (unsigned int)pCar->aNearIds[0] * 0x54) & 0x10)
                    ? 11000 : 0;
  }
  if (local_24 == 0) {
    local_20 = iVar9 * 24;
    *(__int64 *)((char *)&(*(int *)((char *)&g_aBrSfxChan + 0x8)) + local_20) =
        (__int64)(fVar10 * BR_K_00779E4 * BR_K_00779E8);
    local_8 = (float)local_1c;
    iVar4 = (int)(local_8 * local_14);
    iVar5 = (int)(local_8 * local_18);
    *(int *)((char *)&(*(int *)((char *)&g_aBrSfxChan + 0x14)) + local_20) = iVar4 + iVar5 * 0x10000;
  }
  local_20 = pCar->f0F68;
  if ((((*(int *)&g_aBrSfxChan[1 + iVar9].ratio) | (&(*(int *)((char *)&g_aBrSfxChan + 0x24)))[iVar9 * 6]) == 0) ||
     ((pCar->aBody[0].f0209 == '\0' && (local_20 >= 4) && (local_20 <= 7)))) {
    pCar->f0F68 = 0;
    pCar->f0F6C = 0;
    pCar->f0F70 = 0;
  }
  if (0x7f < *(unsigned char *)&pCar->aBody[0].f01FE) {
    BrSndPan((void *)iVar3, (void *)iVar8, &local_8, &local_c, (int *)&local_28, 0);
    fVar10 = (float)*(int *)&local_28;
    iVar6 = (int)(fVar10 * local_c);
    iVar4 = (int)(fVar10 * local_8);
    *(int *)&local_28 = iVar6 + iVar4 * 0x10000;
    if (*(unsigned char *)&pCar->aBody[0].f01FE < 0xab) {
      BrSub10072AF0(0x11, *(int *)&local_28);
    }
    else if (*(unsigned char *)&pCar->aBody[0].f01FE < 0xd5) {
      BrSub10072AF0(0x10, *(int *)&local_28);
    }
    else {
      BrSub10072AF0(1, *(int *)&local_28);
    }
    BrFfbSetDurationShort();
    BrFfbCommitDuration();
  }
  pCar->aBody[0].f01FE = 0;
  if (0x7f < pCar->aBody[0].f01FF) {
    BrSndPan((void *)iVar3, (void *)iVar8, &local_c, &local_8, (int *)&local_28, 0);
    fVar10 = (float)*(int *)&local_28;
    iVar6 = (int)(fVar10 * local_8);
    iVar4 = (int)(fVar10 * local_c);
    *(int *)&local_28 = iVar6 + iVar4 * 0x10000;
    if (pCar->aBody[0].f01FF < 0xab) {
      BrSub10072AF0(0x13, *(int *)&local_28);
    }
    else if (pCar->aBody[0].f01FF < 0xd5) {
      BrSub10072AF0(0x12, *(int *)&local_28);
    }
    else {
      BrSub10072AF0(2, *(int *)&local_28);
    }
    BrFfbCommitDuration();
    BrFfbSetDurationShort();
  }
  *(char *)&pCar->aBody[0].f01FF = 0;
  if (0x7f < *(unsigned char *)&pCar->aBody[0].f0208) {
    BrSndPan((void *)iVar3, (void *)iVar8, &local_c, &local_8, (int *)&local_28, 0);
    fVar10 = (float)*(int *)&local_28;
    iVar6 = (int)(fVar10 * local_8);
    iVar4 = (int)(fVar10 * local_c);
    *(int *)&local_28 = iVar6 + iVar4 * 0x10000;
    BrSub10072AF0(3, *(int *)&local_28);
  }
  pCar->aBody[0].f0208 = 0;
  if (0x7f < *(unsigned char *)&pCar->aBody[0].f0202) {
    BrSndPan((void *)iVar3, (void *)iVar8, &local_c, &local_8, (int *)&local_28, 0);
    iVar4 = *(int *)&local_28;
    if (2 < iVar4) {
      iVar4 = 0x20;
    }
    iVar4 = iVar4 * 0x10001;
    *(int *)&local_28 = iVar4;
    BrSub10072AF0(0x14, iVar4);
  }
  pCar->aBody[0].f0202 = 0;
  if (0x7f < *(unsigned char *)&pCar->aBody[0].f0203) {
    BrSndPan((void *)iVar3, (void *)iVar8, &local_c, &local_8, (int *)&local_28, 0);
    iVar4 = *(int *)&local_28;
    if (2 < iVar4) {
      iVar4 = 0x20;
    }
    iVar4 = iVar4 * 0x10001;
    *(int *)&local_28 = iVar4;
    BrSub10072AF0(0x15, iVar4);
  }
  pCar->aBody[0].f0203 = 0;
  if (0x7f < *(unsigned char *)&pCar->aBody[0].f0204) {
    BrSndPan((void *)iVar3, (void *)iVar8, &local_c, &local_8, (int *)&local_28, 0);
    iVar4 = *(int *)&local_28;
    if (2 < iVar4) {
      iVar4 = 0x20;
    }
    iVar4 = iVar4 * 0x10001;
    *(int *)&local_28 = iVar4;
    BrSub10072AF0(0x16, iVar4);
  }
  pCar->aBody[0].f0204 = 0;
  if (0x7f < *(unsigned char *)&pCar->aBody[0].f0205) {
    BrSndPan((void *)iVar3, (void *)iVar8, &local_c, &local_8, (int *)&local_28, 0);
    iVar4 = *(int *)&local_28;
    if (2 < iVar4) {
      iVar4 = 0x20;
    }
    iVar4 = iVar4 * 0x10001;
    *(int *)&local_28 = iVar4;
    BrSub10072AF0(0x17, iVar4);
  }
  pCar->aBody[0].f0205 = 0;
  if (0x7f < *(unsigned char *)&pCar->aBody[0].f0206) {
    BrSndPan((void *)iVar3, (void *)iVar8, &local_c, &local_8, (int *)&local_28, 0);
    *(int *)&local_28 = *(int *)&local_28 * 0x10001;
    if (pCar->f0F74 >= BR_K_00778D8) {
      BrSndSetVolumePairF(0x18, pCar->f140,
                          pCar->f0F74 * BR_K_0077A00);
      BrPodNop();
    }
    if (0x7f < *(unsigned char *)&pCar->aBody[0].f0207) {
      BrWrap_10072B80(0x18, pCar->f140, *(int *)&local_28);
    }
    else {
      BrWrap_10072B10(0x18, pCar->f140, *(int *)&local_28);
    }
  }
  else if (0x7f < *(unsigned char *)&pCar->aBody[0].f0207) {
    FUN_1006bb10(0x18, pCar->f140);
  }
  iVar6 = pCar->f0F68;
  pCar->aBody[0].f0207 = pCar->aBody[0].f0206;
  pCar->aBody[0].f0206 = 0;
  if (((iVar6 != 0) && ((iVar6 < 4 || (7 < iVar6)))) && ((iVar6 < 8 || (0xc < iVar6))))
    goto LAB_10061dc3;
  iVar6 = -1;
  if (*(int *)&pCar->aBody[2].rb.f1B4 != 0) {
    iVar6 = (int)*(char *)&pCar->aBody[2].rb.f01A0;
  }
  bVar1 = *(unsigned char *)&pCar->aBody[0].f0209;
  uVar7 = (unsigned int)bVar1;
  if ((pCar->f0F6C < (int)uVar7) ||
     ((bVar1 != 0 &&
      ((pCar->f0F70 < 0x28 ||
       (((bVar1 != 0 && (pCar->f0F70 == 0x28)) && ((local_20 >= 4 && (local_20 <= 7)))))
       ))))) {
    *(unsigned int *)&pCar->f0F6C = uVar7;
    if (DAT_104b15e8 == 3) {
      if (iVar6 >= 0) {
        if (iVar6 < 4) {
          pCar->f0F68 = 7;
          goto LAB_10061cb3;
        }
        if (iVar6 == 4) {
          pCar->f0F68 = 7;
          pCar->f0F6C = 0;
        }
      }
    }
    else {
      switch (iVar6) {
      case 0:
      case 3:
        iVar8 = pCar->f140;
        if ((iVar8 < (*(int *)&g_brRaceNEntrant)) && (DAT_117a5f08[iVar8] == 0)) {
          DAT_117a5f18[iVar8] = 0;
          DAT_117a5f0c[pCar->f140] = 0;
          DAT_117a5f10[pCar->f140] = 1;
          DAT_117a5f14[pCar->f140] = 9;
          DAT_117a5f1c[pCar->f140] = 10;
          DAT_117a5f08[pCar->f140] = 1;
        }
        pCar->f0F68 = 7;
        break;
      case 1:
        iVar8 = pCar->f140;
        if ((iVar8 < (*(int *)&g_brRaceNEntrant)) && (DAT_117a5f08[iVar8] == 0)) {
          DAT_117a5f18[iVar8] = 0;
          DAT_117a5f0c[pCar->f140] = 0;
          DAT_117a5f10[pCar->f140] = 1;
          DAT_117a5f14[pCar->f140] = 0xe;
          DAT_117a5f1c[pCar->f140] = 0xf;
          DAT_117a5f08[pCar->f140] = 1;
        }
        pCar->f0F68 = 5;
        break;
      case 2:
        iVar8 = pCar->f140;
        if ((iVar8 < (*(int *)&g_brRaceNEntrant)) && (DAT_117a5f08[iVar8] == 0)) {
          DAT_117a5f18[iVar8] = 0;
          DAT_117a5f0c[pCar->f140] = 0;
          DAT_117a5f10[pCar->f140] = 1;
          DAT_117a5f14[pCar->f140] = 0xc;
          DAT_117a5f1c[pCar->f140] = 0xd;
          DAT_117a5f08[pCar->f140] = 1;
        }
        pCar->f0F68 = 4;
        break;
      case 4:
        pCar->f0F68 = 0xc;
      }
      uVar7 = (unsigned int)*(unsigned char *)&pCar->aBody[0].f0209;
LAB_10061cb3:
      *(unsigned int *)&pCar->f0F6C = uVar7 >> 1;
    }
    sVar2 = pCar->f0162;
    pCar->f0F70 = 0x28;
    pCar->f0162 = sVar2 + 1;
    if (0x10 < sVar2) {
      pCar->f0162 = 0x10;
    }
    pCar->f0F6C = ((int)pCar->f0162 * pCar->f0F6C) / 0xc;
  }
  else {
    pCar->f0162 = 0;
  }
  iVar8 = pCar->f0F68;
  if ((iVar8 != 0) && ((iVar8 < 8 || (0xc < iVar8)))) goto LAB_10061dc3;
  if (DAT_104b15e8 == 3) {
    if (iVar6 < 0) {
switchD_10061d51_default:
      pCar->f0F68 = 0;
    }
    else if (iVar6 < 4) {
      pCar->f0F68 = 0xb;
    }
    else {
      if (iVar6 != 4) goto switchD_10061d51_default;
      pCar->f0F68 = 10;
    }
  }
  else {
    switch (iVar6) {
    case 0:
    case 3:
      pCar->f0F68 = 10;
      break;
    case 1:
      pCar->f0F68 = 8;
      break;
    case 2:
      pCar->f0F68 = 9;
      break;
    case 4:
      pCar->f0F68 = 0xc;
      break;
    default:
      goto switchD_10061d51_default;
    }
  }
  pCar->f0F70 = 1;
  iVar6 = (int)(BrVec3Length(&pCar->f1024.x) * BR_K_0077A04);
  pCar->f0F6C = iVar6;
  if (0x40 < iVar6) {
    pCar->f0F6C = 0x40;
  }
LAB_10061dc3:
  iVar6 = pCar->f0F68;
  if (iVar6 == 0) {
    if (iVar9 == 0) {
      (*(int *)((char *)&g_aBrSfxChan + 0x2C)) = 0;
      (*(int *)((char *)&g_aBrSfxChan + 0x20)) = 0;
      (*(int *)((char *)&g_aBrSfxChan + 0x24)) = 0;
    }
  }
  else {
    local_28 = pCar->f0F74 * BR_K_00779F0;
    if ((BR_K_0077A08 < local_28) || (local_28 < BR_K_00778D8)) {
      local_28 = 0.0f;
    }
    if ((iVar6 != local_20) && (iVar9 == 0)) {
      BrSfxSrcPlaySilent(1, (*(int *)&g_brStages[27 + iVar6].f10[0]), g_brStages[28 + iVar6].f04,
                         g_brStages[28 + iVar6].f08);
    }
    if ((local_24 == 0) && (iVar9 == 0)) {
      iVar4 = pCar->f0F6C * local_1c >> 7;
      *(__int64 *)&(*(int *)((char *)&g_aBrSfxChan + 0x20)) = (__int64)(local_28 * BR_K_00779E4 * BR_K_00779E8);
      fVar10 = (float)iVar4;
      iVar6 = (int)(fVar10 * local_18);
      iVar9 = (int)(fVar10 * local_14);
      (*(int *)((char *)&g_aBrSfxChan + 0x2C)) = iVar6 * 0x10000 + iVar9;
    }
  }
  if (g_brMode0AA8B4 == 1) {
    iVar6 = *(int *)((char *)&(*(int *)&g_aBrRaceCar[0].pMatA) + local_4) + 0x30;
    *local_10 = *(int *)iVar6;
    local_10[1] = *(int *)(iVar6 + 4);
    local_10[2] = *(int *)(iVar6 + 8);
    return;
  }
  iVar6 = BR_LP64_PTR_AS_INT(pCar->pMatA) + 0x30;
  *local_10 = *(int *)iVar6;
  local_10[1] = *(int *)(iVar6 + 4);
  local_10[2] = *(int *)(iVar6 + 8);
  return;
}

