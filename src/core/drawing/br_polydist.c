/* br_polydist.c -- drawing: how far a corner is from each screen edge.
 *
 * RESPONSIBILITY: drawing/ -- turn geometry and images into pixels.
 *
 * Filed out of slice4_52.c, an address batch and not a module.  Four
 * contiguous one-line routines the polygon trimmer calls, one per edge:
 *
 *     0x1000DEC0   x                  the LEFT edge
 *     0x1000DED0   constant - x       the RIGHT edge
 *     0x1000DEE0   y                  the TOP edge
 *     0x1000DEF0   constant - y       the BOTTOM edge
 *
 * The pairing is read off the offsets and the shared 0x1007720C constant,
 * and the four addresses being 0x10 apart is what says they are one
 * translation unit.  The port's BrPolyDistX / BrPolyDistY return float where
 * the matching twins return double; both spellings are kept.
 *
 * slice4_52.c's preamble is carried over verbatim.  An include set that
 * looks redundant has already been shown elsewhere in this module to move
 * VC5's register allocation (see br_rdpmode.c).
 */
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include "slice4_52.h"
#include "slice1_03.h"      /* BrComCallLocked68 (0x1000C4D0) */

#include "slice3_33.h"   /* BrUiScreen / BrUiCtl / BrUiPhase, BrOperatorNew,
                          * BrUiCtlCtor, BrErrShow  (pulls slice1_06.h)      */
#include "slice1_07.h"   /* BrTables64Clear                                  */
#include "slice3_39.h"   /* g_BrDikState / g_BrDikEdge / g_BrDikPrev,
                          * g_pBrAA2E80                                      */
#include "slice2_22.h"   /* BrDPlayRandStep, BrDPlaySendTag3, BrDPlayLink    */
#include "slice2_14.h"   /* BrScrPt                                          */
#include "slice1_01.h"   /* BrAdler32                                        */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ==========================================================================
 * 0x10010960 / 0x10010980  BrPolyDistX / BrPolyDistY
 * ========================================================================== */

/* WHAT IT DOES: tells the shape-trimming code how far a corner lies from the
 * left edge of the screen -- which is just its horizontal position, so a
 * negative answer means the corner is off to the left. Its neighbour below does
 * the same for the top edge. */
/* @implements 0x1000DEC0 glide BrPolyDistX */
float BrPolyDistX(const struct BrScrPt *pPt)
{
    return ((const BrScrPt *)pPt)->f0C;
}

float BrPolyDistY(const struct BrScrPt *pPt)
{
    return ((const BrScrPt *)pPt)->f10;
}

#ifdef BR_MATCHING_BUILD
extern int DAT_117a5f28;
extern float _DAT_1007720c;
int FUN_10069A80();
int FUN_10069a80();

/* WHAT IT DOES: return the float at offset +0x10 in a struct, cast to double. */
/* @implements 0x1000DEE0 glide BrGetFieldFloat */

double BrGetFieldFloat(int param_1)

{
  return (double)*(float *)(param_1 + 0x10);
}

/* WHAT IT DOES: return (constant at 0x1007720C) minus the float at +0xC, as double. */
/* @implements 0x1000DED0 glide BrGetFieldFloatSubC */

double BrGetFieldFloatSubC(int param_1)

{
  return (double)_DAT_1007720c - (double)*(float *)(param_1 + 0xc);
}

/* WHAT IT DOES: return (constant at 0x1007720C) minus the float at +0x10, as double. */
/* @implements 0x1000DEF0 glide BrGetFieldFloatSub10 */

double BrGetFieldFloatSub10(int param_1)

{
  return (double)_DAT_1007720c - (double)*(float *)(param_1 + 0x10);
}

/* ==========================================================================
 * 0x1000DC00 (D3D 0x100106A0, shared body) -- BrPolyClipTri
 *
 * The port body is slice2_13.c's BrPolyClipTri; this is the same function
 * written the way the bytes say.  Everything that matters:
 *  - the pool pop is INLINE and unguarded: `n = free; if (n) free = n->pNext;`
 *    then the node is used whether or not it was NULL;
 *  - the six copied words go through `n->pData` re-read per statement (the
 *    stores may alias it), and the source point is the PARAMETER, read once;
 *  - the four plane calls are NESTED on `cVerts >= 2`, and the decision is
 *    a final `if (cVerts < 2) free-loop else draw-loop`; VC5 threads every
 *    early `jl` straight into the free loop;
 *  - both loops recycle through the global free-list head directly; in the
 *    call-free free loop VC5 register-caches it (see the note there);
 *  - the draw loop's counter is the THIRD VERTEX PARAMETER reused (its slot
 *    at [esp+0x30]), so that parameter is declared as an int here. */
typedef struct BrPolyTriList {
    BrLerpNode *pHead;    /* +0x00 circular ring */
    int32_t     cVerts;   /* +0x04 */
} BrPolyTriList;

extern BrLerpNode *g_2E5ECC;              /* 0x102E16B4 free-list head   */
extern BrLerpNode  g_2E54C0[64];          /* 0x102E0CA8 the node pool    */
extern void BrPointProjectXY(float *v);                     /* 0x1000E270 */
extern void BrPolyClipPlane(BrPolyTriList *pList, void *pfnDist); /* 0x1000DF00 */
extern void BrScrPtKeepNearest(float *pM, int aOut, int aFlags, int corner,
                               float *pPt, float u, float v, int pRef); /* 0x1000E150 */

