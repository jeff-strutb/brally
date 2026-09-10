/* br_lapsave.c -- recycle parked cars onto empty driver slots.
 *
 * Glide 0x1005F6C0 (2104 B). Called from the race step over every car
 * when mode==0. Neighbours: BrRankAssign / BrRankCmpKey, BrRaceGateStep.
 */
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include <stdint.h>
#include <stdlib.h>

#ifdef BR_MATCHING_BUILD

#include "br_match.h"

extern int DAT_106eed48;
extern int DAT_100b2f00;
extern int DAT_100a9360;
extern int DAT_100b3858;
extern int DAT_10af07f8;
extern int DAT_10af0858;
extern char DAT_10af085c;
extern int DAT_10af0860;
extern char DAT_10af086c;
extern char DAT_10af0800;
extern char DAT_10af0804;
extern char DAT_10af0808;
extern char DAT_10af080c;
extern char DAT_10af0810;
extern char DAT_10af0814;
extern char DAT_10af0818;
extern char DAT_10af081c;
extern char DAT_10af0820;
extern char DAT_10af0824;
extern char DAT_10af0828;
extern char DAT_10af082c;
extern char DAT_10af0830;
extern char DAT_10af0834;
extern char DAT_10af0838;
extern char DAT_10af083c;
extern char DAT_10af0840;
extern char DAT_10af0844;
extern char DAT_10af0848;
extern char DAT_10af084c;
extern char DAT_10af0854;
extern char DAT_10af0855;
extern char DAT_10af0856;
extern int DAT_10af07fc;
extern int DAT_10af1208;
extern double DAT_10077990;

int BrRankCmpKey(const void *pA, const void *pB);
void BrPodNop(const char *fmt, int a, int b, int c, int d);
float BrVec3Dist(void *pA, void *pB);
void BrVec3Sub(void *pOut, void *pA, void *pB);
void BrVec3Direction(void *pOut, void *pA, void *pB);
void BrVec3Midpoint(void *pOut, void *pA, void *pB);
void BrVec3Cross(void *pOut, void *pA, void *pB);
void BrVec3NormaliseGuard(void *pV);
void BR_THISCALL1 BrSub10062C50(uint8_t *pCar);
void BR_THISCALL1 BrCarInitTables(uint8_t *pCar);
void __fastcall BrEntSetPos(uint8_t *pCar, float x, float y, float z);
void __fastcall BrEntSetVel(uint8_t *pCar, float x, float y, float z);
void __fastcall BrEntSetMatrix(uint8_t *pCar, int unused, void *pSrc);
void BrModelSlotApply(uint8_t *pCar, void *pDrv);

/* WHAT IT DOES: for this car, score every driver slot by wrapped along-track
 * delta plus world distance, sort the others by that score, then for each of
 * two groups copy lap, gate and pose from attached cars onto the driver
 * record (the "saving lap/gate" trace) and write them back onto empty slots
 * of the same group (the "restoring lap/gate" trace). */
/* RESIDUE: frame is sub esp,0xd8 and this lives in ebp. Size 2101/2104 B,
 * insn gap 5, rows 26+21, 18 regions. Unpaired: wrap-arm dist (two calls
 * vs orig's shared call), jg/jl polarity on driver-count and entrant tests,
 * saved-car lea +0x20 vs +0x1c, operand-swapped cmp on +0x64/+0x140.
 * fxch st(2) x4 is x87 scheduling. Gate 0+A4+A5+B pass; A1/A2/A3 fail. */
