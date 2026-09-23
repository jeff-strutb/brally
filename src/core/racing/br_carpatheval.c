/* br_carpatheval.c -- matching arm for BrCarPathEval (0x1005D3C0).
 *
 * Evaluate the car's smooth track-path frame for the current segment: build the
 * side/tangent vectors from the segment's control points, find the along-segment
 * parameter from the car's position, and from its cubic powers lay down the
 * blended position, tangent and normal frame (car+0xF18..+0xF58).
 */
#ifdef BR_MATCHING_BUILD
#include "br_match.h"      /* BR_THISCALL1 -- thiscall via __fastcall on VC5 */

void  BrVec3Sub(float *pOut, const void *pA, const void *pB);
void  BrVec3Midpoint(float *pOut, const void *pA, const void *pB);
float BrVec3Dot(const void *pA, const void *pB);
void  BrVec3Direction(float *pOut, const void *pFrom, const void *pTo);
float BrVec3Dist(const void *pA, const void *pB);
void  BrVec3ScaleBy(float *pV, float s);
void  BrVec3Scale(float *pOut, const void *pV, float s);
void  BrVec3MulAddTo(float *pA, const void *pB, float s);
void  BrVec3Lerp(float *pOut, const void *pA, const void *pB, float t);
void  BrVec3Cross(float *pOut, const void *pA, const void *pB);
void  br_dl_normalise(float *pV);

extern float DAT_100778d8;
extern float DAT_100778cc;
extern float DAT_100778f8;
extern float DAT_100778f4;
extern float DAT_1007789c;

/* WHAT IT DOES: evaluate the car's path frame for its current track segment --
 * build the segment side and tangent vectors from the control points, solve the
 * along-segment parameter from the car position, and blend the position,
 * tangent and normal frame (and the signed off-path distance) from the cubic
 * powers of that parameter into car+0xF18..+0xF58. */
/* RESIDUE (2026-09-20): bytes 938/937, insn gap 2, 7 masked regions.  Hand
 * transcription (this function had only a Ghidra draft, no project code); it
 * came within a single byte on the first faithful pass.  The remainder is
 * colouring, read out of the diff:
 *   - the two segment scalars (seg+0x64, seg+0x8c) load direct into the FPU in
 *     the original (fld/fsub from segment memory) where ours holds them in
 *     locals and subtracts from a stack copy;
 *   - the final off-path-distance abs: fchs + fcomp-mem (test ah,1) vs our
 *     fld-const + fcomp-st (test ah,0x41) -- x87 compare operand order.
 * A5 oracle EQUIV-MODULO-FP on 48 seeds (oracle_profiles _pe_bss/_pe_buf): the
 * cubic path blend, the perpendicular side vector, the segment scale, and the
 * final frame dot are all verified -- negative controls on each fire DIFF; the
 * BrVec3 leaf helpers all run (pure vector math on the seeded path).  The only
 * residual disagreement is x87 64-vs-80-bit intermediate rounding.  No source
 * lever moved the byte count off 938 across 10 spellings and 4 opt levels. */
/* @t4-pass 0x1005D3C0 1 2026-09-20 probes 10 bytes 938 insns 303 regions 7 rows 26 census no  (fn.py: compare operand order, sub/mul commutes, 2x and 0 literal forms, neg-add regroup, O2/O2y/O2p/Ox.  Best +1 byte/+2 insn at O2; none reached 0.  Residue is the segment-scalar load timing + final-abs x87 compare order.) */
/* @t4-pass 0x1005D3C0 2 2026-09-20 probes 10 bytes 938 insns 303 regions 7 rows 26 census yes  (census of unpaired multiset: MISSING fld [seg+0x64] + fsub [seg+0x8c] vs EXTRA mov-to-reg + fsub-reg = the segment-scalar difference computed direct-in-FPU vs via locals; MISSING fchs + fcomp-mem (test ah,1) vs EXTRA fld-const + fcomp-st (test ah,0x41) = x87 operand order on the final abs compare.  Every divergent row is x87 scheduling / operand order of identical logic.  A5 oracle EQUIV-MODULO-FP on 48 seeds.) */
/* @t3 0x1005D3C0 2026-09-20 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 938/937 insns 303/301 rows 12+14 regions 7 oracle EQUIV-MODULO-FP
 * @t3-effort passes 2 zero-movement 1 2
 */
