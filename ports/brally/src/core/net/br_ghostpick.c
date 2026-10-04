/* br_ghostpick.c -- matching arm for BrGhostPickBlend (0x10005810).
 *
 * Under the shared network mutex, pick the two freshest remote snapshots for a
 * peer slot, interpolate the local car state between them (weighted by how far
 * the newest snapshot has aged), fold the along-track delta into the result,
 * then renormalise the car's orientation quaternion and wrap its angles.
 */
#include "slice1_02.h"   /* br_globals: its objects */
#include <windows.h>

/* BrTicks30FromMs: prototype in br_funcs.h */
/* BrCarStateLerp: prototype in br_funcs.h */
/* BrVec3Length_100682C0: prototype in br_funcs.h */
/* BrQuatNormalise_1006D410: prototype in br_funcs.h */
/* BrAngleWrap_10005C40: prototype in br_funcs.h */
/* BrAngleWrap_10005C70: prototype in br_funcs.h */
/* BrAngleWrap_10005CA0: prototype in br_funcs.h */

/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* WHAT IT DOES: under the network mutex, choose the two freshest snapshots for
 * this peer, interpolate the car state between them by the newest snapshot's
 * age, add the along-track distance delta, release the mutex and renormalise
 * the car's quaternion and wrap its three angle channels; returns 0 if the slot
 * has too few snapshots, else 1. */
/* RESIDUE (2026-09-20): bytes 1000/1070, 2 masked regions.  Hand transcription
 * (this function had only a Ghidra draft, no project code).  Accessing every
 * per-slot field through the base pointer (as the original does) rather than
 * absolute &g_1021xxxx[slot*0x25e] collapsed most of the gap; the remainder is
 * colouring, read out of the diff:
 *   - the original spills the per-slot scalars to stack temps and re-reads them
 *     (mov [esp+S],R / mov R,[base+I]) where ours keeps them in registers;
 *   - the 0x28-dword snapshot copy is a rep movsd in the original, an unrolled
 *     store loop here (same effect);
 *   - x87 conversion width in the age-weight divide (fidiv vs fild+fdiv) and
 *     the frame size that follows from the spill choice.
 * A5 oracle EQUIVALENT on 48 seeds (oracle_profiles _gp_bss/_gp_arg): the
 * two-freshest snapshot selection, the interpolation weight and the along-track
 * delta fold are all verified -- negative controls on the pick, the weight and
 * the delta fire DIFF; the two KERNEL32 mutex imports are black-boxed via their
 * own IAT slots and the timer/quat/angle post-passes are stubbed.  No source
 * lever moved the byte count off 1000 across 11 spellings and 5 opt levels. */
/* @t4-pass 0x10005810 1 2026-09-20 probes 11 bytes 1000 insns 310 regions 2 rows 76 census no  (fn.py: loop-condition and decrement spellings, compare operand order, += fold, address-of vs +1, cast drop, O2/O2y/O2p/Ox/O1.  Best -14 bytes at O2p, -70 at O2; none reached 0.  Residue is per-slot register-vs-stack spilling + rep-movsd-vs-loop + x87 divide width.) */
/* @t4-pass 0x10005810 2 2026-09-20 probes 11 bytes 1000 insns 310 regions 2 rows 76 census yes  (census of unpaired multiset: MISSING mov [esp+S],R + mov R,[base+I] = the original's spill-and-reread of the per-slot scalars vs our register residency; MISSING rep movsd = the snapshot block copy vs our unrolled loop; EXTRA fild qword / fstp reg vs MISSING fld reg + fidiv = x87 conversion-width in the age-weight divide; the add/sub esp deltas are the frame size that follows from the spill choice.  Every divergent row is register allocation, a copy idiom, or x87 width of identical logic.  A5 oracle EQUIVALENT on 48 seeds.) */
/* @t3 0x10005810 2026-09-20 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 1000/1070 insns 310/328 rows 47+29 regions 2 oracle EQUIVALENT
 * @t3-effort passes 2 zero-movement 1 2
 */