/* @t4-pass 0x1005F6C0 1 2026-09-09 probes 20 bytes 2101 insns 557 regions 18 rows 47 census yes */
/* @t4-pass 0x1005F6C0 2 2026-09-09 probes 20 bytes 2101 insns 557 regions 18 rows 47 census yes */
/* @implements 0x1005F6C0 glide BrLapSaveRestore */
void BR_THISCALL1 BrLapSaveRestore(uint8_t *pCar)
{
  float *pfVar1;
  unsigned char uVar2;
  unsigned char uVar3;
  int uVar4;
  int uVar5;
  int iVar6;
  uint8_t *pfVar7;
  int bVar8;
  int *piVar9;
  int iVar10;
  int iVar11;
  float *pfVar12;
  int iVar13;
  int *piVar14;
  int iVar15;
  int *puVar16;
  float fVar20;
  float local_d8;
  int local_d4;
  int local_d0;
  int local_cc;
  float *local_c8;
  float *local_c4;
  int local_c0;
  uint8_t *local_bc;
  char local_b8[12];
  char local_ac[12];
  float local_a0[40];

  fVar20 = *(float *)(DAT_106eed48 + 100);
  local_bc = pCar;
  local_d0 = 0;
  if (0 < DAT_100b2f00) {
    pfVar12 = local_a0;
    piVar14 = (int *)&DAT_10af085c;
    do {
      iVar10 = piVar14[-1];
      if (iVar10 != 0) {
        iVar13 = *(int *)(iVar10 + 0x140);
        if ((iVar10 == (int)pCar) || (iVar13 >= DAT_100b3858)) {
          if (((DAT_100a9360 == 0) &&
              (iVar13 >= DAT_100b3858)) &&
             (((*(unsigned char *)(*(int *)(iVar10 + 0xf00) + 0x68) & 2) != 0 &&
              ((*(unsigned char *)(iVar10 + 0x29af) == 2 &&
               (*(float *)(iVar10 + 0x29b0) == 0.0f)))))) {
            *pfVar12 = 1e+09f;
          }
          else {
            local_cc = *(int *)(iVar10 + 0xfac) - *(int *)(pCar + 0xfac);
            local_d8 = (*(float *)(pCar + 0xff4) - *(float *)(iVar10 + 0xff4)) +
                       (float)local_cc * fVar20;
            if (!(local_d8 <= fVar20 * 0.5f)) {
              local_d8 = local_d8 - fVar20;
              iVar10 = iVar10 + 0x30;
              *pfVar12 = local_d8 * local_d8 + BrVec3Dist(pCar + 0x30, (int *)iVar10);
            } else {
              if (local_d8 < fVar20 * -0.5f) {
                local_d8 = fVar20 + local_d8;
              }
              iVar10 = iVar10 + 0x30;
              *pfVar12 = local_d8 * local_d8 + BrVec3Dist(pCar + 0x30, (int *)iVar10);
            }
          }
        }
        else {
          *pfVar12 = 1e+10f;
        }
      }
      else if (((DAT_100a9360 == 0) && (DAT_100b3858 <= *piVar14)) &&
              ((*(unsigned char *)(piVar14 + 1) & 2) != 0)) {
        *pfVar12 = 1e+09f;
      }
      else {
        local_cc = piVar14[-8] - *(int *)(pCar + 0xfac);
        local_d8 = (*(float *)(pCar + 0xff4) - *(float *)&piVar14[-5]) +
                   (float)local_cc * fVar20;
        if (!(local_d8 <= fVar20 * 0.5f)) {
          local_d8 = local_d8 - fVar20;
        }
        else if (local_d8 < fVar20 * -0.5f) {
          local_d8 = fVar20 + local_d8;
        }
        piVar9 = piVar14 + -0x19;
        *pfVar12 = local_d8 * local_d8 + BrVec3Dist(pCar + 0x30, piVar9);
      }
      *(int *)(pfVar12 + 1) = local_d0;
      local_d0 = local_d0 + 1;
      piVar14 = piVar14 + 0x20;
      pfVar12 = pfVar12 + 2;
    } while (local_d0 < DAT_100b2f00);
  }
  if (1 < DAT_100b2f00) {
    iVar10 = *(int *)(pCar + 0x140);
    fVar20 = local_a0[iVar10 * 2];
    local_a0[iVar10 * 2] = local_a0[0];
    *(int *)(local_a0 + 1) = iVar10;
    *(int *)(local_a0 + iVar10 * 2 + 1) = 0;
    local_a0[0] = fVar20;
    qsort(local_a0 + 2, DAT_100b2f00 - 1, 8, BrRankCmpKey);
  }
  local_c0 = 0;
  do {
    iVar11 = 0;
    bVar8 = 1;
    local_d0 = 0;
    local_d4 = 0;
    iVar10 = local_c0;
    if (0 < DAT_100b2f00) {
      local_c4 = (float *)&local_cc;
      local_c8 = local_a0;
      do {
        puVar16 = &DAT_10af07f8 + *(int *)(local_c8 + 1) * 0x20;
        if ((puVar16[0x1d] == iVar10) && (DAT_100b3858 <= puVar16[0x19])) {
          if (local_d4 < 1) {
            local_d4 = local_d4 + 1;
            iVar15 = puVar16[0x18];
            if (iVar15 != 0) {
              bVar8 = 0;
              if (((*((unsigned char *)puVar16 + 0x68) & 2) != 0) &&
                 ((*(unsigned char *)(iVar15 + 0x29af) == 2 &&
                  (*(float *)(iVar15 + 0x29b0) == 0.0f)))) {
LAB_save:
                bVar8 = 0;
                puVar16[0] = *(int *)(iVar15 + 0x30);
                puVar16[1] = *(int *)(iVar15 + 0x34);
                puVar16[2] = *(int *)(iVar15 + 0x38);
                puVar16[6] = *(int *)(iVar15 + 0x1024);
                puVar16[7] = *(int *)(iVar15 + 0x1028);
                puVar16[8] = *(int *)(iVar15 + 0x102c);
                puVar16[9] = *(int *)(iVar15 + 0x1030);
                puVar16[10] = *(int *)(iVar15 + 0xf8c);
                puVar16[11] = *(int *)(iVar15 + 0xf90);
                puVar16[12] = *(int *)(iVar15 + 0xfb0);
                puVar16[13] = *(int *)(iVar15 + 0xfe4);
                puVar16[14] = *(int *)(iVar15 + 0xfec);
                puVar16[15] = *(int *)(iVar15 + 0xff0);
                puVar16[16] = *(int *)(iVar15 + 0xfa8);
                puVar16[17] = *(int *)(iVar15 + 0xfac);
                uVar4 = *(int *)(iVar15 + 0xfa0);
                puVar16[18] = uVar4;
                uVar5 = *(int *)(iVar15 + 0xfa4);
                puVar16[19] = uVar5;
                BrPodNop("saving lap (%d/%d) and gate (%d/%d)\n",
                         puVar16[16], puVar16[17], uVar4, uVar5);
                iVar10 = local_c0;
                iVar6 = puVar16[0x18];
                puVar16[20] = *(int *)(iVar15 + 0xff4);
                puVar16[21] = *(int *)(iVar15 + 0xff8);
                iVar11 = iVar11 + 1;
                puVar16[0x18] = 0;
                *(int *)local_c4 = iVar6;
                *(int *)(iVar15 + 0xf00) = 0;
                local_c4 = local_c4 + 1;
                *(int *)(iVar15 + 0xf04) = 0x3c;
              }
            }
          }
          else {
            iVar15 = puVar16[0x18];
            if (iVar15 != 0) {
              bVar8 = 0;
              if ((*(int *)(iVar15 + 0xf04) == 0) && (DAT_10077990 < *local_c8))
                goto LAB_save;
            }
          }
        }
        local_d0 = local_d0 + 1;
        local_c8 = local_c8 + 2;
      } while (local_d0 < DAT_100b2f00);
    }
    if (bVar8) {
      iVar11 = iVar11 + 1;
      (&local_d0)[iVar11] = (int)(&DAT_10af1208 + (iVar10 + DAT_100b3858) * 0xada);
    }
    local_d0 = 0;
    if (iVar11 != 0) {
      local_c4 = local_a0 + 1;
      piVar14 = &local_cc + iVar11;
      local_c8 = (float *)(local_bc + 0xeb0);
      do {
        if (DAT_100b2f00 <= local_d0) break;
        puVar16 = &DAT_10af07f8 + *(int *)local_c4 * 0x20;
        if (((puVar16[0x1d] == iVar10) &&
            (DAT_100b3858 <= puVar16[0x19])) &&
           (((*((unsigned char *)puVar16 + 0x68) & 2) == 0 &&
            (puVar16[0x18] == 0)))) {
          pfVar7 = (uint8_t *)piVar14[-1];
          piVar14 = piVar14 + -1;
          puVar16[0x18] = (int)pfVar7;
          *(int *)(pfVar7 + 0x1030) = puVar16[9];
          *(int *)(pfVar7 + 0xf8c) = puVar16[10];
          iVar11 = iVar11 + -1;
          *(int *)(pfVar7 + 0xf90) = puVar16[11];
          *(int *)(pfVar7 + 0xfb0) = puVar16[12];
          *(int *)(pfVar7 + 0xfe4) = puVar16[13];
          *(int *)(pfVar7 + 0xfec) = puVar16[14];
          *(int *)(pfVar7 + 0xff0) = puVar16[15];
          *(int *)(pfVar7 + 0xfa8) = puVar16[16];
          *(int *)(pfVar7 + 0xfac) = puVar16[17];
          uVar4 = puVar16[18];
          *(int *)(pfVar7 + 0xfa0) = uVar4;
          uVar5 = puVar16[19];
          *(int *)(pfVar7 + 0xfa4) = uVar5;
          BrPodNop("restoring lap (%d/%d) and gate (%d/%d)\n",
                   *(int *)(pfVar7 + 0xfa8),
                   *(int *)(pfVar7 + 0xfac), uVar4, uVar5);
          *(int *)(pfVar7 + 0xff4) = puVar16[20];
          *(int *)(pfVar7 + 0xff8) = puVar16[21];
          *(int **)(pfVar7 + 0xf00) = puVar16;
          pfVar7[0x29af] = 0;
          *(float *)(pfVar7 + 0x29b0) = 1.0f;
          *(int *)(pfVar7 + 0xf78) = 1;
          BrSub10062C50(pfVar7);
          BrCarInitTables(pfVar7);
          *(int *)(pfVar7 + 0xf80) = puVar16[3];
          *(int *)(pfVar7 + 0xf84) = puVar16[4];
          *(int *)(pfVar7 + 0xf88) = puVar16[5];
          BrEntSetPos(pfVar7,
                      *(float *)puVar16,
                      *(float *)(puVar16 + 1),
                      *(float *)(puVar16 + 2) - -0.1f);
          pfVar7[0x29af] = 0;
          *(float *)(pfVar7 + 0x29b0) = 1.0f;
          uVar2 = *((unsigned char *)puVar16 + 0x5e);
          uVar3 = *((unsigned char *)puVar16 + 0x5d);
          pfVar7[0x29ac] = *((unsigned char *)puVar16 + 0x5c);
          pfVar7[0x29ad] = uVar3;
          pfVar7[0x29ae] = uVar2;
          BrModelSlotApply(pfVar7, puVar16);
          iVar10 = *(int *)(pfVar7 + 0xf8c) + *(int *)(pfVar7 + 0xf90) * 0x28;
          BrVec3Sub(local_b8, (void *)(iVar10 + 0x40), (void *)(iVar10 + 0x4c));
          iVar10 = *(int *)(pfVar7 + 0xf8c) + *(int *)(pfVar7 + 0xf90) * 0x28;
          BrVec3Sub(local_ac, (void *)(iVar10 + 0x68), (void *)(iVar10 + 0x74));
          iVar10 = *(int *)(pfVar7 + 0xf8c) + *(int *)(pfVar7 + 0xf90) * 0x28;
          BrVec3Direction(pfVar7, (void *)(iVar10 + 0x4c), (void *)(iVar10 + 0x74));
          pfVar1 = (float *)(pfVar7 + 0x10);
          BrVec3Midpoint(pfVar1, local_b8, local_ac);
          BrVec3Cross((float *)(pfVar7 + 0x20), pfVar7, pfVar1);
          BrVec3NormaliseGuard(pfVar7 + 0x20);
          BrVec3Cross(pfVar1, (float *)(pfVar7 + 0x20), pfVar7);
          BrVec3NormaliseGuard(pfVar1);
          *(int *)(pfVar7 + 0xf94) = *(int *)pfVar7;
          *(int *)(pfVar7 + 0xf98) = *(int *)(pfVar7 + 4);
          *(int *)(pfVar7 + 0xf9c) = *(int *)(pfVar7 + 8);
          BrEntSetMatrix(pfVar7, 0, pfVar7);
          if ((*(unsigned char *)(*(int *)(pfVar7 + 0xf00) + 0x68) & 1) == 0) {
            BrEntSetVel(pfVar7,
                        *(float *)pfVar7 * 50.0f,
                        *(float *)(pfVar7 + 4) * 50.0f,
                        *(float *)(pfVar7 + 8) * 50.0f);
          }
          else {
            BrEntSetVel(pfVar7, 0.0f, 0.0f, 0.0f);
          }
          *(int *)(*(int *)(pfVar7 + 0x168) + 0x19c) = 0;
          *(int *)(*(int *)(pfVar7 + 0x168) + 0x1b4) = 0;
          *(unsigned char *)(*(int *)(pfVar7 + 0x168) + 0x1a0) = 2;
          *(int *)(*(int *)(pfVar7 + 0x16c) + 0x19c) = 0;
          *(int *)(*(int *)(pfVar7 + 0x16c) + 0x1b4) = 0;
          *(unsigned char *)(*(int *)(pfVar7 + 0x16c) + 0x1a0) = 2;
          *(int *)(*(int *)(pfVar7 + 0x174) + 0x19c) = 0;
          *(int *)(*(int *)(pfVar7 + 0x174) + 0x1b4) = 0;
          *(unsigned char *)(*(int *)(pfVar7 + 0x174) + 0x1a0) = 2;
          *(int *)(*(int *)(pfVar7 + 0x170) + 0x19c) = 0;
          *(int *)(*(int *)(pfVar7 + 0x170) + 0x1b4) = 0;
          *(unsigned char *)(*(int *)(pfVar7 + 0x170) + 0x1a0) = 2;
          *(int *)(pfVar7 + 0xea0) = 0;
          *(int *)(pfVar7 + 0xea4) = 0;
          *(int *)(pfVar7 + 0xea8) = 0;
          *(int *)(pfVar7 + 0xeac) = 0xffffff4c;
          *(int *)(pfVar7 + 0x35c) = 0x28;
          *(int *)(pfVar7 + 0xe20) = 0;
          iVar10 = local_c0;
        }
        iVar13 = *(int *)local_c4;
        local_c4 = local_c4 + 2;
        *(int *)local_c8 = (int)(&DAT_10af07f8 + iVar13 * 0x20);
        local_d0 = local_d0 + 1;
        local_c8 = local_c8 + 1;
      } while (iVar11 != 0);
    }
    local_c0 = iVar10 + 1;
    if (local_c0 >= 2) {
      return;
    }
  } while (1);
}

#endif /* BR_MATCHING_BUILD */
