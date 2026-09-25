/* br_polyclip.c -- drawing: the screen-edge polygon clipper and its node pool.
 *
 * RESPONSIBILITY: drawing/ -- turn geometry and images into pixels.
 *
 * A triangle headed for the screen is turned into a ring of corner nodes from
 * a fixed pool, trimmed against each of the four screen edges in turn by
 * BrPolyClipPlane (which inserts a new corner wherever the outline crosses an
 * edge and recycles the corners that fall outside), and the surviving corners
 * are handed on. The pool, its bounds test and the edge-distance functions
 * share the file-static pool array, so they move as one group.
 *
 * Filed out of the address batch slice2_13.c; its preamble is carried over
 * verbatim. See slice2_13.h for the identification notes and every GOTCHA.
 */
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef BR_MATCHING_BUILD
/* Header prototype is cdecl; the original is __stdcall. */
#define BrFileWriteChecked BrFileWriteChecked_cdecl
#define BrDPlayThreadProc  BrDPlayThreadProc_cdecl_hdr
#endif
#include "slice2_13.h"
#ifdef BR_MATCHING_BUILD
#undef BrFileWriteChecked
#undef BrDPlayThreadProc
uint32_t __stdcall BrDPlayThreadProc(void *pvCtx);
#endif
#include "slice1_03.h"   /* BrAppMsg, BrAppMsgDispatch (= 0x1000BEA0) */

/* ==========================================================================
 * Cross-slice declarations
 * ========================================================================== */

/* 0x10008CC0 -- the printf-style error reporter both file helpers call.
 * It is in no packet in this slice.
 * XSLICE 0x10008CC0 */
extern void BrErrorf(const char *pszFmt, ...);

/* XSLICE 0x10071480 */
extern void BrSub10071480(uint32_t idPlayer);
/* XSLICE 0x10005FE0 */
extern void BrSub10005FE0(uint32_t idPlayer);
/* XSLICE 0x100360F0 */
extern void BrSub100360F0(void *pv1, uint32_t f0C, uint32_t f10,
                          uint32_t f08, uint32_t idTo);
/* XSLICE 0x1003CE80 */
extern void BrSub1003CE80(void);
/* 0x1000BAF0, the non-system message route. slice2_22 knows it as
 * APPMSG_HOSTSTARTED.
 * XSLICE 0x1000BAF0 */
extern void BrSub1000BAF0(void *pCtx, const void *pvData, uint32_t cbData,
                          uint32_t idFrom, uint32_t idTo);
/* 0x1003D0B0 -- "size it, allocate it, fill it" over state->pDPGlobal.
 * *ppvOut receives a GlobalAlloc'd + GlobalLock'ed record.
 * XSLICE 0x1003D0B0 */
extern int32_t BrSub1003D0B0(BrDPlay4Obj *pObj, void **ppvOut);


/* ==========================================================================
 * 4. The 0x102E54C0 clip pool
 * ========================================================================== */

static BrLerpNode g_aBrPolyPool[BR_POLY_POOL_NODES];

BrLerpNode *BrPolyPoolBase(void)
{
    return g_aBrPolyPool;
}

void BrPolyPoolInit(void)
{
    int i;

    /* The original walks DOWN from the highest node, so the head ends up on
     * the lowest one and next always points at the node above. */
    g_aBrPolyPool[BR_POLY_POOL_NODES - 1].pNext = NULL;
    for (i = BR_POLY_POOL_NODES - 2; i >= 0; --i)
        g_aBrPolyPool[i].pNext = &g_aBrPolyPool[i + 1];

    g_pBrLerpFree = &g_aBrPolyPool[0];
}

int BrPolyPoolCount(void)
{
    BrLerpNode *p = g_pBrLerpFree;
    int         n = 0;

    while (p != NULL) {
        ++n;
        p = p->pNext;
    }
    return n;
}

BrLerpNode *BrPolyPoolAlloc(void)
{
    BrLerpNode *p = g_pBrLerpFree;

    if (p != NULL)
        g_pBrLerpFree = p->pNext;

    /* DEVIATION: the original returns the null head and every caller then
     * dereferences it. NULL is returned here and every caller guards. */
    return p;
}

/* The pool bounds test, spelled the way the original spells it:
 * 0x102E54C0 <= p < 0x102E5EC0. */
static int BrPolyInPool(const BrLerpNode *p)
{
    return p >= &g_aBrPolyPool[0] && p < &g_aBrPolyPool[BR_POLY_POOL_NODES];
}

/* @n64 0x8026BA70 located */
void BrPolyPoolFree(BrLerpNode *pNode)
{
    if (!BrPolyInPool(pNode))
        return;   /* silently dropped -- this is the original's behaviour */

    pNode->pNext  = g_pBrLerpFree;
    g_pBrLerpFree = pNode;
}

/* @n64 0x80223470 located */
float BrPolyDistMaxX(const BrScrPt *pPt)
{
    return BR_POLY_CLIP_MAX - pPt->f0C;
}

