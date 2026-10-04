/* br_sndcar.c -- audio.
 *
 * Per-car engine loops and one-shot dispatch, once a frame, thiscall on the
 * car.  Neighbour 0x10061310 (race bring-up) lives in br_sndrace.c.
 */
/* The original is /MD: CRT calls go through the import
 * table (FF 15). */
#define _CRTIMP __declspec(dllimport)


#include <stdint.h>

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

float BrSndDoppler(void *, void *, void *, void *);
void BrSndPan(void *, void *, float *, float *, int *, int);
int BrSndPlaySimple(int, int);  /* 0x1006BA60 */
int BrFfbSetDurationShort(void);
int BrFfbCommitDuration(void);
int BrSfxSrcPlaySilent(int, int, int, int);
float BrVec3Length(void *);
int BrPodNop();
int BrSndSetVolumePairF(int, int, float);
int FUN_1006ba80(int, int, int);
int FUN_1006baf0(int, int, int);
int FUN_1006bb10(int, int);

extern int DAT_100aa044;
extern int DAT_100b32b0[];
extern int DAT_100b32bc[];
extern int DAT_100b32c0[];
extern int DAT_100b3858;
extern int DAT_104b15e8;
extern int DAT_105ccb5c;
extern int DAT_106e86c8;
extern int DAT_106e8720;
extern int DAT_106eed38;
extern int DAT_10af2180;
extern int DAT_10af393c;
extern short DAT_117a5f08[];
extern short DAT_117a5f0c[];
extern short DAT_117a5f10[];
extern short DAT_117a5f14[];
extern short DAT_117a5f18[];
extern short DAT_117a5f1c[];
extern int DAT_1184c454;
extern int DAT_118eef48;
extern int DAT_118eef4c;
extern int DAT_118eef54;
extern int DAT_118eef60;
extern int DAT_118eef64;
extern int DAT_118eef6c;
extern char DAT_100b3844[];

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
/* Source shapes that decide the bytes:
 *  - each one-shot's two gains and its packed level are block locals; VC5
 *    gives every block's pair the same two slots at the top of the frame and
 *    the level a slot shared with the late Doppler float;
 *  - the remote-listener flag is set by one `local_24 = 1` after a goto
 *    join, so it has fewer references than the shared level slot;
 *  - the packed-pair temp is a double: with a float temp VC5 converts the
 *    shifted half first;
 *  - the impact-byte test widens the byte once (uVar7) and re-reads the
 *    byte for the char tests, and the three disjuncts are written flat;
 *  - the two ==3 mode branches are switches (`cmp ebx,3 / jle`);
 *  - the engine ratio multiplies by 1/11000 in its own statement;
 *  - the taunt level is stored as (v * 0x10001) and re-read for the call.
 *
 * @t4-pass 0x10061470 1 2026-09-09 probes 11 bytes 2796 insns 765 regions 23 rows 101 census yes  (decl order, corpus at pack)
 * @t4-pass 0x10061470 2 2026-09-09 probes 11 bytes 2796 insns 765 regions 23 rows 101 census yes  (locals + comment probes) */
/* @implements 0x10061470 glide BrSndCarStep */

void BR_THISCALL1 BrSndCarStep(uint8_t *pCar)