#define BR_POLY_CLIP_MAX 1024.0f

/* WHAT IT DOES: build a triangle out of three screen points as a ring of
 * three pool nodes, project each, clip the ring against the four scissor
 * planes, and if at least two corners survive offer every surviving corner
 * to the keep-nearest test at each of the four corners of the 1024x1024
 * square.  Fewer than two survivors returns the ring to the pool instead.
 * Nodes are recycled only if their address lies inside the pool. */
/* @implements 0x1000DC00 glide BrPolyClipTri */
void BrPolyClipTri(float *pM, int aOut, int aFlags, const BrScrPt *pV0,
                   const BrScrPt *pV1, int iV2, int pRef)
{
    BrPolyTriList list;
    BrLerpNode   *n0, *n1, *n2;
    const BrScrPt *pSrc;

    list.pHead = NULL;

    n0 = g_2E5ECC;
    if (n0 != NULL)
        g_2E5ECC = n0->pNext;
    pSrc = (const BrScrPt *)iV2;
    n0->pData = &n0->data[0];
    n0->pData[0] = pSrc->f00;
    n0->pData[1] = pSrc->f04;
    n0->pData[2] = pSrc->f08;
    n0->pData[5] = pSrc->pad[0];
    n0->pData[6] = pSrc->pad[1];
    n0->pData[7] = pSrc->pad[2];
    BrPointProjectXY(n0->pData);
    n0->pNext  = list.pHead;
    list.pHead = n0;

    n1 = g_2E5ECC;
    if (n1 != NULL)
        g_2E5ECC = n1->pNext;
    n1->pData = &n1->data[0];
    n1->pData[0] = pV1->f00;
    n1->pData[1] = pV1->f04;
    n1->pData[2] = pV1->f08;
    n1->pData[5] = pV1->pad[0];
    n1->pData[6] = pV1->pad[1];
    n1->pData[7] = pV1->pad[2];
    BrPointProjectXY(n1->pData);
    n1->pNext  = list.pHead;
    list.pHead = n1;

    n2 = g_2E5ECC;
    if (n2 != NULL)
        g_2E5ECC = n2->pNext;
    n2->pData = &n2->data[0];
    n2->pData[0] = pV0->f00;
    n2->pData[1] = pV0->f04;
    n2->pData[2] = pV0->f08;
    n2->pData[5] = pV0->pad[0];
    n2->pData[6] = pV0->pad[1];
    n2->pData[7] = pV0->pad[2];
    BrPointProjectXY(n2->pData);
    n2->pNext  = list.pHead;
    list.pHead = n2;
    n0->pNext  = n2;
    list.cVerts = 3;

    BrPolyClipPlane(&list, (void *)BrPolyDistX);
    if (list.cVerts >= 2) {
        BrPolyClipPlane(&list, (void *)BrGetFieldFloatSubC);
        if (list.cVerts >= 2) {
            BrPolyClipPlane(&list, (void *)BrGetFieldFloat);
            if (list.cVerts >= 2)
                BrPolyClipPlane(&list, (void *)BrGetFieldFloatSub10);
        }
    }

    if (list.cVerts < 2) {
        int32_t k;

        /* list.pHead is the cursor and is advanced BEFORE the recycle, so
         * the original re-reads it after `p->pNext = ...` (that store may
         * alias the list, whose address has escaped).  The free-list head
         * is the PLAIN GLOBAL: with no call in the loop VC5 keeps it in edx
         * across iterations and writes it through (`mov edx,eax; mov
         * [g],edx`) -- a local copy compiles to a store from eax instead
         * (the A3 short form, one byte short). */
        for (k = list.cVerts; k > 0; k--) {
            BrLerpNode *p = list.pHead;

            list.pHead = p->pNext;
            if (p >= &g_2E54C0[0] && p < &g_2E54C0[64]) {
                p->pNext = g_2E5ECC;
                g_2E5ECC = p;
            }
        }
    } else {
        for (iV2 = 0; iV2 < list.cVerts; iV2++) {
            BrLerpNode *p = list.pHead;

            list.pHead = p->pNext;
            BrScrPtKeepNearest(pM, aOut, aFlags, 0, p->pData, 0.0f, 0.0f, pRef);
            BrScrPtKeepNearest(pM, aOut, aFlags, 1, p->pData,
                               BR_POLY_CLIP_MAX, 0.0f, pRef);
            BrScrPtKeepNearest(pM, aOut, aFlags, 2, p->pData,
                               0.0f, BR_POLY_CLIP_MAX, pRef);
            BrScrPtKeepNearest(pM, aOut, aFlags, 3, p->pData,
                               BR_POLY_CLIP_MAX, BR_POLY_CLIP_MAX, pRef);
            if (p >= &g_2E54C0[0] && p < &g_2E54C0[64]) {
                p->pNext = g_2E5ECC;
                g_2E5ECC = p;
            }
        }
    }
}

#endif /* BR_MATCHING_BUILD */