float BrPolyDistMaxY(const BrScrPt *pPt)
{
    return BR_POLY_CLIP_MAX - pPt->f10;
}

/* -- 0x100109A0 ---------------------------------------------------------- */

/* WHAT IT DOES: trims a shape against one straight edge, so that only the part
 * on the visible side survives. It walks the ring of corners, drops the ones
 * that fall outside, and puts a new corner exactly where the outline crosses
 * the edge. Calling it four times -- once per side -- is how a triangle gets
 * cut down to what actually fits on screen. */
/* @t4-pass 0x1000DF00 1 2026-09-07 probes 97 bytes 359 insns 132 regions 5 rows 5 census yes  (tools/crank.py) */
/* @t4-pass 0x1000DF00 2 2026-09-07 probes 92 bytes 359 insns 132 regions 5 rows 5 census yes  (tools/crank.py) */
/* @t4-pass 0x1000DF00 3 2026-09-10 probes 30 bytes 339 insns 127 regions 3 rows 2 census yes  (tools/crank.py) */
/* @t4-pass 0x1000DF00 4 2026-09-10 probes 30 bytes 339 insns 127 regions 3 rows 2 census yes  (tools/crank.py) */
/* @t3 0x1000DF00 2026-09-10 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 339/347 insns 127/129 rows 2+0 regions 3 oracle UNCLASSIFIED
 * @t3-effort passes 4 zero-movement 3 4
 * RESIDUE: two homed stack slots the original keeps in registers -- the
 * recycle walk's look-ahead and its head -- and nothing else; the whole
 * multiset pairs and the two extra rows are allocation singletons.  What
 * closed the rest is in the two comments below: the count's write-back is
 * shared by both arms through a goto, and the look-ahead starts AS p so
 * the null case reuses p instead of materialising a zero.
 * 30 compiles in the last pass, levers accepted: none that survived the
 * cluster rule; every candidate and score is in build/match/crank.log.
 * Do not reopen before the end-grind (CLAUDE.md rule 12). */
/* @implements 0x1000DF00 glide BrPolyClipPlane */
void BrPolyClipPlane(BrPolyList *pList, BrPolyDistFn pfnDist)
{
    BrLerpNode *pPrev    = pList->pHead;
    BrLerpNode *pCur     = pPrev->pNext;
    BrLerpNode *pOut     = pPrev;
    BrLerpNode *pRecycle = NULL;
    int32_t     n        = pList->cVerts;
    /* The count's write-back is SHARED by the entering and the wholly-outside
     * arms: the original loads it, adjusts it and `jmp`s into the other arm's
     * store.  Writing `pList->cVerts = pList->cVerts +/- 1` in each arm gives
     * two stores. */
    int32_t     cv;

    if (n > 0) {
        for (;;) {
            BrLerpNode *pNext = pCur->pNext;
            float dCur  = pfnDist((const BrScrPt *)(const void *)pCur->pData);
            float dPrev = pfnDist((const BrScrPt *)(const void *)pPrev->pData);

            /* Nested on the x87 C0 flags; int temps materialize 0/1. */
            if (dCur >= 0.0f) {
                if (dPrev >= 0.0f) {
                pOut = pCur;
            } else {
                /* entering: splice in the crossing, keep pCur */
                float       t    = dPrev / (dPrev - dCur);
                BrLerpNode *pNew = BrLerpNodeAlloc(pPrev, pCur, t);

#ifndef BR_MATCHING_BUILD
                /* DEVIATION: the original never tests the allocation and
                 * writes through a null node when the pool is empty. */
                if (pNew != NULL) {
#endif
                    pNew->pNext = pOut->pNext;
                    pOut->pNext = pNew;
#ifndef BR_MATCHING_BUILD
                }
#endif
                cv = pList->cVerts + 1;
                pOut = pCur;
                goto BR_STORE_CV;
                }
            } else if (dPrev >= 0.0f) {
                /* leaving: drop pCur, splice in the crossing in its place */
                float       t;
                BrLerpNode *pNew;

                if (pOut->pNext != NULL)
                    pOut->pNext = pOut->pNext->pNext;

                t = dCur / (dCur - dPrev);
                pCur->pNext = pRecycle;
                pRecycle    = pCur;

                pNew = BrLerpNodeAlloc(pCur, pPrev, t);
#ifndef BR_MATCHING_BUILD
                if (pNew != NULL) {
#endif
                    pNew->pNext = pOut->pNext;
                    pOut->pNext = pNew;
                    pOut        = pNew;
#ifndef BR_MATCHING_BUILD
                }
#endif
                /* count unchanged: one out, one in */
            } else {
                /* wholly outside: drop pCur */
                if (pOut->pNext != NULL)
                    pOut->pNext = pOut->pNext->pNext;

                pCur->pNext = pRecycle;
                pRecycle    = pCur;
                cv = pList->cVerts - 1;
BR_STORE_CV:
                pList->cVerts = cv;
            }

            pPrev = pCur;
            pCur  = pNext;

            if (pList->cVerts < 2)
                break;
            if (--n <= 0)
                break;
        }
    }

    pList->pHead = pCur;

    {
        /* The chain is walked one node ahead, because the free-list push
         * overwrites the node's link. Inlined: a call to BrPolyPoolFree
         * cannot emit the hoisted free-head load. */
        BrLerpNode *p      = pRecycle;
        /* The look-ahead starts AS p, and only steps when p is non-null: the
         * original reuses the null p as the null look-ahead.  A `: NULL` arm
         * materialises a fresh zero and costs the extra `xor`/`jmp`. */
        BrLerpNode *pNextR = p;

        if (p != NULL)
            pNextR = p->pNext;

        while (p != NULL) {
            BrLerpNode *pHead = g_pBrLerpFree;

            if (p >= &g_aBrPolyPool[0] && p < &g_aBrPolyPool[BR_POLY_POOL_NODES]) {
                p->pNext      = pHead;
                g_pBrLerpFree = p;
            }
            p = pNextR;
            if (p != NULL)
                pNextR = p->pNext;
        }
    }
}

