/* br_carstep.c -- matching arm for BrCarStep (0x1006F170).
 *
 * Per-car frame step, called once per car by the race step and the AI.  On the
 * live path it casts the wheel collision ray, applies the control input, then
 * writes the four wheels' suspension force / slip fields; on the replay path
 * (pCar->f7c set) it drives the entity straight from the recorded transform.
 * Either way it then hands off to the net/steer sub-steps, recomputes the
 * scalar speed, and -- for the local camera car -- advances the chase camera.
 */
#include "br_race.h"   /* br_globals: its objects */
#include "br_match.h"      /* BR_THISCALL1 -- thiscall via __fastcall on VC5 */
#include "slice3_41.h"

/* callees */
/* BrCollRayCast_1006EC30: prototype in br_funcs.h */
/* BrCtlInputApply: prototype in br_funcs.h */
/* BrCarSub1005A7A0: prototype in br_funcs.h */
/* BrCarWheelSteerStep_1005ACE0: prototype in br_funcs.h */
/* BrCarNetSendState: prototype in br_funcs.h */
/* BrEntSetPos: prototype in br_funcs.h */
/* BrEntSetVel: prototype in br_funcs.h */
/* BrEntSetAngVel: prototype in br_funcs.h */
/* BrEntSetOrientation: prototype in br_funcs.h */
/* BrCamChaseStep: prototype in br_funcs.h */
/* 64-bit core: declared once, by its definition's header */
/* 64-bit core: declared once, by its definition's header */
/* 64-bit core: declared once, by its definition's header */
/* 0x1002F640 -- thiscall (`this` in ecx = the latch at *(pCar+0x29C0), edx
 * unread, one stack arg, `ret 4`); same arm as br_ctlinput.c:21. */
/* BrBitLatchTake: prototype in br_funcs.h */
/* BrSqrtF: prototype in br_funcs.h */

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
/* BrCtlHuman: prototype in br_funcs.h */

/* WHAT IT DOES: step one car for the frame -- cast the wheel collision ray and
 * apply control input (live) or replay the recorded transform, write the four
 * wheels' suspension/slip fields from the body slip and spin, run the net and
 * steer sub-steps, recompute the scalar speed, and advance the chase camera for
 * the entry that owns it. */
/* RESIDUE (2026-09-20): bytes 1289/1295, insn gap 4, 10 regions.  Transcribed
 * from the disassembly (this function had only a Ghidra draft, no project
 * code).  The remainder is compiler colouring, read out of the diff:
 *   - the wheel-mode selector byte compiles as a running compare that reuses
 *     the zero register (sub eax,ebp / dec eax) where our C emits cmp/cmp;
 *   - the speed magnitude's vx*vx + vz*vz reassociates (fld [1f0] vs [1e8]) --
 *     commutative x87 operand order;
 *   - the chase-cam id-table loop is peeled and the two return sites keep
 *     separate epilogues (add esp,0x14 twice) where ours share one -- a -6 byte
 *     tail difference and a jl-vs-jle bound test;
 *   - __fastcall ecx-load scheduling (mov ecx before vs after the arg pushes).
 * A5 oracle EQUIVALENT on 64 seeds (oracle_profiles _cs_bss/_cs_buf): the live
 * path's four-wheel slip/spin distribution across all three mode arms, the slip
 * threshold and sign flip, pCar->1020/1030, and the sub-call sequence are all
 * verified (negative controls on each fire DIFF); the replay path forwards
 * recorded values to the entity setters, which are black-boxed and certified in
 * their own modules.  No source lever moved the byte count off 1289 across 11
 * spellings and 5 opt levels. */
