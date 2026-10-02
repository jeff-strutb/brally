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
  BrDriverCar *self = (BrDriverCar *)this;
  float *pfVar1;
  int iVar6;
  BrDriverCar *pfVar7;
  int bVar8;
  BrDriverCar *pc;
  int pass;
  int iVar11;
  float *pfVar12;
  int iVar13;
  BrDriver *piVar14;
  BrDriverCar *iVar15;
  BrDriver *pd;
  const BrAiPathPt *pPt;
  float local_d8;
  float fLap;
  int local_d0;
  int nSaved;
  BrDriverCar *apFree[16];      /* the original's stack run from local_cc */
  BrVec3 local_b8;
  BrVec3 local_ac;
  float local_a0[40];           /* {distance key, driver index} pairs */

  fLap = BR_PTR32(BrAiPathNode *, g_brTrkHdr.aPathRoot)->aPt[0].arc;
  local_d0 = 0;
  if ((*(int *)&g_brRaceNDriver) > 0) {
    pfVar12 = local_a0;
    piVar14 = g_aBrRaceDriver;
    do {
      pc = piVar14->pCar;
      if (pc != 0) {
        if ((pc != self) && (pc->f140 < (*(int *)&g_brRaceNEntrant))) {
          *pfVar12 = 1e+10f;
        }
        else if ((((*(int *)&g_brRaceRules.mode) == 0) &&
                 (pc->f140 >= (*(int *)&g_brRaceNEntrant))) &&
                ((pc->pProfile->f68 & 2) != 0 &&
                 ((pc->b29AF == 2 && (pc->f29B0 == 0.0f))))) {
          *pfVar12 = 1e+09f;
        }
        else {
          local_d8 = (self->fFF4 - pc->fFF4) + (float)(pc->lapB - self->lapB) * fLap;
          if (!(local_d8 <= fLap * 0.5f)) {
            local_d8 = local_d8 - fLap;
          }
          else if (local_d8 < fLap * -0.5f) {
            local_d8 = fLap + local_d8;
          }
          *pfVar12 = local_d8 * local_d8 + BrVec3Dist(&self->pos, &pc->pos);
        }
      }
      else if ((((*(int *)&g_brRaceRules.mode) == 0) && (piVar14->f64 >= (*(int *)&g_brRaceNEntrant))) &&
              ((piVar14->f68 & 2) != 0)) {
        *pfVar12 = 1e+09f;
      }
      else {
        local_d8 = (self->fFF4 - piVar14->f50) + (float)(piVar14->f44 - self->lapB) * fLap;
        if (!(local_d8 <= fLap * 0.5f)) {
          local_d8 = local_d8 - fLap;
        }
        else if (local_d8 < fLap * -0.5f) {
          local_d8 = fLap + local_d8;
        }
        *pfVar12 = local_d8 * local_d8 + BrVec3Dist(&self->pos, &piVar14->f00);
      }
      *(int *)(pfVar12 + 1) = local_d0;
      local_d0 = local_d0 + 1;
      piVar14 = piVar14 + 1;
      pfVar12 = pfVar12 + 2;
    } while (local_d0 < (*(int *)&g_brRaceNDriver));
  }
  if ((*(int *)&g_brRaceNDriver) > 1) {
    iVar6 = self->f140;
    local_d8 = local_a0[iVar6 * 2];
    local_a0[iVar6 * 2] = local_a0[0];
    *(int *)(local_a0 + 1) = iVar6;
    local_a0[0] = local_d8;
    *(int *)(local_a0 + iVar6 * 2 + 1) = 0;
    qsort(local_a0 + 2, (*(int *)&g_brRaceNDriver) - 1, 8, BrRankCmpKey);
  }
  pass = 0;
  do {
    iVar11 = 0;
    bVar8 = 1;
    local_d0 = 0;
    nSaved = 0;
    if ((*(int *)&g_brRaceNDriver) > 0) {
      do {
        pd = &g_aBrRaceDriver[*(int *)(local_a0 + local_d0 * 2 + 1)];
        if (pd->f74 == pass) {
          iVar13 = pd->f64;
          iVar6 = (*(int *)&g_brRaceNEntrant);
          if (iVar13 < iVar6) {
          } else if (nSaved < 1) {
            nSaved = nSaved + 1;
            iVar15 = pd->pCar;
            if (iVar15 != 0) {
              bVar8 = 0;
              if (((pd->f68 & 2) != 0) &&
                 ((iVar15->b29AF == 2 && (iVar15->f29B0 == 0.0f)))) {
LAB_save:
                pd->f00 = iVar15->pos;
                pd->f18 = iVar15->f1024;
                *(int32_t *)&pd->f24 = *(int32_t *)&iVar15->f1030;
                pd->pPathNode = iVar15->pNode.p;
                pd->f2C = (int32_t)iVar15->iPt.v;
                pd->f30 = iVar15->tRun;
                pd->f34 = iVar15->tBest;
                *(float *)&pd->f38 = iVar15->tFinal;
                *(float *)&pd->f3C = iVar15->fFF0;
                pd->f40 = iVar15->lap;
                pd->f44 = iVar15->lapB;
                pd->f48 = iVar15->gateHi;
                pd->f4C = iVar15->gate;
                BrPodNop("saving lap (%d/%d) and gate (%d/%d)\n",
                         pd->f40, pd->f44, pd->f48, pd->f4C);
                pd->f50 = iVar15->fFF4;
                pd->f54 = iVar15->fFF8;
                apFree[iVar11] = pd->pCar;
                iVar11 = iVar11 + 1;
                pd->pCar = 0;
                iVar15->pProfile = 0;
                iVar15->fF04 = 0x3c;
              }
            }
          }
          else {
            iVar15 = pd->pCar;
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
      apFree[iVar11] = &g_aBrRaceCar[pass + (*(int *)&g_brRaceNEntrant)];
      iVar11 = iVar11 + 1;
    }
    local_d0 = 0;
    while (iVar11 != 0) {
      if (local_d0 >= (*(int *)&g_brRaceNDriver)) break;
      pd = &g_aBrRaceDriver[*(int *)(local_a0 + local_d0 * 2 + 1)];
      if (((pd->f74 == pass) && (!(pd->f64 < (*(int *)&g_brRaceNEntrant)))) &&
          (((pd->f68 & 2) == 0 && (pd->pCar == 0)))) {
        iVar11 = iVar11 + -1;
        pfVar7 = apFree[iVar11];
        pd->pCar = pfVar7;
        *(int32_t *)&pfVar7->f1030 = *(int32_t *)&pd->f24;
        pfVar7->pNode.p = pd->pPathNode;
        pfVar7->iPt.v = (uint32_t)pd->f2C;
        pfVar7->tRun = pd->f30;
        pfVar7->tBest = pd->f34;
        pfVar7->tFinal = *(float *)&pd->f38;
        pfVar7->fFF0 = *(float *)&pd->f3C;
        pfVar7->lap = pd->f40;
        pfVar7->lapB = pd->f44;
        pfVar7->gateHi = pd->f48;
        pfVar7->gate = pd->f4C;
        BrPodNop("restoring lap (%d/%d) and gate (%d/%d)\n",
                 pfVar7->lap, pfVar7->lapB, pfVar7->gateHi, pfVar7->gate);
        pfVar7->fFF4 = pd->f50;
        pfVar7->fFF8 = pd->f54;
        pfVar7->pProfile = pd;
        pfVar7->b29AF = 0;
        pfVar7->f29B0 = 1.0f;
        pfVar7->fF78 = 1;
        ((BrCar *)pfVar7)->Sub10062C50();
        ((BrCar *)pfVar7)->InitTables();
        pfVar7->posPrev = pd->f0C;
        ((BrCar *)pfVar7)->SetPos(pd->f00.x, pd->f00.y, pd->f00.z - -0.1f);
        pfVar7->b29AF = 0;
        pfVar7->f29B0 = 1.0f;
        pfVar7->f29AC = pd->f5C;
        pfVar7->f29AD = pd->f5D;
        pfVar7->f29AE = pd->f5E;
        BrModelSlotApply(pfVar7, pd);
        pPt = &pfVar7->pNode.p->aPt[pfVar7->iPt.v];
        BrVec3Sub(&local_b8, &pPt[0].left, &pPt[0].centre);
        BrVec3Sub(&local_ac, &pPt[1].left, &pPt[1].centre);
        BrVec3Direction(&pfVar7->fwd, &pPt[0].centre, &pPt[1].centre);
        pfVar1 = &pfVar7->right.x;
        BrVec3Midpoint((struct BrVec3 *)(pfVar1), &local_b8, &local_ac);
        BrVec3Cross(&pfVar7->up, &pfVar7->fwd, (const struct BrVec3 *)(pfVar1));
        br_dl_normalise(&pfVar7->up);
        BrVec3Cross((struct BrVec3 *)(pfVar1), &pfVar7->up, &pfVar7->fwd);
        br_dl_normalise((struct BrVec3 *)pfVar1);
        V3Copy(&pfVar7->f0F94, &pfVar7->fwd.x);
        ((BrCar *)pfVar7)->SetMatrix(&pfVar7->fwd.x);
        if ((pfVar7->pProfile->f68 & 1) != 0) {
          ((BrCar *)pfVar7)->SetVel(0.0f, 0.0f, 0.0f);
        }
        else {
          ((BrCar *)pfVar7)->SetVel(pfVar7->fwd.x * 50.0f,
                                    pfVar7->fwd.y * 50.0f,
                                    pfVar7->fwd.z * 50.0f);
        }
        pfVar7->aBody[0].rb.child[0]->pPlane = 0;
        *(int *)&pfVar7->aBody[0].rb.child[0]->f1B4 = 0;
        pfVar7->aBody[0].rb.child[0]->f01A0 = 2;
        pfVar7->aBody[0].rb.child[1]->pPlane = 0;
        *(int *)&pfVar7->aBody[0].rb.child[1]->f1B4 = 0;
        pfVar7->aBody[0].rb.child[1]->f01A0 = 2;
        pfVar7->aBody[0].rb.child[3]->pPlane = 0;
        *(int *)&pfVar7->aBody[0].rb.child[3]->f1B4 = 0;
        pfVar7->aBody[0].rb.child[3]->f01A0 = 2;
        pfVar7->aBody[0].rb.child[2]->pPlane = 0;
        *(int *)&pfVar7->aBody[0].rb.child[2]->f1B4 = 0;
        pfVar7->aBody[0].rb.child[2]->f01A0 = 2;
        pfVar7->cHoldFwd = 0;
        pfVar7->cHoldRev = 0;
        pfVar7->cRevRun = 0;
        pfVar7->cFwdRun = 0xffffff4c;
        pfVar7->aBody[0].f01F8 = 0x28;
        *(int *)&pfVar7->f0E20 = 0;
      }
      self->apRank[local_d0] = &g_aBrRaceDriver[*(int *)(local_a0 + local_d0 * 2 + 1)];
      local_d0 = local_d0 + 1;
    }
    pass = pass + 1;
  } while (pass < 2);
}

/* C entry points (generated by ports/64b/tools/methodfwd.py) */
extern "C" void BrLapSaveRestore(void *self)
{
    ((class BrCar *)self)->LapSaveRestore();
}
/* end of C entry points */
