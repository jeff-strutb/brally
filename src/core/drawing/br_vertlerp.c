/* br_vertlerp.c -- drawing.
 *
 * Pop a clip/vertex record off the free list at 0x102E16B4 and fill its
 * 8 floats by interpolating two source vertices; and the corner-slot
 * update that keeps the nearest projected point (0x1000E150).
 *
 * BYTE-EXACT 2026-09-23.  The "epilogue schedule" residue was a missing
 * return: the original keeps the node in eax to the end because the clipper
 * (BrPolyClipPlane) uses it.  Returning it reproduces the interleave.
 */

#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#endif

extern void *DAT_102e16b4;

/* WHAT IT DOES: take one vertex record from the display-list free list and
 * fill it with the 8-float blend of two source vertices (position, tex, and
 * extra channels) at fraction t, and return the record. The free-list pop is
 * skipped when the list is empty; the writes still go through that pointer. */
/* @implements 0x1000E060 glide BrVertLerp8 */
void *BrVertLerp8(void *pA, void *pB, float t)
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
  /* the node stays in eax to the end and the clipper uses it (live
   * oracle, software-clipped shadows) */
  return pNode;
}

#ifdef BR_MATCHING_BUILD
/* One 32-byte corner slot: position, 2D key, three unused floats. */
typedef struct BrScrSlot {
  float p[3];
  float key[2];
  float pad[3];
} BrScrSlot;

/* WHAT IT DOES: offers a candidate point for one corner slot of a screen-space
 * quad. The candidate replaces the point already in slot idx only if its 2D
 * key (+0x0C/+0x10) is strictly nearer to (cx, cy) than the incumbent's, and
 * only if its transformed depth is no further than the reference depth plus
 * one; the stored point is the candidate's position run through the 4x4
 * (row-vector convention), its 2D key is copied across, and the slot is
 * flagged as filled. */
/* Transcribed from the Glide bytes: 32-byte slots, the distance comparison
 * is fcompp/test ah,0x41 (a tie keeps the incumbent, NaN rejects), the depth
 * guard fcomp/test ah,1 against pRef+0x38 minus -1.0 (0x10077210). */
/* @implements 0x1000E150 glide BrScrPtKeepNearest */
void BrScrPtKeepNearest(const float *pM, BrScrSlot *aOut, int *aFlags, int idx,
                        const BrScrSlot *pIn, float cx, float cy,
                        const float *pRef)
{
  float dx1, dy1, dx2, dy2;
  float oz;

  dx1 = pIn->key[0] - cx;
  dy1 = pIn->key[1] - cy;
  dx2 = aOut[idx].key[0] - cx;
  dy2 = aOut[idx].key[1] - cy;
  if (!(dx1 * dx1 + dy1 * dy1 < dx2 * dx2 + dy2 * dy2))
    return;

  oz = (pM[10] * pIn->p[2] + pM[6] * pIn->p[1]) + pM[2] * pIn->p[0] + pM[14];
  if (pRef[14] - -1.0f < oz)
    return;

  aOut[idx].p[0] = (pM[8] * pIn->p[2] + pM[4] * pIn->p[1]) + pM[0] * pIn->p[0] + pM[12];
  aOut[idx].p[1] = (pM[9] * pIn->p[2] + pM[5] * pIn->p[1]) + pM[1] * pIn->p[0] + pM[13];
  aOut[idx].p[2] = oz;
  aOut[idx].key[0] = pIn->key[0];
  aOut[idx].key[1] = pIn->key[1];
  aFlags[idx] = 1;
}
#endif /* BR_MATCHING_BUILD */