/* @implements 0x10005810 glide BrGhostPickBlend */
unsigned int BrGhostPickBlend(float *param_1, int param_2)
{
  float *pfVar1;
  BrNetSlot *pSlot;
  float *pfVar3;
  int iVar4;
  int iVar5;
  int iVar6;
  int iVar7;
  unsigned int *puVar8;
  int iVar9;
  float *pfVar10;
  float *pfVar11;
  float fVar12;
  float fVar13;
  HANDLE ahWait[2];     /* the two mutexes, waited on together */
  float v3[3];          /* a vector on the stack, measured by BrVec3Length */

  iVar4 = param_2;
  pfVar3 = param_1;
  ahWait[0] = (*(HANDLE *)&g_hBrNetMutex);
  ahWait[1] = (HANDLE)g_aBrNetSlot[param_2].hMutex;
  pSlot = &g_aBrNetSlot[param_2];
  WaitForMultipleObjects(2, ahWait, 1, 0xffffffff);
  if (param_2 != g_id) {
    if ((int)pSlot->f558 < 2) {
      param_1[0x1f] = 400.0f;
      ReleaseMutex(pSlot->hMutex);
      ReleaseMutex((*(HANDLE *)&g_hBrNetMutex));
      return 0;
    }
    iVar5 = 0;
    iVar9 = 0;
    param_1 = (float *)0x0;
    puVar8 = pSlot->f00C;   /* f038 is 0x2C bytes on: puVar8[0xb] */
    do {
      if ((puVar8[0xb] != 0) && (param_1 < (float *)*puVar8)) {
        iVar9 = iVar5;
        param_1 = (float *)*puVar8;
      }
      iVar5 = iVar5 + 1;
      puVar8 = puVar8 + 1;
    } while (iVar5 < 8);
    iVar6 = 0;
    iVar5 = 0;
    param_2 = 0;
    param_1 = (float *)0x0;
    puVar8 = pSlot->f00C;   /* f038 is 0x2C bytes on: puVar8[0xb] */
    do {
      if (((puVar8[0xb] != 0) && (iVar5 = param_2, param_1 < (float *)*puVar8)) && (iVar6 != iVar9)) {
        iVar5 = iVar6;
        param_1 = (float *)*puVar8;
        param_2 = iVar6;
      }
      iVar6 = iVar6 + 1;
      puVar8 = puVar8 + 1;
    } while (iVar6 < 8);
    iVar6 = (int)pSlot->f00C[iVar9] - (int)pSlot->f00C[iVar5];
    if (pSlot->f560 == iVar9) {
      if ((int)pSlot->f568 < 0xf) {
        pSlot->f568 = pSlot->f568 + 1;
        pSlot->f564 = pSlot->f564 + 1;
      }
      if (iVar6 == 0) {
        pfVar10 = (float *)&pSlot->cars[pSlot->f560];
        pfVar11 = pfVar3;
        for (iVar9 = 0x28; iVar9 != 0; iVar9 = iVar9 + -1) {
          *pfVar11 = *pfVar10;
          pfVar10 = pfVar10 + 1;
          pfVar11 = pfVar11 + 1;
        }
        goto LAB_done;
      }
      iVar7 = BrTicks30FromMs();
      iVar7 = iVar7 - (int)pSlot->f00C[iVar9];
      if (6 < iVar7) {
        iVar7 = 6;
      }
      BrCarStateLerp(pfVar3, (float)(iVar7 + iVar6) / (float)iVar6, &pSlot->cars[iVar5],
                     &pSlot->cars[iVar9]);
      v3[0] = pSlot->cars[iVar9].f10;   /* the stored float, copied as a dword */
      v3[1] = pSlot->cars[iVar9].f14;   /* the stored float, copied as a dword */
      v3[2] = pSlot->cars[iVar9].f18;   /* the stored float, copied as a dword */
      fVar12 = BrGroundProbeZ(v3);
      v3[0] = pfVar3[4];
      v3[1] = pfVar3[5];
      v3[2] = pfVar3[6];
      fVar13 = BrGroundProbeZ(v3);
    } else {
      pSlot->f560 = iVar9;
      if ((((unsigned int)(int)pSlot->f00C[iVar9] < (unsigned int)(pSlot->f564 + 1)) &&
           ((int)pSlot->f56C < 0x14)) && (iVar6 != 0)) {
        pSlot->f568 = 1;
        pSlot->f56C = pSlot->f56C + 1;
        pSlot->f564 = pSlot->f564 + 1;
        iVar7 = BrTicks30FromMs();
        iVar7 = iVar7 - (int)pSlot->f00C[iVar9];
        if (6 < iVar7) {
          iVar7 = 6;
        }
        BrCarStateLerp(pfVar3, (float)(iVar7 + iVar6) / (float)iVar6, &pSlot->cars[iVar5],
                       &pSlot->cars[iVar9]);
        v3[0] = pSlot->cars[iVar9].f10;   /* the stored float, copied as a dword */
        v3[1] = pSlot->cars[iVar9].f14;   /* the stored float, copied as a dword */
        v3[2] = pSlot->cars[iVar9].f18;   /* the stored float, copied as a dword */
        fVar12 = BrGroundProbeZ(v3);
        v3[0] = pfVar3[4];
        v3[1] = pfVar3[5];
        v3[2] = pfVar3[6];
        fVar13 = BrGroundProbeZ(v3);
      } else {
        pSlot->f568 = 0;
        pSlot->f56C = 0;
        pSlot->f564 = (int)pSlot->f00C[iVar9];
        iVar7 = BrTicks30FromMs();
        iVar7 = iVar7 - (int)pSlot->f00C[iVar9];
        if (6 < iVar7) {
          iVar7 = 6;
        }
        BrCarStateLerp(pfVar3, (float)(iVar7 + iVar6) / (float)iVar6, &pSlot->cars[iVar5],
                       &pSlot->cars[iVar9]);
        v3[0] = pSlot->cars[iVar9].f10;   /* the stored float, copied as a dword */
        v3[1] = pSlot->cars[iVar9].f14;   /* the stored float, copied as a dword */
        v3[2] = pSlot->cars[iVar9].f18;   /* the stored float, copied as a dword */
        fVar12 = BrGroundProbeZ(v3);
        v3[0] = pfVar3[4];
        v3[1] = pfVar3[5];
        v3[2] = pfVar3[6];
        fVar13 = BrGroundProbeZ(v3);
      }
    }
    pfVar3[6] = (fVar12 - fVar13) + pfVar3[6];
  }
LAB_done:
  ReleaseMutex(pSlot->hMutex);
  ReleaseMutex((*(HANDLE *)&g_hBrNetMutex));
  BrCarClampUnit(pfVar3);
  pfVar10 = pfVar3 + 1;
  BrCarClampUnit(pfVar10);
  pfVar11 = pfVar3 + 2;
  BrCarClampUnit(pfVar11);
  pfVar1 = pfVar3 + 3;
  BrCarClampUnit(pfVar1);
  if (*pfVar3 + *pfVar10 + *pfVar1 + *pfVar11 == DAT_100770b0) {
    *pfVar3 = 1.0f;
    *pfVar10 = 0.0f;
    *pfVar11 = 0.0f;
    *pfVar1 = 0.0f;
  } else {
    BrVec4Normalise(pfVar3);
  }
  BrCarClampPosXY(pfVar3 + 4);
  BrCarClampPosXY(pfVar3 + 5);
  BrCarClampPosZ(pfVar3 + 6);
  return 1;
}

