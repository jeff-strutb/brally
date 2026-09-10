/* br_vertlerp.c -- drawing.
 *
 * Pop a clip/vertex record off the free list at 0x102E16B4 and fill its
 * 8 floats by interpolating two source vertices.
 *
 * @t4-pass 0x1000E060 1 2026-09-08 probes 2 bytes 231 insns 79 regions 1 rows 0 census no
 *
 * PARKED T2. Instruction multiset matches (REGNORM 0+0, 79/79). Residue is
 * epilogue schedule: orig pops edi in the middle of channel 6 and fstp
 * before reloading ecx/edx for channel 7; we pop esi at +0xc8 where orig
 * fstps. Same ops, pop/fstp order. Do not permute (colouring).
 */

#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#endif

extern void *DAT_102e16b4;

/* WHAT IT DOES: take one vertex record from the display-list free list and
 * fill it with the 8-float blend of two source vertices (position, tex, and
 * extra channels) at fraction t. The free-list pop is skipped when the list
 * is empty; the writes still go through that pointer. */
/* @t4-pass 0x1000E060 2 2026-09-09 probes 10 bytes 231 insns 79 regions 1 rows 0 census no  (hand, fn.py variants: channel shapes, load orders, sum orders, head-store forms, all inert) */
/* @t4-pass 0x1000E060 3 2026-09-09 probes 10 bytes 231 insns 79 regions 1 rows 0 census yes  (hand, fn.py variants: declaration orders, reload placement, difference temp, all inert; corpus MISS at +0xc8 len 12 -- the pop/fstp epilogue interleave is proven nowhere) */
/* @t3 0x1000E060 2026-09-09 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 231/231 insns 79/79 rows 0+0 regions 1 oracle UNCLASSIFIED
 * @t3-effort passes 3 zero-movement 2 3
 * residue is epilogue scheduling only: orig interleaves the callee-saved
 * pops with the last channel's fstp, ours pops after (identical multiset,
 * REGNORM 0+0, size-exact); see the PARKED T2 note in the file header.
 * Do not reopen before the end-grind (CLAUDE.md rule 12). */
/* @implements 0x1000E060 glide BrVertLerp8 */
void BrVertLerp8(void *pA, void *pB, float t)
{
  float *pNode;
  float *pDst;
  float *a;
  float *b;

  pNode = DAT_102e16b4;
  if (pNode != 0)
    DAT_102e16b4 = *(void **)pNode;
  pDst = pNode + 2;
  *(float **)((char *)pNode + 4) = pDst;

  b = *(float **)((char *)pB + 4);
  a = *(float **)((char *)pA + 4);
  pDst[0] = (b[0] - a[0]) * t + a[0];

  b = *(float **)((char *)pB + 4);
  a = *(float **)((char *)pA + 4);
  pDst = *(float **)((char *)pNode + 4);
  pDst[1] = (b[1] - a[1]) * t + a[1];

  b = *(float **)((char *)pB + 4);
  a = *(float **)((char *)pA + 4);
  pDst = *(float **)((char *)pNode + 4);
  pDst[2] = (b[2] - a[2]) * t + a[2];

  b = *(float **)((char *)pB + 4);
  a = *(float **)((char *)pA + 4);
  pDst = *(float **)((char *)pNode + 4);
  pDst[3] = (b[3] - a[3]) * t + a[3];

  b = *(float **)((char *)pB + 4);
  a = *(float **)((char *)pA + 4);
  pDst = *(float **)((char *)pNode + 4);
  pDst[4] = (b[4] - a[4]) * t + a[4];

  b = *(float **)((char *)pB + 4);
  a = *(float **)((char *)pA + 4);
  pDst = *(float **)((char *)pNode + 4);
  pDst[5] = (b[5] - a[5]) * t + a[5];

  b = *(float **)((char *)pB + 4);
  a = *(float **)((char *)pA + 4);
  pDst = *(float **)((char *)pNode + 4);
  pDst[6] = (b[6] - a[6]) * t + a[6];

  pB = *(void **)((char *)pB + 4);
  pA = *(void **)((char *)pA + 4);
  pDst = *(float **)((char *)pNode + 4);
  pDst[7] = (((float *)pB)[7] - ((float *)pA)[7]) * t + ((float *)pA)[7];
}