/* @t4-pass 0x1006F170 1 2026-09-20 probes 11 bytes 1289 insns 319 regions 10 rows 30 census no  (fn.py: loop-bound spelling, compare operand order (e68/e6c), puVar1 decl split, bool test, zero-cast args, O2/O2y/O2p/O1/Ox.  Best -6 bytes/-4 insns at O2; none reached 0.  Residue is the zero-register mode compare + x87 reassociation + peeled tail with duplicated epilogue.) */
/* @t4-pass 0x1006F170 2 2026-09-20 probes 11 bytes 1289 insns 319 regions 10 rows 30 census yes  (census of unpaired multiset: MISSING sub R,R / dec R (mode-byte compare reusing the ebp zero) pair with EXTRA cmp R,R / cmp R,1; MISSING fld [M+0x1f0] + EXTRA fld [M+0x1e8] = commutative x87 operand order in the speed sum; MISSING add esp,0x14 + jl vs EXTRA jle = the duplicated tail epilogue and loop-bound polarity.  Every divergent row is register allocation, x87 reassociation or branch/epilogue shape of identical logic.  A5 oracle EQUIVALENT on 64 seeds.) */
/* @t3 0x1006F170 2026-09-20 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 1294/1295 insns 320/323 rows 11+14 regions 10 oracle EQUIVALENT
 * @t3-effort passes 2 zero-movement 1 2
 * RE-OPENED 2026-09-21: the cert missed BrBitLatchTake's ABI -- 0x1002F640
 * is thiscall (`this` = *(pCar+0x29C0) in ecx, `ret 4`), and the cdecl
 * 1-arg call here dropped the ecx load AND double-popped the stack
 * (add esp,4 after the callee's ret 4).  The 2026-09-20 EQUIVALENT was
 * silent because no seed set the &0x10 mode bit, so the arm never ran.
 * Fixed with br_ctlinput.c:21's proven __fastcall arm. */
