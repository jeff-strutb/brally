/* br_ghostpick.c -- matching arm for BrGhostPickBlend (0x10005810).
 *
 * Under the shared network mutex, pick the two freshest remote snapshots for a
 * peer slot, interpolate the local car state between them (weighted by how far
 * the newest snapshot has aged), fold the along-track delta into the result,
 * then renormalise the car's orientation quaternion and wrap its angles.
 */
#ifdef BR_MATCHING_BUILD
#include <windows.h>

int   BrTicks30FromMs(void);
void  BrCarStateLerp(float *pOut, float t, void *pA, void *pB);
float BrVec3Length_100682C0(float *pV);
void  BrQuatNormalise_1006D410(float *pQuat);
void  BrAngleWrap_10005C40(float *pAngle);
void  BrAngleWrap_10005C70(float *pAngle);
void  BrAngleWrap_10005CA0(float *pAngle);

extern HANDLE DAT_10226a64;
extern int    DAT_1007b264;
extern unsigned int DAT_1021ce58;
extern float  DAT_100770b0;

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
  unsigned int *puVar2;
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
  HANDLE local_14;
  HANDLE local_10;
  float local_c;
  float local_8;
  float local_4;

  iVar4 = param_2;
  pfVar3 = param_1;
  local_14 = DAT_10226a64;
  local_10 = (HANDLE)(&DAT_1021ce58)[param_2 * 0x25e];
  puVar2 = &DAT_1021ce58 + param_2 * 0x25e;
  WaitForMultipleObjects(2, &local_14, 1, 0xffffffff);
  if (param_2 != DAT_1007b264) {
    if ((int)((int *)puVar2)[0x156] < 2) {
      param_1[0x1f] = 400.0f;
      ReleaseMutex((HANDLE)*puVar2);
      ReleaseMutex(DAT_10226a64);
      return 0;
    }
    iVar5 = 0;
    iVar9 = 0;
    param_1 = (float *)0x0;
    puVar8 = puVar2 + 3;
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
    puVar8 = puVar2 + 3;
    do {
      if (((puVar8[0xb] != 0) && (iVar5 = param_2, param_1 < (float *)*puVar8)) && (iVar6 != iVar9)) {
        iVar5 = iVar6;
        param_1 = (float *)*puVar8;
        param_2 = iVar6;
      }
      iVar6 = iVar6 + 1;
      puVar8 = puVar8 + 1;
    } while (iVar6 < 8);
    iVar6 = ((int *)puVar2)[3 + iVar9] - ((int *)puVar2)[3 + iVar5];
    if (((int *)puVar2)[0x158] == iVar9) {
      if ((int)((int *)puVar2)[0x15a] < 0xf) {
        ((int *)puVar2)[0x15a] = ((int *)puVar2)[0x15a] + 1;
        ((int *)puVar2)[0x159] = ((int *)puVar2)[0x159] + 1;
      }
      if (iVar6 == 0) {
        pfVar10 = (float *)(puVar2 + ((int *)puVar2)[0x158] * 0x28 + 0x16);
        pfVar11 = pfVar3;
        for (iVar9 = 0x28; iVar9 != 0; iVar9 = iVar9 + -1) {
          *pfVar11 = *pfVar10;
          pfVar10 = pfVar10 + 1;
          pfVar11 = pfVar11 + 1;
        }
        goto LAB_done;
      }
      iVar7 = BrTicks30FromMs();
      iVar7 = iVar7 - ((int *)puVar2)[3 + iVar9];
      if (6 < iVar7) {
        iVar7 = 6;
      }
      BrCarStateLerp(pfVar3, (float)(iVar7 + iVar6) / (float)iVar6, puVar2 + iVar5 * 0x28 + 0x16,
                     puVar2 + iVar9 * 0x28 + 0x16);
      local_c = (float)puVar2[iVar9 * 0x28 + 0x1a];
      local_8 = (float)puVar2[iVar9 * 0x28 + 0x1b];
      local_4 = (float)puVar2[iVar9 * 0x28 + 0x1c];
      fVar12 = BrVec3Length_100682C0(&local_c);
      local_c = pfVar3[4];
      local_8 = pfVar3[5];
      local_4 = pfVar3[6];
      fVar13 = BrVec3Length_100682C0(&local_c);
    } else {
      ((int *)puVar2)[0x158] = iVar9;
      if ((((unsigned int)((int *)puVar2)[3 + iVar9] < (unsigned int)(((int *)puVar2)[0x159] + 1)) &&
           ((int)((int *)puVar2)[0x15b] < 0x14)) && (iVar6 != 0)) {
        ((int *)puVar2)[0x15a] = 1;
        ((int *)puVar2)[0x15b] = ((int *)puVar2)[0x15b] + 1;
        ((int *)puVar2)[0x159] = ((int *)puVar2)[0x159] + 1;
        iVar7 = BrTicks30FromMs();
        iVar7 = iVar7 - ((int *)puVar2)[3 + iVar9];
        if (6 < iVar7) {
          iVar7 = 6;
        }
        BrCarStateLerp(pfVar3, (float)(iVar7 + iVar6) / (float)iVar6, puVar2 + iVar5 * 0x28 + 0x16,
                       puVar2 + iVar9 * 0x28 + 0x16);
        local_c = (float)puVar2[iVar9 * 0x28 + 0x1a];
        local_8 = (float)puVar2[iVar9 * 0x28 + 0x1b];
        local_4 = (float)puVar2[iVar9 * 0x28 + 0x1c];
        fVar12 = BrVec3Length_100682C0(&local_c);
        local_c = pfVar3[4];
        local_8 = pfVar3[5];
        local_4 = pfVar3[6];
        fVar13 = BrVec3Length_100682C0(&local_c);
      } else {
        ((int *)puVar2)[0x15a] = 0;
        ((int *)puVar2)[0x15b] = 0;
        ((int *)puVar2)[0x159] = ((int *)puVar2)[3 + iVar9];
        iVar7 = BrTicks30FromMs();
        iVar7 = iVar7 - ((int *)puVar2)[3 + iVar9];
        if (6 < iVar7) {
          iVar7 = 6;
        }
        BrCarStateLerp(pfVar3, (float)(iVar7 + iVar6) / (float)iVar6, puVar2 + iVar5 * 0x28 + 0x16,
                       puVar2 + iVar9 * 0x28 + 0x16);
        local_c = (float)puVar2[iVar9 * 0x28 + 0x1a];
        local_8 = (float)puVar2[iVar9 * 0x28 + 0x1b];
        local_4 = (float)puVar2[iVar9 * 0x28 + 0x1c];
        fVar12 = BrVec3Length_100682C0(&local_c);
        local_c = pfVar3[4];
        local_8 = pfVar3[5];
        local_4 = pfVar3[6];
        fVar13 = BrVec3Length_100682C0(&local_c);
      }
    }
    pfVar3[6] = (fVar12 - fVar13) + pfVar3[6];
  }
LAB_done:
  ReleaseMutex((HANDLE)*puVar2);
  ReleaseMutex(DAT_10226a64);
  BrAngleWrap_10005C40(pfVar3);
  pfVar10 = pfVar3 + 1;
  BrAngleWrap_10005C40(pfVar10);
  pfVar11 = pfVar3 + 2;
  BrAngleWrap_10005C40(pfVar11);
  pfVar1 = pfVar3 + 3;
  BrAngleWrap_10005C40(pfVar1);
  if (*pfVar3 + *pfVar10 + *pfVar1 + *pfVar11 == DAT_100770b0) {
    *pfVar3 = 1.0f;
    *pfVar10 = 0.0f;
    *pfVar11 = 0.0f;
    *pfVar1 = 0.0f;
  } else {
    BrQuatNormalise_1006D410(pfVar3);
  }
  BrAngleWrap_10005C70(pfVar3 + 4);
  BrAngleWrap_10005C70(pfVar3 + 5);
  BrAngleWrap_10005CA0(pfVar3 + 6);
  return 1;
}

#endif /* BR_MATCHING_BUILD */