/* @implements 0x1005D3C0 glide BrCarPathEval */
void BR_THISCALL1 BrCarPathEval(unsigned char *pCar)
{
  int param_1 = (int)pCar;
  int iVar1;
  float fVar2;
  float fVar3;
  unsigned char bVar4;
  int iVar5;
  int *piVar6;
  float fVar7;
  float fVar8;
  int iVar9;
  int *piVar10;
  float fVar11;
  float fVar12;
  float fVar13;
  unsigned int local_64;
  float a54[3];
  float a48[3];
  float a3c[3];
  float l30[3];
  float l24[3];
  float l18[3];
  float lc[3];

  iVar5 = *(int *)(param_1 + 0xf90);
  iVar9 = *(int *)(param_1 + 0xf8c);
  fVar2 = *(float *)(iVar9 + 100 + iVar5 * 0x28);
  fVar3 = *(float *)(iVar9 + 0x8c + iVar5 * 0x28);
  iVar9 = iVar9 + iVar5 * 0x28;
  BrVec3Sub(a48, (void *)(iVar9 + 0x40), (void *)(iVar9 + 0x4c));
  iVar9 = *(int *)(param_1 + 0xf8c) + *(int *)(param_1 + 0xf90) * 0x28;
  BrVec3Sub(a54, (void *)(iVar9 + 0x68), (void *)(iVar9 + 0x74));
  BrVec3Midpoint(a54, a48, a54);
  a48[1] = a54[0];
  a48[0] = -a54[1];
  iVar9 = *(int *)(param_1 + 0xf8c) + *(int *)(param_1 + 0xf90) * 0x28;
  a48[2] = 0.0f;
  BrVec3Midpoint(a54, (void *)(iVar9 + 0x4c), (void *)(iVar9 + 0x74));
  BrVec3Sub(a54, (void *)(param_1 + 0x30), a54);
  fVar11 = BrVec3Dot(a48, a54);
  piVar6 = *(int **)(param_1 + 0xf8c);
  piVar10 = piVar6;
  if (DAT_100778d8 <= fVar11) {
    local_64 = *(unsigned int *)(param_1 + 0xf90);
    iVar9 = local_64 - 1;
  } else {
    iVar9 = *(int *)(param_1 + 0xf90);
    local_64 = iVar9 + 1;
    if (local_64 == *(unsigned short *)(piVar6 + 5)) {
      piVar10 = (int *)*piVar6;
      bVar4 = *(unsigned char *)((int)piVar10 + 0x16);
      while ((bVar4 & 1) != 0) {
        piVar10 = (int *)piVar10[1];
        bVar4 = *(unsigned char *)((int)piVar10 + 0x16);
      }
      local_64 = 0;
    }
  }
  BrVec3Direction(a48, piVar6 + iVar9 * 10 + 0x13, piVar6 + iVar9 * 10 + 0x1d);
  BrVec3Direction(a54, piVar10 + local_64 * 10 + 0x13, piVar10 + local_64 * 10 + 0x1d);
  fVar2 = (fVar2 - fVar3) * DAT_100778cc;
  BrVec3ScaleBy(a48, fVar2);
  BrVec3ScaleBy(a54, fVar2);
  BrVec3Midpoint(l30, piVar6 + iVar9 * 10 + 0x13, piVar6 + iVar9 * 10 + 0x1d);
  BrVec3Midpoint(l24, piVar10 + local_64 * 10 + 0x13, piVar10 + local_64 * 10 + 0x1d);
  a3c[0] = *(float *)(param_1 + 0x30);
  a3c[1] = *(float *)(param_1 + 0x34);
  a3c[2] = 0.0f;
  BrVec3Sub(l18, a3c, l30);
  BrVec3Sub(lc, a3c, l24);
  fVar11 = BrVec3Dot(l18, a48);
  fVar12 = BrVec3Dot(lc, a54);
  fVar13 = BrVec3Dist(l30, l24);
  iVar9 = param_1 + 0xf18;
  /* The original's rounding points (live oracle, championship): t is
   * computed in the FPU and fst'd; t^2 = t(unrounded) * t(stored) and
   * t^3 = t^2(unrounded) * t(stored), each fst'd on the way; the leading
   * cubic term is stored before use.  The doubles are the x87 registers,
   * the int images force the float stores VC5 would forward away. */
  {
    double t, t2, t3;
    int bits;

    /* the two dots and the segment length arrive as stored floats */
    bits = *(int *)&fVar11;
    fVar11 = *(float *)&bits;
    fVar12 = -fVar12;
    bits = *(int *)&fVar12;
    fVar12 = *(float *)&bits;                        /* -dot, as stored */
    bits = *(int *)&fVar13;
    fVar13 = *(float *)&bits;
    t = (((double)fVar11 * fVar13) / ((double)fVar12 + fVar11)) / fVar13;
    fVar2 = (float)t;
    bits = *(int *)&fVar2;
    fVar2 = *(float *)&bits;                         /* t  as stored */
    t2 = t * fVar2;
    fVar3 = (float)t2;
    bits = *(int *)&fVar3;
    fVar3 = *(float *)&bits;                         /* t^2 as stored */
    t3 = t2 * fVar2;
    fVar7 = (float)t3;
    bits = *(int *)&fVar7;
    fVar7 = *(float *)&bits;                         /* t^3 as stored */
    fVar8 = (float)(t3 * DAT_1007789c);
    bits = *(int *)&fVar8;
    fVar8 = *(float *)&bits;                         /* 2t^3 term as stored */
  }
  BrVec3Scale((float *)iVar9, l30, DAT_100778f8 - (fVar8 - fVar3 * DAT_100778f4));
  BrVec3MulAddTo((float *)iVar9, l24, fVar8 - fVar3 * DAT_100778f4);
  BrVec3MulAddTo((float *)iVar9, a48, (fVar7 - (fVar3 + fVar3)) + fVar2);
  BrVec3MulAddTo((float *)iVar9, a54, fVar7 - fVar3);
  iVar5 = param_1 + 0xf24;
  BrVec3Lerp((float *)iVar5, a54, a48, fVar2);
  br_dl_normalise((float *)iVar5);
  iVar1 = param_1 + 0xf30;
  BrVec3Cross((float *)iVar1, (void *)(param_1 + 0xf3c), (void *)iVar5);
  br_dl_normalise((float *)iVar1);
  BrVec3Cross((void *)(param_1 + 0xf3c), (void *)iVar5, (void *)iVar1);
  BrVec3Sub((float *)(param_1 + 0xf4c), (void *)(param_1 + 0x30), (void *)iVar9);
  fVar12 = BrVec3Dot((void *)iVar1, (void *)(param_1 + 0xf4c));
  *(float *)(param_1 + 0xf58) = fVar12;
  if (fVar12 < DAT_100778d8) {
    fVar12 = -fVar12;
  }
  *(float *)(param_1 + 0xf48) = fVar12;
  return;
}

#endif /* BR_MATCHING_BUILD */
