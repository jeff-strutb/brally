/* br_carstep.c -- matching arm for BrCarStep (0x1006F170).
 *
 * Per-car frame step, called once per car by the race step and the AI.  On the
 * live path it casts the wheel collision ray, applies the control input, then
 * writes the four wheels' suspension force / slip fields; on the replay path
 * (pCar->f7c set) it drives the entity straight from the recorded transform.
 * Either way it then hands off to the net/steer sub-steps, recomputes the
 * scalar speed, and -- for the local camera car -- advances the chase camera.
 */
#ifdef BR_MATCHING_BUILD
#include "br_match.h"      /* BR_THISCALL1 -- thiscall via __fastcall on VC5 */

/* callees */
int   BrCollRayCast_1006EC30(void *a, void *b, void *c, void *d, void *e,
                             void *f, void *g, void *h, void *i);
void  BR_THISCALL1 BrCtlInputApply(unsigned char *pCar);
void  BR_THISCALL1 BrCarSub1005A7A0(unsigned char *pCar);
void  BR_THISCALL1 BrCarWheelSteerStep_1005ACE0(unsigned char *pCar);
void  BrCarNetSendState(unsigned char *pCar);
void  BR_THISCALL1 BrEntSetPos(unsigned char *pCar, float x, float y, float z);
void  BR_THISCALL1 BrEntSetVel(unsigned char *pCar, float x, float y, float z);
void  BR_THISCALL1 BrEntSetAngVel(unsigned char *pCar, float x, float y, float z);
void  BR_THISCALL1 BrEntSetOrientation(unsigned char *pCar, float x, float y, float z);
void  BR_THISCALL1 BrCamChaseStep(unsigned char *pCar);
void  BrVec3Scale(float *pOut, void *pV, float s);
void  BrVec3AddTo(void *pDst, float *pSrc);
void  BrVec3Cross(void *pOut, void *pA, void *pB);
/* 0x1002F640 -- thiscall (`this` in ecx = the latch at *(pCar+0x29C0), edx
 * unread, one stack arg, `ret 4`); same arm as br_ctlinput.c:21. */
void  __fastcall BrBitLatchTake(void *pThis, void *_edx, unsigned mask);
float BrSqrtF(float x);

