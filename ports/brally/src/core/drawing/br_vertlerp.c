#include "br_mat.h"   /* br_globals: its objects */
#include "slice2_14.h"   /* br_globals: its objects */
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

#define _CRTIMP __declspec(dllimport)

/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* WHAT IT DOES: take one vertex record from the display-list free list and
 * fill it with the 8-float blend of two source vertices (position, tex, and
 * extra channels) at fraction t, and return the record. The free-list pop is
 * skipped when the list is empty; the writes still go through that pointer. */
/* @implements 0x1000E060 glide BrVertLerp8 */
void *BrVertLerp8(void *pA, void *pB, float t)
{
  /* The node is BrLerpNode (slice2_14.h): the link, the data pointer, eight
   * floats.  The original addresses it at the i386 offsets (+0 / +4 / +8)
   * and re-reads the two source data pointers before every component. */
  BrLerpNode *pNode = (BrLerpNode *)g_pBrLerpFree;
  const BrLerpNode *a = (const BrLerpNode *)pA, *b = (const BrLerpNode *)pB;
  int k;

  if (pNode != 0)
    g_pBrLerpFree = pNode->pNext;
  pNode->pData = pNode->data;
  for (k = 0; k < 8; k++)
    pNode->pData[k] = (b->pData[k] - a->pData[k]) * t + a->pData[k];
  /* the node stays in eax to the end and the clipper uses it (live
   * oracle, software-clipped shadows) */
  return pNode;
}

/* One 32-byte corner slot: position, 2D key, three unused floats. */
typedef struct BrScrSlot {
  float p[3];
  float key[2];
  float pad[3];
} BrScrSlot;

/* Transcribed from the Glide bytes: 32-byte slots, the distance comparison
 * is fcompp/test ah,0x41 (a tie keeps the incumbent, NaN rejects), the depth
 * guard fcomp/test ah,1 against pRef+0x38 minus -1.0 (0x10077210). */
/* WHAT IT DOES: offers a candidate point for one corner slot of a screen-space
 * quad. The candidate replaces the point already in slot idx only if its 2D
 * key (+0x0C/+0x10) is strictly nearer to (cx, cy) than the incumbent's, and
 * only if its transformed depth is no further than the reference depth plus
 * one; the stored point is the candidate's position run through the 4x4
 * (row-vector convention), its 2D key is copied across, and the slot is
 * flagged as filled. */
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


/* 64-bit core: declared once, in br_globals.h or its struct's header */           /* 0x106E78F0 */

/* WHAT IT DOES: takes a point record (x, y, z at +0/+4/+8) and writes its
 * transformed screen x and y (at +0xC and +0x10) through the first two
 * columns of the combined view matrix, translation included.  Only x and y
 * are produced; z/w are left alone. */
/* @implements 0x1000E270 glide BrPointProjectXY */
void BrPointProjectXY(float *v)
{
    /* Declaration order y, z, x and assignment order x, y, z: together they
     * put x in the fresh frame dword, y in the dead parameter slot, and z
     * on the x87 stack, with x copied through ecx first.  Any other pairing
     * measured 4-9 bytes off (all six declaration orders probed). */
    float y;
    float z;
    float x;

    x = v[0];
    y = v[1];
    z = v[2];

    v[3] = x * (*(float (*)[4][4])&g_BrDrawCombined)[0][0] + y * (*(float (*)[4][4])&g_BrDrawCombined)[1][0]
         + z * (*(float (*)[4][4])&g_BrDrawCombined)[2][0] + (*(float (*)[4][4])&g_BrDrawCombined)[3][0];
    v[4] = x * (*(float (*)[4][4])&g_BrDrawCombined)[0][1] + y * (*(float (*)[4][4])&g_BrDrawCombined)[1][1]
         + z * (*(float (*)[4][4])&g_BrDrawCombined)[2][1] + (*(float (*)[4][4])&g_BrDrawCombined)[3][1];
}
