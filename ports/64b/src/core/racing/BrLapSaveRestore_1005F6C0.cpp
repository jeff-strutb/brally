/* WHAT IT DOES: for this car, score every driver slot by wrapped along-track
 * delta plus world distance, sort the others by that score, then for each of
 * two groups copy lap, gate and pose from attached cars onto the driver
 * record (the "saving lap/gate" trace) and write them back onto empty slots
 * of the same group (the "restoring lap/gate" trace). */
/* RESIDUE: 1 byte of 2104.  At +0x252 the original spills the group counter
 * from its own register (`xor edx,edx; mov [esp+0x28],edx`, 89 54); ours
 * constant-propagates the zero and stores the zero register (89 5c).  Every
 * other byte, reloc-masked, is identical.
 * Source facts that closed the rest (2026-09-21, from 2076 B / 47 rows):
 *   - C++ member: BrEntSetMatrix is a two-argument thiscall; the C fastcall
 *     shim cost a dummy `xor edx,edx`.
 *   - score loop is positive && chains with one 1e9 store per arm and one
 *     BrVec3Dist call per arm (the compiler cross-jumps the calls).
 *   - the lap delta is written inline, `(float)(carLap - myLap)`; a named int
 *     temp is what produced the "x87 scheduling wall" (fxch st(2) x4).
 *   - the rank / saved-car / slot walks are INDEXED; the pointer walks and
 *     `sub r,4` are the compiler's own strength reduction.
 *   - restore loop is `while (n != 0) { if (i >= count) break; ... }`.
 *   - trace-call arguments are read back from the records, not held in temps.
 *   - tint bytes pass through three int temps (xor/mov al x3), read b,g,r.
 *   - the facing copy is an inlined V3Copy(dst, src) -- strict load/store
 *     interleave because the inlined pointers may alias.
 *   - zero-velocity arm first; qsort swap writes key before clearing index.
 * Dead for the last byte (13 probes): for-loop forms, store at loop top,
 * chained assignment, goto entry, unsigned either side, volatile slot,
 * address-taken slot, single compiler-spilled variable (rotates everything),
 * slot-first init. */
/* @t4-pass 0x1005F6C0 5 2026-09-21 probes 58 bytes 2104 insns 562 regions 1 rows 0 census yes */
/* @t4-pass 0x1005F6C0 6 2026-09-21 probes 13 bytes 2104 insns 562 regions 1 rows 0 census no */
/* @t3 0x1005F6C0 2026-09-21 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 2104/2104 insns 562/562 rows 0+0 regions 1 oracle EQUIVALENT
 * @t3-effort passes 2 zero-movement 5 6
 * Residue is one register choice in one store (see RESIDUE above); A5 oracle
 * EQUIVALENT on the C++ object.  Passes 1-4 are in the git history of
 * src/core/racing/br_lapsave.c (C lane, 2076 B). */
/* @implements 0x1005F6C0 glide BrLapSaveRestore
 * @cpp_symbol ?LapSaveRestore@BrCar@@QAEXXZ */
#define _CRTIMP __declspec(dllimport)
#include "br_trkhdr.h"   /* g_brTrkHdr, the loaded track header */
#include "br_race.h"   /* br_globals: its objects */
#include <stdint.h>
#include "slice3_41.h"
#include <stdlib.h>

class BrCar {
public:
    void LapSaveRestore();
    void Sub10062C50();
    void InitTables();
    void SetPos(float x, float y, float z);
    void SetVel(float x, float y, float z);
    void SetMatrix(void *pSrc);
};
#define pCar ((uint8_t *)this)

extern "C" {
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

/* 64-bit core: declared once, by its definition's header */
/* BrPodNop: prototype in br_funcs.h */
/* BrVec3Dist: prototype in br_funcs.h */
/* BrVec3Sub: prototype in br_funcs.h */
/* BrVec3Direction: prototype in br_funcs.h */
/* BrVec3Midpoint: prototype in br_funcs.h */
/* BrVec3Cross: prototype in br_funcs.h */
/* BrVec3NormaliseGuard: prototype in br_funcs.h */
struct BrV3 { float x, y, z; };
}
inline void V3Copy(float *d, float *s) { d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; }
extern "C" {
/* BrModelSlotApply: prototype in br_funcs.h */
}