extern float DAT_106e9d8c;
extern float DAT_10077c60;
extern float DAT_10077c64;
extern float DAT_10077c68;
extern float DAT_10077c6c;
extern float DAT_10077c70;
extern float DAT_10077c74;
extern float DAT_10077c78;
extern float DAT_10077c38;
extern int   DAT_105ccb88;
extern int   DAT_100a9360;
extern int   DAT_10226a44;
extern int   DAT_10226a48;
extern int   DAT_100aa044;
extern int   DAT_106e86c8;
extern void  BrCtlHuman(void);   /* 0x1005D050 -- compared as a function pointer */

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
void BR_THISCALL1 BrCarStep(unsigned char *pCar)
{
  float local_c[3];
  float fVar2;
  int cVar3;
  int iVar5;
  int *piVar6;

  if (*(int *)(pCar + 0xf7c) != 0) {
    BrVec3Scale(local_c, pCar,
                *(float *)(pCar + 0x2728) * DAT_106e9d8c * DAT_10077c60);
    BrVec3AddTo(pCar + 0x30, local_c);
    BrEntSetPos(pCar, *(float *)(pCar + 0x30), *(float *)(pCar + 0x34),
                *(float *)(pCar + 0x38));
    BrEntSetVel(pCar, 0.0f, 0.0f, 0.0f);
    BrEntSetAngVel(pCar, 0.0f, 0.0f, 0.0f);
    *(int *)(pCar + 0xe24) = 0;
    BrEntSetOrientation(pCar,
                        *(float *)(pCar + 0x2720) * DAT_106e9d8c * DAT_10077c64,
                        *(float *)(pCar + 0x2724) * DAT_106e9d8c * DAT_10077c64,
                        *(float *)(pCar + 0x272c) * DAT_106e9d8c * DAT_10077c64);
    if ((**(unsigned char **)(pCar + 0x29c0) & 0x10) != 0) {
      int *puVar1 = (int *)(pCar + 0x20);
      *puVar1 = 0;
      puVar1[1] = 0;
      puVar1[2] = 0x3f800000;
      BrVec3Cross(pCar + 0x10, puVar1, pCar);
      BrVec3Cross(puVar1, pCar, pCar + 0x10);
      BrBitLatchTake(*(void **)(pCar + 0x29c0), 0, 0x10);
    }
  } else {
    *(int *)(pCar + 0x1020) =
        BrCollRayCast_1006EC30(pCar + 0x30, pCar + 0x20, pCar + 0x30, pCar + 0x290c,
                               pCar + 0x294c, pCar + 0x2950, pCar + 0x2990,
                               pCar + 0x2994, pCar + 0x2998);
    *(int *)(*(int *)(pCar + 0x16c) + 0x1c0) = 0;
    *(int *)(*(int *)(pCar + 0x168) + 0x1c0) = 0;
    BrCtlInputApply(pCar);
    *(int *)(*(int *)(pCar + 0x174) + 0x1c0) = *(int *)(pCar + 0xe20);
    *(int *)(*(int *)(pCar + 0x170) + 0x1c0) = *(int *)(*(int *)(pCar + 0x174) + 0x1c0);
    *(int *)(*(int *)(pCar + 0x168) + 0x1cc) = 0;
    *(int *)(*(int *)(pCar + 0x16c) + 0x1cc) = 0;
    *(int *)(*(int *)(pCar + 0x170) + 0x1cc) = 0;
    *(int *)(*(int *)(pCar + 0x174) + 0x1cc) = 0;
    *(int *)(*(int *)(pCar + 0x168) + 0x1d0) = 0;
    *(int *)(*(int *)(pCar + 0x16c) + 0x1d0) = 0;
    *(int *)(*(int *)(pCar + 0x170) + 0x1d0) = 0;
    *(int *)(*(int *)(pCar + 0x174) + 0x1d0) = 0;
    if (*(float *)(pCar + 0xe68) > DAT_10077c38) {
      if ((**(unsigned int **)(pCar + 0x29c0) & 0x20000) != 0) {
        *(float *)(pCar + 0xe68) = -*(float *)(pCar + 0xe68);
      }
      cVar3 = *(char *)(*(int *)(pCar + 0x29c4) + 0xd9);
      if (cVar3 == 0) {
        *(float *)(*(int *)(pCar + 0x168) + 0x1cc) = *(float *)(pCar + 0xe68) * DAT_10077c6c;
        *(float *)(*(int *)(pCar + 0x16c) + 0x1cc) = *(float *)(pCar + 0xe68) * DAT_10077c6c;
        *(float *)(*(int *)(pCar + 0x170) + 0x1cc) = *(float *)(pCar + 0xe68) * DAT_10077c70;
        *(float *)(*(int *)(pCar + 0x174) + 0x1cc) = *(float *)(pCar + 0xe68) * DAT_10077c70;
      } else if (cVar3 == 1) {
        *(float *)(*(int *)(pCar + 0x168) + 0x1cc) = *(float *)(pCar + 0xe68) * DAT_10077c68;
        *(float *)(*(int *)(pCar + 0x16c) + 0x1cc) = *(float *)(pCar + 0xe68) * DAT_10077c68;
        *(int *)(*(int *)(pCar + 0x170) + 0x1cc) = 0;
        *(int *)(*(int *)(pCar + 0x174) + 0x1cc) = 0;
      } else {
        *(float *)(*(int *)(pCar + 0x168) + 0x1cc) = -*(float *)(pCar + 0xe68);
        *(float *)(*(int *)(pCar + 0x16c) + 0x1cc) = -*(float *)(pCar + 0xe68);
        *(float *)(*(int *)(pCar + 0x170) + 0x1cc) = -*(float *)(pCar + 0xe68);
        *(float *)(*(int *)(pCar + 0x174) + 0x1cc) = -*(float *)(pCar + 0xe68);
      }
    }
    if (*(float *)(pCar + 0xe6c) < DAT_10077c38) {
      *(float *)(*(int *)(pCar + 0x168) + 0x1d0) = -*(float *)(pCar + 0xe6c);
      *(float *)(*(int *)(pCar + 0x16c) + 0x1d0) = -*(float *)(pCar + 0xe6c);
      *(int *)(*(int *)(pCar + 0x168) + 0x1cc) = 0;
      *(int *)(*(int *)(pCar + 0x16c) + 0x1cc) = 0;
      iVar5 = *(int *)(pCar + 0x170);
      fVar2 = *(float *)(iVar5 + 0x1cc);
      if (*(float *)(iVar5 + 0x1cc) < DAT_10077c38) {
        fVar2 = -fVar2;
      }
      if (fVar2 < DAT_10077c74) {
        *(float *)(iVar5 + 0x1d0) = -*(float *)(pCar + 0xe6c);
        *(float *)(*(int *)(pCar + 0x174) + 0x1d0) = -*(float *)(pCar + 0xe6c);
      }
    }
    if ((DAT_105ccb88 == 0) &&
        (((DAT_100a9360 != 2 || (*(int *)(pCar + 0x140) != 1)) ||
          (*(int *)(*(int *)(pCar + 0x29c0) + 0x44) == 0)))) {
      if (DAT_100a9360 == 4) {
        if ((*(int *)(pCar + 0x140) == 0) &&
            (*(int *)(*(int *)(pCar + 0x29c0) + 0x44) != 0))
          goto LAB_net;
      } else if (((DAT_100a9360 != 5) && (DAT_10226a44 == 0)) &&
                 (0x5a < *(int *)(pCar + 0x730)))
        goto LAB_net;
      BrCarSub1005A7A0(pCar);
    }
  }
LAB_net:
  if (DAT_10226a48 == 0) {
    if (DAT_105ccb88 == 0) {
      BrCarWheelSteerStep_1005ACE0(pCar);
    }
  } else if ((*(void **)(pCar + 0xf08) == (void *)BrCtlHuman) && (DAT_105ccb88 == 0)) {
    BrCarNetSendState(pCar);
  }
  if (*(int *)(pCar + 0x730) != 0) {
    *(float *)(pCar + 0x1030) =
        BrSqrtF(*(float *)(pCar + 0x1f0) * *(float *)(pCar + 0x1f0) +
                *(float *)(pCar + 0x1ec) * *(float *)(pCar + 0x1ec) +
                *(float *)(pCar + 0x1e8) * *(float *)(pCar + 0x1e8)) * DAT_10077c78;
  }
  if ((DAT_10226a48 == 0) && (iVar5 = 0, 0 < DAT_100aa044)) {
    piVar6 = &DAT_106e86c8;
    while (*(int *)(pCar + 0x140) != *piVar6) {
      iVar5 = iVar5 + 1;
      piVar6 = piVar6 + 0x16;
      if (DAT_100aa044 <= iVar5) {
        return;
      }
    }
    BrCamChaseStep(pCar);
  }
  return;
}

#endif /* BR_MATCHING_BUILD */