/* -- 0x100106A0 ---------------------------------------------------------- */

/* One of the three identical vertex-setup blocks the original inlines. */
static BrLerpNode *BrPolyMakeVert(const BrScrPt *pSrc)
{
    BrLerpNode *p = BrPolyPoolAlloc();
    BrScrPt    *pDst;

    if (p == NULL)
        return NULL;   /* DEVIATION: see BrPolyPoolAlloc */

    p->pData = &p->data[0];
    pDst     = (BrScrPt *)(void *)p->pData;

    /* f0C / f10 are deliberately NOT copied -- BrScrPtProject produces them. */
    pDst->f00    = pSrc->f00;
    pDst->f04    = pSrc->f04;
    pDst->f08    = pSrc->f08;
    pDst->pad[0] = pSrc->pad[0];
    pDst->pad[1] = pSrc->pad[1];
    pDst->pad[2] = pSrc->pad[2];

    BrScrPtProject(pDst);
    return p;
}

void BrPolyClipTri(const BrMat4 *pM, BrScrPt *aOut, int *aFlags,
                   const BrScrPt *pV0, const BrScrPt *pV1, const BrScrPt *pV2,
                   const BrDepthRef *pRef)
{
    BrPolyList  list;
    BrLerpNode *n0, *n1, *n2;
    int32_t     i;

    list.pHead  = NULL;
    list.cVerts = 0;

    /* Allocation order is pV2, pV1, pV0 -- the LAST vertex argument gets the
     * first node off the free list. */
    n0 = BrPolyMakeVert(pV2);
    if (n0 == NULL)
        return;
    n0->pNext  = list.pHead;   /* NULL at this point */
    list.pHead = n0;

    n1 = BrPolyMakeVert(pV1);
    if (n1 == NULL)
        return;
    n1->pNext  = list.pHead;   /* n0 */
    list.pHead = n1;

    n2 = BrPolyMakeVert(pV0);
    if (n2 == NULL)
        return;
    n2->pNext  = list.pHead;   /* n1 */
    list.pHead = n2;

    n0->pNext   = n2;          /* close the ring: n2 -> n1 -> n0 -> n2 */
    list.cVerts = 3;

    BrPolyClipPlane(&list, BrPolyDistX);
    if (list.cVerts >= 2) BrPolyClipPlane(&list, BrPolyDistMaxX);
    if (list.cVerts >= 2) BrPolyClipPlane(&list, BrPolyDistY);
    if (list.cVerts >= 2) BrPolyClipPlane(&list, BrPolyDistMaxY);

    if (list.cVerts < 2) {
        int32_t k = list.cVerts;
        BrLerpNode *p = list.pHead;

        if (k <= 0)
            return;
        do {
            BrLerpNode *pNext = p->pNext;

            BrPolyPoolFree(p);
            p = pNext;
        } while (--k != 0);
        return;
    }

    for (i = 0; i < list.cVerts; ++i) {
        BrLerpNode    *p    = list.pHead;
        const BrScrPt *pPt  = (const BrScrPt *)(const void *)p->pData;

        list.pHead = p->pNext;

        BrScrPtKeepNearest(pM, aOut, aFlags, 0, pPt, 0.0f, 0.0f, pRef);
        BrScrPtKeepNearest(pM, aOut, aFlags, 1, pPt,
                           BR_POLY_CLIP_MAX, 0.0f, pRef);
        BrScrPtKeepNearest(pM, aOut, aFlags, 2, pPt,
                           0.0f, BR_POLY_CLIP_MAX, pRef);
        BrScrPtKeepNearest(pM, aOut, aFlags, 3, pPt,
                           BR_POLY_CLIP_MAX, BR_POLY_CLIP_MAX, pRef);

        BrPolyPoolFree(p);
    }
}