/* @implements 0x1006F170 glide BrCarStep */
void BR_THISCALL1 BrCarStep(BrDriverCar *pCar)
{
  float local_c[3];
  float fVar2;
  int cVar3;
  int iVar5;
  int *piVar6;

  if (pCar->fF7C != 0) {
    BrVec3Scale(local_c, &pCar->fwd.x,
                pCar->aimFwd * g_brRaceFlyStep * DAT_10077c60);
    BrVec3AddTo(&pCar->pos.x, local_c);
    BrEntSetPos(pCar, pCar->pos.x, pCar->pos.y,
                pCar->pos.z);
    BrEntSetVel(pCar, 0.0f, 0.0f, 0.0f);
    BrEntSetAngVel(pCar, 0.0f, 0.0f, 0.0f);
    *(int *)&pCar->f0E24 = 0;
    BrEntSetOrientation(pCar,
                        pCar->f2720 * g_brRaceFlyStep * DAT_10077c64,
                        pCar->f2724 * g_brRaceFlyStep * DAT_10077c64,
                        pCar->f272C * g_brRaceFlyStep * DAT_10077c64);
    if ((**(unsigned char * *)&pCar->pCtl & 0x10) != 0) {
      int *puVar1 = (int *)&pCar->up.x;
      *puVar1 = 0;
      puVar1[1] = 0;
      puVar1[2] = 0x3f800000;
      BrVec3Cross(&pCar->right.x, puVar1, &pCar->fwd.x);
      BrVec3Cross(puVar1, &pCar->fwd.x, &pCar->right.x);
      BrBitLatchTake(pCar->pCtl, 0x10);
    }
  } else {
    *(int *)&pCar->sz100C[20] =
        FUN_1006ec30(&pCar->pos.x, &pCar->up.x, &pCar->pos.x, &pCar->aNearIds[0],
                               &pCar->gotHit, &pCar->aFarIds[0], &pCar->farCount,
                               &pCar->fHitDist, &pCar->iHitFace);
    *(int *)&pCar->aBody[0].rb.child[1]->f1C0 = 0;
    *(int *)&pCar->aBody[0].rb.child[0]->f1C0 = 0;
    BrCtlInputApply(pCar);
    *(int *)&pCar->aBody[0].rb.child[3]->f1C0 = *(int *)&pCar->f0E20;
    *(int *)&pCar->aBody[0].rb.child[2]->f1C0 = *(int *)&pCar->aBody[0].rb.child[3]->f1C0;
    *(int *)&pCar->aBody[0].rb.child[0]->f1CC = 0;
    *(int *)&pCar->aBody[0].rb.child[1]->f1CC = 0;
    *(int *)&pCar->aBody[0].rb.child[2]->f1CC = 0;
    *(int *)&pCar->aBody[0].rb.child[3]->f1CC = 0;
    *(int *)&pCar->aBody[0].rb.child[0]->f1D0 = 0;
    *(int *)&pCar->aBody[0].rb.child[1]->f1D0 = 0;
    *(int *)&pCar->aBody[0].rb.child[2]->f1D0 = 0;
    *(int *)&pCar->aBody[0].rb.child[3]->f1D0 = 0;
    if (pCar->fE68 > DAT_10077c38) {
      if ((**(unsigned int * *)&pCar->pCtl & 0x20000) != 0) {
        pCar->fE68 = -pCar->fE68;
      }
      cVar3 = *(char *)(((char *)pCar->pModel) + 0xd9);
      if (cVar3 == 0) {
        pCar->aBody[0].rb.child[0]->f1CC = pCar->fE68 * DAT_10077c6c;
        pCar->aBody[0].rb.child[1]->f1CC = pCar->fE68 * DAT_10077c6c;
        pCar->aBody[0].rb.child[2]->f1CC = pCar->fE68 * DAT_10077c70;
        pCar->aBody[0].rb.child[3]->f1CC = pCar->fE68 * DAT_10077c70;
      } else if (cVar3 == 1) {
        pCar->aBody[0].rb.child[0]->f1CC = pCar->fE68 * DAT_10077c68;
        pCar->aBody[0].rb.child[1]->f1CC = pCar->fE68 * DAT_10077c68;
        *(int *)&pCar->aBody[0].rb.child[2]->f1CC = 0;
        *(int *)&pCar->aBody[0].rb.child[3]->f1CC = 0;
      } else {
        pCar->aBody[0].rb.child[0]->f1CC = -pCar->fE68;
        pCar->aBody[0].rb.child[1]->f1CC = -pCar->fE68;
        pCar->aBody[0].rb.child[2]->f1CC = -pCar->fE68;
        pCar->aBody[0].rb.child[3]->f1CC = -pCar->fE68;
      }
    }
    if (pCar->f0E6C < DAT_10077c38) {
      pCar->aBody[0].rb.child[0]->f1D0 = -pCar->f0E6C;
      pCar->aBody[0].rb.child[1]->f1D0 = -pCar->f0E6C;
      *(int *)&pCar->aBody[0].rb.child[0]->f1CC = 0;
      *(int *)&pCar->aBody[0].rb.child[1]->f1CC = 0;
      iVar5 = BR_LP64_PTR_AS_INT(pCar->aBody[0].rb.child[2]);
      fVar2 = *(float *)(iVar5 + 0x1cc);
      if (*(float *)(iVar5 + 0x1cc) < DAT_10077c38) {
        fVar2 = -fVar2;
      }
      if (fVar2 < DAT_10077c74) {
        *(float *)(iVar5 + 0x1d0) = -pCar->f0E6C;
        pCar->aBody[0].rb.child[3]->f1D0 = -pCar->f0E6C;
      }
    }
    if (((*(int *)&DAT_105ccb68[8]) == 0) &&
        ((((*(int *)&g_brRaceRules.mode) != 2 || (pCar->f140 != 1)) ||
          (pCar->pCtl->pHdr == 0)))) {
      if ((*(int *)&g_brRaceRules.mode) == 4) {
        if ((pCar->f140 == 0) &&
            (pCar->pCtl->pHdr != 0))
          goto LAB_net;
      } else if ((((*(int *)&g_brRaceRules.mode) != 5) && ((*(int *)&g_brRaceTick) == 0)) &&
                 (0x5a < *(int *)&pCar->aBody[2].rb.f1B4))
        goto LAB_net;
      BrCarPhysStep(pCar);
    }
  }
LAB_net:
  if ((*(int *)&g_brRaceNet) == 0) {
    if ((*(int *)&DAT_105ccb68[8]) == 0) {
      BrCarWheelSteerStep_1005ACE0(pCar);
    }
  } else if ((*(void * *)&pCar->pfnControl == (void *)BrCtlHuman) && ((*(int *)&DAT_105ccb68[8]) == 0)) {
    BrCarNetSendState(pCar);
  }
  if (*(int *)&pCar->aBody[2].rb.f1B4 != 0) {
    pCar->f1030 =
        BrSqrtF(pCar->aBody[0].rb.st.vel.z * pCar->aBody[0].rb.st.vel.z +
                pCar->aBody[0].rb.st.vel.y * pCar->aBody[0].rb.st.vel.y +
                pCar->aBody[0].rb.st.vel.x * pCar->aBody[0].rb.st.vel.x) * DAT_10077c78;
  }
  if (((*(int *)&g_brRaceNet) == 0) && (iVar5 = 0, 0 < g_brMode0AA8B4)) {
    piVar6 = &g_aBrView[0].iCar;
    while (pCar->f140 != *piVar6) {
      iVar5 = iVar5 + 1;
      piVar6 = &g_aBrView[iVar5].iCar;
      if (g_brMode0AA8B4 <= iVar5) {
        return;
      }
    }
    BrCamChaseStep(pCar);
  }
  return;
}