{
  short sVar2;
  int iVar3;
  int iVar4;
  int iVar5;
  int iVar6;
  unsigned int uVar7;
  int iVar8;
  int iVar9;
  float fVar10;
  double dVar11;
  float local_28;
  int local_24;
  int local_20;
  int local_1c;
  float local_18;
  float local_14;
  int *local_10;
  int local_4;

  iVar9 = *(int *)(pCar + 0x140) * 2;
  local_4 = DAT_106e86c8 * 0x2b68;
  if ((DAT_105ccb5c != 0) || (*(int *)(pCar + 0xf00) == 0)) {
    (&DAT_118eef54)[iVar9 * 6] = 0;
    (&DAT_118eef48)[iVar9 * 6] = 0;
    (&DAT_118eef4c)[iVar9 * 6] = 0;
    return;
  }
  local_24 = 0;
  if (DAT_100aa044 == 1) {
    iVar8 = *(int *)((char *)&DAT_10af393c + local_4);
    if (*(int *)(pCar + 0xf78) == 0 && *(int *)((char *)&DAT_10af2180 + local_4) == 0)
      goto LAB_quiet;
  }
  else {
    iVar8 = *(int *)(pCar + 0x2734);
    if (*(int *)(pCar + 0xf78) == 0 && *(int *)((char *)&DAT_10af2180 + local_4) == 0 &&
        *(int *)((char *)&DAT_10af2180 + DAT_106e8720 * 0x2b68) == 0)
      goto LAB_quiet;
  }
  local_24 = 1;
LAB_quiet:
  local_10 = (int *)(pCar + 0xf5c);
  iVar3 = (int)(pCar + 0x30);
  *(float *)(pCar + 0xf74) =
      BrSndDoppler((void *)iVar3, pCar + 0xf80, (char *)iVar8 + 0x30, local_10);
  BrSndPan((void *)iVar3, (void *)iVar8, &local_18, &local_14, &local_1c, 0);
  if (*(float *)(pCar + 0xe24) > BR_K_00778D8) {
    fVar10 = *(float *)(pCar + 0xe24) * BR_K_00778CC;
  }
  else {
    fVar10 = *(float *)(pCar + 0xe24) * BR_K_0077904;
  }
  fVar10 = fVar10 * BR_K_00779F8;
  fVar10 = fVar10 * *(float *)(pCar + 0xf74);
  if (fVar10 > BR_K_00779FC) {
    fVar10 = BR_K_00778D8;
  }
  else if (fVar10 < BR_K_00778D8) {
    fVar10 = BR_K_00778D8;
  }
  if (*(int *)(pCar + 0x140) == 0) {
    DAT_1184c454 = (*(unsigned char *)(DAT_106eed38 + 0x4c +
                     (unsigned int)*(unsigned short *)(pCar + 0x290c) * 0x54) & 0x10)
                    ? 11000 : 0;
  }
  if (local_24 == 0) {
    fVar10 = fVar10 * BR_K_00779E4;
    *(__int64 *)(&DAT_118eef48 + iVar9 * 6) = (__int64)(fVar10 * BR_K_00779E8);
    dVar11 = (double)local_1c;
    iVar4 = (int)(dVar11 * local_14);
    iVar5 = (int)(dVar11 * local_18);
    (&DAT_118eef54)[iVar9 * 6] = iVar4 + iVar5 * 0x10000;
  }
  local_20 = *(int *)(pCar + 0xf68);
  if ((((__int64 *)&DAT_118eef60)[iVar9 * 3] == 0) ||
     ((*(char *)(pCar + 0x36d) == '\0' && (local_20 >= 4) && (local_20 <= 7)))) {
    *(int *)(pCar + 0xf68) = 0;
    *(int *)(pCar + 0xf6c) = 0;
    *(int *)(pCar + 0xf70) = 0;
  }
  if (0x7f < *(unsigned char *)(pCar + 0x362)) {
    float pl, pr; int pv;
    BrSndPan((void *)iVar3, (void *)iVar8, &pl, &pr, &pv, 0);
    dVar11 = (double)pv;
    iVar6 = (int)(dVar11 * pr);
    iVar4 = (int)(dVar11 * pl);
    pv = iVar6 + iVar4 * 0x10000;
    if (*(unsigned char *)(pCar + 0x362) < 0xab) {
      BrSndPlaySimple(0x11, pv);
    }
    else if (*(unsigned char *)(pCar + 0x362) < 0xd5) {
      BrSndPlaySimple(0x10, pv);
    }
    else {
      BrSndPlaySimple(1, pv);
    }
    BrFfbSetDurationShort();
    BrFfbCommitDuration();
  }
  *(char *)(pCar + 0x362) = 0;
  if (0x7f < *(unsigned char *)(pCar + 0x363)) {
    float pl, pr; int pv;
    BrSndPan((void *)iVar3, (void *)iVar8, &pr, &pl, &pv, 0);
    dVar11 = (double)pv;
    iVar6 = (int)(dVar11 * pl);
    iVar4 = (int)(dVar11 * pr);
    pv = iVar6 + iVar4 * 0x10000;
    if (*(unsigned char *)(pCar + 0x363) < 0xab) {
      BrSndPlaySimple(0x13, pv);
    }
    else if (*(unsigned char *)(pCar + 0x363) < 0xd5) {
      BrSndPlaySimple(0x12, pv);
    }
    else {
      BrSndPlaySimple(2, pv);
    }
    BrFfbCommitDuration();
    BrFfbSetDurationShort();
  }
  *(char *)(pCar + 0x363) = 0;
  if (0x7f < *(unsigned char *)(pCar + 0x36c)) {
    float pl, pr; int pv;
    BrSndPan((void *)iVar3, (void *)iVar8, &pr, &pl, &pv, 0);
    dVar11 = (double)pv;
    iVar6 = (int)(dVar11 * pl);
    iVar4 = (int)(dVar11 * pr);
    pv = iVar6 + iVar4 * 0x10000;
    BrSndPlaySimple(3, pv);
  }
  *(char *)(pCar + 0x36c) = 0;
  if (0x7f < *(unsigned char *)(pCar + 0x366)) {
    float pl, pr; int pv;
    BrSndPan((void *)iVar3, (void *)iVar8, &pr, &pl, &pv, 0);
    iVar4 = pv;
    if (2 < iVar4) {
      iVar4 = 0x20;
    }
    pv = iVar4 * 0x10001;
    BrSndPlaySimple(0x14, pv);
  }
  *(char *)(pCar + 0x366) = 0;
  if (0x7f < *(unsigned char *)(pCar + 0x367)) {
    float pl, pr; int pv;
    BrSndPan((void *)iVar3, (void *)iVar8, &pr, &pl, &pv, 0);
    iVar4 = pv;
    if (2 < iVar4) {
      iVar4 = 0x20;
    }
    pv = iVar4 * 0x10001;
    BrSndPlaySimple(0x15, pv);
  }
  *(char *)(pCar + 0x367) = 0;
  if (0x7f < *(unsigned char *)(pCar + 0x368)) {
    float pl, pr; int pv;
    BrSndPan((void *)iVar3, (void *)iVar8, &pr, &pl, &pv, 0);
    iVar4 = pv;
    if (2 < iVar4) {
      iVar4 = 0x20;
    }
    pv = iVar4 * 0x10001;
    BrSndPlaySimple(0x16, pv);
  }
  *(char *)(pCar + 0x368) = 0;
  if (0x7f < *(unsigned char *)(pCar + 0x369)) {
    float pl, pr; int pv;
    BrSndPan((void *)iVar3, (void *)iVar8, &pr, &pl, &pv, 0);
    iVar4 = pv;
    if (2 < iVar4) {
      iVar4 = 0x20;
    }
    pv = iVar4 * 0x10001;
    BrSndPlaySimple(0x17, pv);
  }
  *(char *)(pCar + 0x369) = 0;
  if (0x7f < *(unsigned char *)(pCar + 0x36a)) {
    float pl, pr; int pv;
    BrSndPan((void *)iVar3, (void *)iVar8, &pr, &pl, &pv, 0);
    pv = pv * 0x10001;
    if (*(float *)(pCar + 0xf74) >= BR_K_00778D8) {
      BrSndSetVolumePairF(0x18, *(int *)(pCar + 0x140),
                          *(float *)(pCar + 0xf74) * BR_K_0077A00);
      BrPodNop(DAT_100b3844, *(float *)(pCar + 0xf74));
    }
    if (*(unsigned char *)(pCar + 0x36b) <= 0x7f) {
      FUN_1006ba80(0x18, *(int *)(pCar + 0x140), pv);
    }
    else {
      FUN_1006baf0(0x18, *(int *)(pCar + 0x140), pv);
    }
  }
  else if (0x7f < *(unsigned char *)(pCar + 0x36b)) {
    FUN_1006bb10(0x18, *(int *)(pCar + 0x140));
  }
  iVar6 = *(int *)(pCar + 0xf68);
  *(char *)(pCar + 0x36b) = *(char *)(pCar + 0x36a);
  *(char *)(pCar + 0x36a) = 0;
  if (((iVar6 != 0) && ((iVar6 < 4 || (7 < iVar6)))) && ((iVar6 < 8 || (0xc < iVar6))))
    goto LAB_10061dc3;
  iVar6 = -1;
  if (*(int *)(pCar + 0x730) != 0) {
    iVar6 = (int)*(char *)(pCar + 0x71c);
  }
  uVar7 = (unsigned int)*(unsigned char *)(pCar + 0x36d);
  if (((int)uVar7 > *(int *)(pCar + 0xf6c)) ||
      (*(char *)(pCar + 0x36d) != 0 && *(int *)(pCar + 0xf70) < 0x28) ||
      (*(char *)(pCar + 0x36d) != 0 && *(int *)(pCar + 0xf70) == 0x28 && local_20 >= 4 && local_20 <= 7)) {
    *(unsigned int *)(pCar + 0xf6c) = uVar7;
    if (DAT_104b15e8 == 3) {
      switch (iVar6) {
      case 0:
      case 1:
      case 2:
      case 3:
        *(int *)(pCar + 0xf68) = 7;
        goto LAB_10061cb3;
      case 4:
        *(int *)(pCar + 0xf68) = 7;
        *(int *)(pCar + 0xf6c) = 0;
      }
    }
    else {
      switch (iVar6) {
      case 0:
      case 3:
        iVar8 = *(int *)(pCar + 0x140);
        if ((iVar8 < DAT_100b3858) && (DAT_117a5f08[iVar8] == 0)) {
          DAT_117a5f18[iVar8] = 0;
          DAT_117a5f0c[*(int *)(pCar + 0x140)] = 0;
          DAT_117a5f10[*(int *)(pCar + 0x140)] = 1;
          DAT_117a5f14[*(int *)(pCar + 0x140)] = 9;
          DAT_117a5f1c[*(int *)(pCar + 0x140)] = 10;
          DAT_117a5f08[*(int *)(pCar + 0x140)] = 1;
        }
        *(int *)(pCar + 0xf68) = 7;
        break;
      case 1:
        iVar8 = *(int *)(pCar + 0x140);
        if ((iVar8 < DAT_100b3858) && (DAT_117a5f08[iVar8] == 0)) {
          DAT_117a5f18[iVar8] = 0;
          DAT_117a5f0c[*(int *)(pCar + 0x140)] = 0;
          DAT_117a5f10[*(int *)(pCar + 0x140)] = 1;
          DAT_117a5f14[*(int *)(pCar + 0x140)] = 0xe;
          DAT_117a5f1c[*(int *)(pCar + 0x140)] = 0xf;
          DAT_117a5f08[*(int *)(pCar + 0x140)] = 1;
        }
        *(int *)(pCar + 0xf68) = 5;
        break;
      case 2:
        iVar8 = *(int *)(pCar + 0x140);
        if ((iVar8 < DAT_100b3858) && (DAT_117a5f08[iVar8] == 0)) {
          DAT_117a5f18[iVar8] = 0;
          DAT_117a5f0c[*(int *)(pCar + 0x140)] = 0;
          DAT_117a5f10[*(int *)(pCar + 0x140)] = 1;
          DAT_117a5f14[*(int *)(pCar + 0x140)] = 0xc;
          DAT_117a5f1c[*(int *)(pCar + 0x140)] = 0xd;
          DAT_117a5f08[*(int *)(pCar + 0x140)] = 1;
        }
        *(int *)(pCar + 0xf68) = 4;
        break;
      case 4:
        *(int *)(pCar + 0xf68) = 0xc;
      }
      uVar7 = (unsigned int)*(unsigned char *)(pCar + 0x36d);
LAB_10061cb3:
      *(unsigned int *)(pCar + 0xf6c) = uVar7 >> 1;
    }
    sVar2 = (*(short *)(pCar + 0x162))++;
    *(int *)(pCar + 0xf70) = 0x28;
    if (0x10 < sVar2) {
      *(short *)(pCar + 0x162) = 0x10;
    }
    *(int *)(pCar + 0xf6c) = ((int)*(short *)(pCar + 0x162) * *(int *)(pCar + 0xf6c)) / 0xc;
  }
  else {
    *(short *)(pCar + 0x162) = 0;
  }
  iVar8 = *(int *)(pCar + 0xf68);
  if ((iVar8 != 0) && ((iVar8 < 8 || (0xc < iVar8)))) goto LAB_10061dc3;
  if (DAT_104b15e8 == 3) {
    switch (iVar6) {
    case 0:
    case 1:
    case 2:
    case 3:
      *(int *)(pCar + 0xf68) = 0xb;
      break;
    case 4:
      *(int *)(pCar + 0xf68) = 10;
      break;
    default:
      *(int *)(pCar + 0xf68) = 0;
    }
  }
  else {
    switch (iVar6) {
    case 0:
    case 3:
      *(int *)(pCar + 0xf68) = 10;
      break;
    case 1:
      *(int *)(pCar + 0xf68) = 8;
      break;
    case 2:
      *(int *)(pCar + 0xf68) = 9;
      break;
    case 4:
      *(int *)(pCar + 0xf68) = 0xc;
      break;
    default:
      *(int *)(pCar + 0xf68) = 0;
    }
  }
  *(int *)(pCar + 0xf70) = 1;
  iVar6 = (int)(BrVec3Length(pCar + 0x1024) * BR_K_0077A04);
  *(int *)(pCar + 0xf6c) = iVar6;
  if (0x40 < iVar6) {
    *(int *)(pCar + 0xf6c) = 0x40;
  }
LAB_10061dc3:
  iVar6 = *(int *)(pCar + 0xf68);
  if (iVar6 == 0) {
    if (iVar9 == 0) {
      DAT_118eef6c = 0;
      DAT_118eef60 = 0;
      DAT_118eef64 = 0;
    }
  }
  else {
    local_28 = *(float *)(pCar + 0xf74) * BR_K_00779F0;
    if ((local_28 > BR_K_0077A08) || (local_28 < BR_K_00778D8)) {
      local_28 = 0.0f;
    }
    if ((iVar6 != local_20) && (iVar9 == 0)) {
      BrSfxSrcPlaySilent(1, DAT_100b32b0[iVar6 * 6], DAT_100b32bc[iVar6 * 6],
                         DAT_100b32c0[iVar6 * 6]);
    }
    if ((local_24 == 0) && (iVar9 == 0)) {
      iVar4 = *(int *)(pCar + 0xf6c) * local_1c >> 7;
      *(__int64 *)&DAT_118eef60 = (__int64)(local_28 * BR_K_00779E4 * BR_K_00779E8);
      fVar10 = (float)iVar4;
      iVar6 = (int)(fVar10 * local_18);
      iVar9 = (int)(fVar10 * local_14);
      DAT_118eef6c = iVar6 * 0x10000 + iVar9;
    }
  }
  if (DAT_100aa044 == 1) {
    iVar6 = *(int *)((char *)&DAT_10af393c + local_4) + 0x30;
    *local_10 = *(int *)iVar6;
    local_10[1] = *(int *)(iVar6 + 4);
    local_10[2] = *(int *)(iVar6 + 8);
    return;
  }
  iVar6 = *(int *)(pCar + 0x2734) + 0x30;
  *local_10 = *(int *)iVar6;
  local_10[1] = *(int *)(iVar6 + 4);
  local_10[2] = *(int *)(iVar6 + 8);
  return;
}