void BrCar::LapSaveRestore()
{
  float *pfVar1;
  unsigned int uTintB;
  unsigned int uTintG;
  unsigned int uTintR;
  int iVar6;
  BrDriverCar *pfVar7;
  int bVar8;
  BrDriverCar *iVar10;
  int iVar11;
  float *pfVar12;
  int iVar13;
  BrDriver *piVar14;
  BrDriverCar *iVar15;
  int *puVar16;
  float local_d8;
  union { float f; int i; } local_d4;
  int local_d0;
  int local_cc;
  int local_c0;
  char local_b8[12];
  char local_ac[12];
  float local_a0[40];

  local_d4.f = BR_PTR32(BrAiPathNode *, g_brTrkHdr.aPathRoot)->aPt[0].arc;
  local_d0 = 0;
  if ((*(int *)&g_brRaceNDriver) <= 0) {
  } else {
    pfVar12 = local_a0;
    piVar14 = g_aBrRaceDriver;   /* the walker addresses whole driver records */
    do {
      iVar10 = *(int *)&piVar14->pCar;
      if (iVar10 != 0) {
        if ((iVar10 != (int)pCar) && (iVar10->f140 < (*(int *)&g_brRaceNEntrant))) {
          *pfVar12 = 1e+10f;
        }
        else if ((((*(int *)&g_brRaceRules.mode) == 0) &&
                 (iVar10->f140 >= (*(int *)&g_brRaceNEntrant))) &&
                (((*(unsigned char *)(((char *)&((BrDriver *)(iVar10->pProfile))->f68)) & 2) != 0 &&
                 ((iVar10->b29AF == 2 &&
                  (iVar10->f29B0 == 0.0f)))))) {
          *pfVar12 = 1e+09f;
        }
        else {
          local_d8 = (*(float *)(pCar + 0xff4) - iVar10->fFF4) +
                     (float)(iVar10->lapB - *(int *)(pCar + 0xfac)) * local_d4.f;
          if (!(local_d8 <= local_d4.f * 0.5f)) {
            local_d8 = local_d8 - local_d4.f;
          }
          else if (local_d8 < local_d4.f * -0.5f) {
            local_d8 = local_d4.f + local_d8;
          }
          *pfVar12 = local_d8 * local_d8 + BrVec3Dist((const struct BrVec3 *)(pCar + 0x30), (const struct BrVec3 *)(&iVar10->pos.x));
        }
      }
      else if ((((*(int *)&g_brRaceRules.mode) == 0) && (piVar14->f64 >= (*(int *)&g_brRaceNEntrant))) &&
              ((*(unsigned char *)&piVar14->f68 & 2) != 0)) {
        *pfVar12 = 1e+09f;
      }
      else {
        local_d8 = (*(float *)(pCar + 0xff4) - *(float *)&*(int *)&piVar14->f50) +
                   (float)(piVar14->f44 - *(int *)(pCar + 0xfac)) * local_d4.f;
        if (!(local_d8 <= local_d4.f * 0.5f)) {
          local_d8 = local_d8 - local_d4.f;
        }
        else if (local_d8 < local_d4.f * -0.5f) {
          local_d8 = local_d4.f + local_d8;
        }
        *pfVar12 = local_d8 * local_d8 + BrVec3Dist((const struct BrVec3 *)(pCar + 0x30), (const struct BrVec3 *)(&piVar14->f00.x));
      }
      *(int *)(pfVar12 + 1) = local_d0;
      local_d0 = local_d0 + 1;
      piVar14 = piVar14 + 1;
      pfVar12 = pfVar12 + 2;
    } while (local_d0 < (*(int *)&g_brRaceNDriver));
  }
  if ((*(int *)&g_brRaceNDriver) <= 1) {
  } else {
    iVar10 = *(int *)(pCar + 0x140);
    local_d8 = local_a0[iVar10 * 2];
    local_a0[iVar10 * 2] = local_a0[0];
    *(int *)(local_a0 + 1) = iVar10;
    local_a0[0] = local_d8;
    *(int *)(local_a0 + iVar10 * 2 + 1) = 0;
    qsort(local_a0 + 2, (*(int *)&g_brRaceNDriver) - 1, 8, BrRankCmpKey);
  }
  iVar10 = 0;
  local_c0 = iVar10;
  do {
    iVar11 = 0;
    bVar8 = 1;
    local_d0 = 0;
    local_d4.i = 0;
    if ((*(int *)&g_brRaceNDriver) <= 0) {
    } else {
      do {
        puVar16 = ((int *)&g_aBrRaceDriver[(*(int *)(local_a0 + local_d0 * 2 + 1))].f00.x);
        if (puVar16[0x1d] == iVar10) {
          iVar13 = puVar16[0x19];
          iVar6 = (*(int *)&g_brRaceNEntrant);
          if (iVar13 < iVar6) {
          } else if (local_d4.i < 1) {
            local_d4.i = local_d4.i + 1;
            iVar15 = puVar16[0x18];
            if (iVar15 != 0) {
              bVar8 = 0;
              if (((*((unsigned char *)puVar16 + 0x68) & 2) != 0) &&
                 ((iVar15->b29AF == 2 &&
                  (iVar15->f29B0 == 0.0f)))) {
LAB_save:
                puVar16[0] = *(int *)&iVar15->pos.x;
                puVar16[1] = *(int *)&iVar15->pos.y;
                puVar16[2] = *(int *)&iVar15->pos.z;
                puVar16[6] = *(int *)&iVar15->f1024.x;
                puVar16[7] = *(int *)&iVar15->f1024.y;
                puVar16[8] = *(int *)&iVar15->f1024.z;
                puVar16[9] = *(int *)&iVar15->f1030;
                puVar16[10] = *(int *)&iVar15->pNode;
                puVar16[11] = *(int *)&iVar15->iPt;
                puVar16[12] = *(int *)&iVar15->tRun;
                puVar16[13] = *(int *)&iVar15->tBest;
                puVar16[14] = *(int *)&iVar15->tFinal;
                puVar16[15] = *(int *)&iVar15->fFF0;
                puVar16[16] = iVar15->lap;
                puVar16[17] = iVar15->lapB;
                puVar16[18] = iVar15->gateHi;
                puVar16[19] = iVar15->gate;
                BrPodNop("saving lap (%d/%d) and gate (%d/%d)\n",
                         puVar16[16], puVar16[17], puVar16[18], puVar16[19]);
                iVar6 = puVar16[0x18];
                puVar16[20] = *(int *)&iVar15->fFF4;
                puVar16[21] = iVar15->fFF8;
                iVar10 = local_c0;
                (&local_cc)[iVar11] = iVar6;
                iVar11 = iVar11 + 1;
                puVar16[0x18] = 0;
                iVar15->pProfile = 0;
                iVar15->fF04 = 0x3c;
              }
            }
          }
          else {
            iVar15 = puVar16[0x18];
            if (iVar15 != 0) {
              bVar8 = 0;
              if ((iVar15->fF04 == 0) && (DAT_10077990 < local_a0[local_d0 * 2]))
                goto LAB_save;
            }
          }
        }
        local_d0 = local_d0 + 1;
      } while (local_d0 < (*(int *)&g_brRaceNDriver));
    }
    if (bVar8) {
      (&local_cc)[iVar11] = (int)(&(*(int *)&g_aBrRaceCar) + (iVar10 + (*(int *)&g_brRaceNEntrant)) * 0xada);
      iVar11 = iVar11 + 1;
    }
    local_d0 = 0;
    while (iVar11 != 0) {
      {
        if (local_d0 >= (*(int *)&g_brRaceNDriver)) break;
        puVar16 = ((int *)&g_aBrRaceDriver[(*(int *)(local_a0 + local_d0 * 2 + 1))].f00.x);
        if (((puVar16[0x1d] == iVar10) &&
            (!(puVar16[0x19] < (*(int *)&g_brRaceNEntrant)))) &&
           (((*((unsigned char *)puVar16 + 0x68) & 2) == 0 &&
            (puVar16[0x18] == 0)))) {
          iVar11 = iVar11 + -1;
          pfVar7 = (BrDriverCar *)((uint8_t *)(&local_cc)[iVar11]);
          puVar16[0x18] = (int)pfVar7;
          *(int *)&pfVar7->f1030 = puVar16[9];
          *(int *)&pfVar7->pNode = puVar16[10];
          *(int *)&pfVar7->iPt = puVar16[11];
          *(int *)&pfVar7->tRun = puVar16[12];
          *(int *)&pfVar7->tBest = puVar16[13];
          *(int *)&pfVar7->tFinal = puVar16[14];
          *(int *)&pfVar7->fFF0 = puVar16[15];
          pfVar7->lap = puVar16[16];
          pfVar7->lapB = puVar16[17];
          pfVar7->gateHi = puVar16[18];
          pfVar7->gate = puVar16[19];
          BrPodNop("restoring lap (%d/%d) and gate (%d/%d)\n",
                   pfVar7->lap,
                   pfVar7->lapB, pfVar7->gateHi, pfVar7->gate);
          *(int *)&pfVar7->fFF4 = puVar16[20];
          pfVar7->fFF8 = puVar16[21];
          *(int * *)&pfVar7->pProfile = puVar16;
          pfVar7->b29AF = 0;
          pfVar7->f29B0 = 1.0f;
          pfVar7->fF78 = 1;
          ((BrCar *)&pfVar7->fwd.x)->Sub10062C50();
          ((BrCar *)&pfVar7->fwd.x)->InitTables();
          *(int *)&pfVar7->posPrev.x = puVar16[3];
          *(int *)&pfVar7->posPrev.y = puVar16[4];
          *(int *)&pfVar7->posPrev.z = puVar16[5];
          ((BrCar *)&pfVar7->fwd.x)->SetPos(
                      *(float *)puVar16,
                      *(float *)(puVar16 + 1),
                      *(float *)(puVar16 + 2) - -0.1f);
          pfVar7->b29AF = 0;
          pfVar7->f29B0 = 1.0f;
          uTintB = *((unsigned char *)puVar16 + 0x5e);
          uTintG = *((unsigned char *)puVar16 + 0x5d);
          uTintR = *((unsigned char *)puVar16 + 0x5c);
          pfVar7->f29AC = (uint8_t)uTintR;
          pfVar7->f29AD = (uint8_t)uTintG;
          pfVar7->f29AE = (uint8_t)uTintB;
          BrModelSlotApply(pfVar7, puVar16);
          iVar10 = *(int *)&pfVar7->pNode + *(int *)&pfVar7->iPt * 0x28;
          BrVec3Sub(local_b8, &iVar10->aWheel[0].m[0], (void *)&iVar10->aWheel[0].m[0][3]);
          iVar10 = *(int *)&pfVar7->pNode + *(int *)&pfVar7->iPt * 0x28;
          BrVec3Sub(local_ac, (void *)&iVar10->aWheel[0].m[2][2], (void *)&iVar10->aWheel[0].m[3][1]);
          iVar10 = *(int *)&pfVar7->pNode + *(int *)&pfVar7->iPt * 0x28;
          BrVec3Direction((struct BrVec3 *)(&pfVar7->fwd.x), (void *)&iVar10->aWheel[0].m[0][3], (void *)&iVar10->aWheel[0].m[3][1]);
          pfVar1 = &pfVar7->right.x;
          BrVec3Midpoint((struct BrVec3 *)(pfVar1), local_b8, local_ac);
          BrVec3Cross((struct BrVec3 *)(&pfVar7->up.x), (const struct BrVec3 *)(&pfVar7->fwd.x), (const struct BrVec3 *)(pfVar1));
          br_dl_normalise(&pfVar7->up.x);
          BrVec3Cross((struct BrVec3 *)(pfVar1), (const struct BrVec3 *)(&pfVar7->up.x), (const struct BrVec3 *)(&pfVar7->fwd.x));
          br_dl_normalise(pfVar1);
          V3Copy(&pfVar7->f0F94, &pfVar7->fwd.x);
          ((BrCar *)&pfVar7->fwd.x)->SetMatrix(&pfVar7->fwd.x);
          if ((*(unsigned char *)(((char *)&((BrDriver *)(pfVar7->pProfile))->f68)) & 1) != 0) {
            ((BrCar *)&pfVar7->fwd.x)->SetVel( 0.0f, 0.0f, 0.0f);
          }
          else {
            ((BrCar *)&pfVar7->fwd.x)->SetVel(
                        pfVar7->fwd.x * 50.0f,
                        pfVar7->fwd.y * 50.0f,
                        pfVar7->fwd.z * 50.0f);
          }
          pfVar7->aBody[0].rb.child[0]->pPlane = (struct BrCollPlane *)(intptr_t)(0);
          *(int *)&pfVar7->aBody[0].rb.child[0]->f1B4 = 0;
          pfVar7->aBody[0].rb.child[0]->f01A0 = 2;
          pfVar7->aBody[0].rb.child[1]->pPlane = (struct BrCollPlane *)(intptr_t)(0);
          *(int *)&pfVar7->aBody[0].rb.child[1]->f1B4 = 0;
          pfVar7->aBody[0].rb.child[1]->f01A0 = 2;
          pfVar7->aBody[0].rb.child[3]->pPlane = (struct BrCollPlane *)(intptr_t)(0);
          *(int *)&pfVar7->aBody[0].rb.child[3]->f1B4 = 0;
          pfVar7->aBody[0].rb.child[3]->f01A0 = 2;
          pfVar7->aBody[0].rb.child[2]->pPlane = (struct BrCollPlane *)(intptr_t)(0);
          *(int *)&pfVar7->aBody[0].rb.child[2]->f1B4 = 0;
          pfVar7->aBody[0].rb.child[2]->f01A0 = 2;
          pfVar7->cHoldFwd = 0;
          pfVar7->cHoldRev = 0;
          pfVar7->cRevRun = 0;
          pfVar7->cFwdRun = 0xffffff4c;
          pfVar7->aBody[0].f01F8 = 0x28;
          *(int *)&pfVar7->f0E20 = 0;
          iVar10 = local_c0;
        }
        *(int *)(pCar + 0xeb0 + local_d0 * 4) = (int)(&(*(int *)&g_aBrRaceDriver) + *(int *)(local_a0 + local_d0 * 2 + 1) * 0x20);
        local_d0 = local_d0 + 1;
      }
    }
    iVar10 = iVar10 + 1;
    local_c0 = iVar10;
  } while (iVar10 < 2);
}

/* C entry points (generated by ports/64b/tools/methodfwd.py) */
extern "C" void BrLapSaveRestore(void *self)
{
    ((class BrCar *)self)->LapSaveRestore();
}
/* end of C entry points */
