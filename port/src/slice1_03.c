/* slice1_03.c -- decompiled from BRD3D.dll, pass-03 packet.
 * See slice1_03.h for the map of what is in here and why.
 */
#include "slice1_03.h"

#include <stdio.h>

/* =====================================================================
 * 1. Clipping
 *
 * Original storage:
 *   0x104C01A8 .. 0x104C0BA8   node pool, 0xA00 bytes = 64 * 0x28
 *   0x104C0BBC                 free-list head
 *   0x1008F3C8                 the compared-against constant -- it is 0.0f,
 *                              read out of the DLL's .rdata.
 * ===================================================================== */

static BrClipVert *g_pClipFree;    /* 0x104C0BBC */
static BrClipVert *g_aClipPool;    /* 0x104C01A8 */
static int         g_cClipPool;    /* (0x104C0BA8 - 0x104C01A8) / 0x28 = 64 */

void BrClipPoolInit(BrClipVert *aNodes, int cNodes)
{
    int i;

    g_aClipPool = aNodes;
    g_cClipPool = (aNodes != NULL && cNodes > 0) ? cNodes : 0;
    g_pClipFree = NULL;

    /* Thread lowest-index-first so the free list pops in ascending order,
     * which is what a fresh static pool would do. */
    for (i = g_cClipPool - 1; i >= 0; i--) {
        aNodes[i].pNext = g_pClipFree;
        g_pClipFree = &aNodes[i];
    }
}

int BrClipPoolCount(void)
{
    const BrClipVert *p;
    int n = 0;

    for (p = g_pClipFree; p != NULL; p = p->pNext)
        n++;
    return n;
}

/* 0x1001D940 */
BrClipVert *BrClipLerpVert(const BrClipVert *pA, const BrClipVert *pB,
                           float t)
{
    BrClipVert *pOut = g_pClipFree;

    if (pOut != NULL)
        g_pClipFree = pOut->pNext;

    /* DEVIATION: the original does the pop guarded (`test eax,eax / je`) but
     * then stores through eax unconditionally, so an exhausted free list is
     * a NULL dereference. Returning NULL instead; every caller in this file
     * checks and skips the insertion. */
    if (pOut == NULL)
        return NULL;

    /* +0x00 is deliberately not written -- it still holds the free-list
     * link, and each caller overwrites it right after this returns. */
    pOut->f04 = (pB->f04 - pA->f04) * t + pA->f04;
    pOut->f08 = (pB->f08 - pA->f08) * t + pA->f08;
    pOut->f0C = (pB->f0C - pA->f0C) * t + pA->f0C;
    pOut->f10 = (pB->f10 - pA->f10) * t + pA->f10;
    pOut->f14 = (pB->f14 - pA->f14) * t + pA->f14;
    pOut->f18 = (pB->f18 - pA->f18) * t + pA->f18;
    pOut->f1C = (pB->f1C - pA->f1C) * t + pA->f1C;
    pOut->f20 = (pB->f20 - pA->f20) * t + pA->f20;
    pOut->f24 = (pB->f24 - pA->f24) * t + pA->f24;
    return pOut;
}

typedef float (*BrClipDistFn)(const BrClipVert *pV);

/* The body shared by 0x1001D810 / 0x1001D9F0 / 0x1001DB30 / 0x1001DC70.
 * Those four are byte-for-byte identical apart from the two `fld/fadd/fsub`
 * pairs that produce the plane distance, so they are one function here with
 * the distance passed in.
 *
 * Register correspondence, for anyone checking against the disassembly:
 *   ecx  pPrev     previous vertex of the SOURCE polygon
 *   edi  pOutPrev  previous node of the OUTPUT list (they diverge)
 *   esi  pCur
 *   ebp  pDead     chain of unlinked vertices, recycled at the end
 *   ebx  pList
 *   [esp+0x14] i,  [esp+0x18] pNext, [esp+0x10] dCur, [esp+0x20] dPrev
 */
static void BrClipPlane(BrClipList *pList, BrClipDistFn pfnDist)
{
    BrClipVert *pPrev;
    BrClipVert *pOutPrev;
    BrClipVert *pCur;
    BrClipVert *pNext;
    BrClipVert *pDead = NULL;
    BrClipVert *pTmp;
    BrClipVert *pNew;
    float dCur, dPrev, t;
    int i;

    /* DEVIATION: the original loads pList->pHead and dereferences it before
     * testing the count, so an empty list with a NULL head faults. */
    if (pList == NULL || pList->pHead == NULL)
        return;

    pPrev    = pList->pHead;
    i        = pList->cVerts;
    pCur     = pPrev->pNext;
    pOutPrev = pPrev;

    if (i > 0) {
        for (;;) {
            dCur  = pfnDist(pCur);
            dPrev = pfnDist(pPrev);
            pNext = pCur->pNext;    /* saved before anything is relinked */

            /* Written as `>= 0.0f` rather than `< 0.0f` on purpose: the
             * original branches on the x87 C0 bit, which is also set for an
             * unordered compare, so a NaN distance counts as OUTSIDE. */
            if (dCur >= 0.0f) {
                if (dPrev >= 0.0f) {
                    /* both inside -- nothing to do but advance */
                    pOutPrev = pCur;
                } else {
                    /* entering: splice an on-plane vertex in before pCur */
                    t = dPrev / (dPrev - dCur);
                    pNew = BrClipLerpVert(pPrev, pCur, t);
                    if (pNew != NULL) {
                        pNew->pNext = pOutPrev->pNext;
                        pOutPrev->pNext = pNew;
                        pList->cVerts = pList->cVerts + 1;
                    }
                    pOutPrev = pCur;
                }
            } else if (dPrev >= 0.0f) {
                /* leaving: drop pCur, splice an on-plane vertex in its
                 * place. The count is deliberately left alone -- one out,
                 * one in. */
                pTmp = pOutPrev->pNext;
                if (pTmp != NULL)
                    pOutPrev->pNext = pTmp->pNext;

                t = dCur / (dCur - dPrev);
                pCur->pNext = pDead;
                pDead = pCur;

                pNew = BrClipLerpVert(pCur, pPrev, t);
                if (pNew != NULL) {
                    pNew->pNext = pOutPrev->pNext;
                    pOutPrev->pNext = pNew;
                    pOutPrev = pNew;
                }
            } else {
                /* both outside: drop pCur */
                pTmp = pOutPrev->pNext;
                if (pTmp != NULL)
                    pOutPrev->pNext = pTmp->pNext;

                pCur->pNext = pDead;
                pDead = pCur;
                pList->cVerts = pList->cVerts - 1;
            }

            pPrev = pCur;
            pCur  = pNext;

            /* Two independent exits, in this order. The count test comes
             * first, so a polygon that collapses below 2 vertices stops
             * immediately and leaves the remaining vertices unvisited. */
            if (pList->cVerts < 2)
                break;
            if (--i <= 0)
                break;
        }
    }

    /* The head is set to pCur unconditionally -- which, after a full pass
     * over a circular list, is the node one position past the old head. So
     * every clip call ROTATES the polygon by one vertex. On the early exits
     * it can even be a vertex that was just discarded. */
    pList->pHead = pCur;

    /* Return the discarded chain to the free list, but only those nodes
     * that live inside the pool; anything else is dropped on the floor. */
    pTmp = pDead;
    if (pDead != NULL)
        pDead = pDead->pNext;

    while (pTmp != NULL) {
        if (g_aClipPool != NULL &&
            pTmp >= g_aClipPool && pTmp < g_aClipPool + g_cClipPool) {
            pTmp->pNext = g_pClipFree;
            g_pClipFree = pTmp;
        }
        pNew = pDead;
        if (pDead != NULL)
            pDead = pDead->pNext;
        pTmp = pNew;
    }
}

static float BrClipDistW(const BrClipVert *pV)          { return pV->f18; }
static float BrClipDistWPlusF04(const BrClipVert *pV)   { return pV->f18 + pV->f04; }
static float BrClipDistWMinusF04(const BrClipVert *pV)  { return pV->f18 - pV->f04; }
/* 0x1001DC70 loads f08 first and adds f18, unlike its three siblings which
 * lead with f18. Same value, but noted because it is the one asymmetry. */
static float BrClipDistWPlusF08(const BrClipVert *pV)   { return pV->f08 + pV->f18; }

void BrClipPlaneW(BrClipList *pList)          { BrClipPlane(pList, BrClipDistW); }
void BrClipPlaneWPlusF04(BrClipList *pList)   { BrClipPlane(pList, BrClipDistWPlusF04); }
void BrClipPlaneWMinusF04(BrClipList *pList)  { BrClipPlane(pList, BrClipDistWMinusF04); }
void BrClipPlaneWPlusF08(BrClipList *pList)   { BrClipPlane(pList, BrClipDistWPlusF08); }

/* =====================================================================
 * 2. Text / HUD
 * ===================================================================== */

static BrTextState g_text;

BrTextState *BrTextGetState(void)
{
    return &g_text;
}

/* 0x100192A0 */
void BrTextSetColors(int a1, int a2, int a3, int a4, int a5, int a6)
{
    g_text.f0A74A8 = a1;
    g_text.f0A74AC = a2;
    g_text.f0A74B0 = a3;
    g_text.f4B0364 = 1;
    g_text.f4B0368 = a4;
    g_text.f4B036C = a5;
    g_text.f4B0370 = a6;
}

/* 0x10019300 */
void BrTextDraw(const char *psz, int x, int y)
{
    int w;

    /* Two dwords into the display list, cursor advanced by 8 bytes. On the
     * N64 command set 0xB6 is G_CLEARGEOMETRYMODE and the payload 0x1 is
     * G_ZBUFFER, i.e. "turn the z-buffer off for this text". */
    if (g_text.pGfx != NULL) {      /* DEVIATION: original never checks */
        g_text.pGfx[0] = 0xB6000000u;
        g_text.pGfx[1] = 0x00000001u;
        g_text.pGfx += 2;
    }

    switch (g_text.align) {
    case BR_TEXT_ALIGN_CENTER:
        w = (g_text.pfnMeasure != NULL)
                ? g_text.pfnMeasure(psz, g_text.scale) : 0;
        /* the original uses `sar eax,1`, an arithmetic shift, not a signed
         * divide -- they differ for a negative measurement */
        g_text.x = x - (w >> 1);
        g_text.y = y;
        break;

    case BR_TEXT_ALIGN_RIGHT:
        w = (g_text.pfnMeasure != NULL)
                ? g_text.pfnMeasure(psz, g_text.scale) : 0;
        g_text.x = x - w;
        g_text.y = y;
        break;

    case BR_TEXT_ALIGN_LEFT:
        g_text.x = x;
        g_text.y = y;
        break;

    default:
        /* x is NOT touched here: the original's case-0 arm falls through
         * into the default arm, which only stores y. Any align value other
         * than 0/1/2 therefore reuses the previous call's x. */
        g_text.y = y;
        break;
    }

    if (g_text.pfnDrawString != NULL)
        g_text.pfnDrawString(psz);
}

void BrFormatTime(char *pszOut, size_t cbOut, const char *pszPrefix,
                  float fSeconds)
{
    /* 0x1007C8A0 is __ftol: it forces the x87 rounding mode to chop, so the
     * conversion truncates toward zero, matching a C cast. */
    int total      = (int)(fSeconds * 100.0f);
    int hundredths = total % 100;
    int minutes;
    int seconds;

    total  /= 100;
    minutes = total / 60;
    seconds = total % 60;

    /* DEVIATION: the original sprintf()s into a 32-byte stack buffer with an
     * unbounded "%s" prefix. snprintf here. */
    snprintf(pszOut, cbOut, "%s%d:%02d.%02d",
             (pszPrefix != NULL) ? pszPrefix : "",
             minutes, seconds, hundredths);
}

/* 0x100171F0 */
void BrHudDrawTimeEntry(const char *pszLabel, const char *pszPrefix,
                        float fSeconds, int x, int y)
{
    char sz[32];      /* the original's local buffer is exactly 0x20 */

    BrFormatTime(sz, sizeof(sz), pszPrefix, fSeconds);

    /* the time line goes out first, 15 pixels below the label */
    BrTextDraw(sz, x, y + 15);
    BrTextDraw(pszLabel, x, y);
}

/* =====================================================================
 * 3. Glue
 * ===================================================================== */

static BrAppMsgHooks g_appMsg;

BrAppMsgHooks *BrAppMsgGetHooks(void)
{
    return &g_appMsg;
}

/* 0x1000BEA0
 *
 * The original is two dispatchers stacked, and both collapse:
 *
 *  - ids <= 0x21 run a compare chain (`cmp 0x21 / je`, `sub 3 / je`,
 *    `sub 2 / jne`) whose only non-returning outcome is id == 5.
 *
 *  - ids in [0x31, 0x107] index a 0xD7-byte selector table at 0x1000BF20
 *    into a 6-entry jump table at 0x1000BF08. Read out of the DLL, five of
 *    the six jump targets are the same bare `ret` at 0x1000BF04, and the
 *    selector byte 4 -- the only one reaching real code at 0x1000BEE6 --
 *    appears at exactly ONE index, 0xD6, i.e. id 0x107.
 *
 * So the whole table apparatus is equivalent to a single `if`. */
void BrAppMsgDispatch(void *pv1, const BrAppMsg *pMsg, void *pv3, void *pv4,
                      void *pv5)
{
    int32_t id;

    (void)pv3;    /* pushed by every caller, never read by the original */
    (void)pv4;

    if (pMsg == NULL)
        return;   /* DEVIATION: the original dereferences unconditionally */

    id = pMsg->id;

    if (id > 0x21) {
        if ((uint32_t)(id - 0x31) > 0xD6u)
            return;
        if (id != 0x107)
            return;
        if (g_appMsg.pfnMsg107 != NULL)
            g_appMsg.pfnMsg107(pv1, pMsg->f0C, pMsg->f10, pMsg->f08, pv5);
        return;
    }

    if (id != 5)
        return;
    if (g_appMsg.f0AC300 != 0)
        return;
    if (g_appMsg.pfnMsg5 != NULL)
        g_appMsg.pfnMsg5(pMsg->f08);
}

static BrComHolder  *g_pComHolder;      /* 0x10A9D008 */
static BrComLockHooks g_comLock;

BrComHolder **BrComGetHolderSlot(void)
{
    return &g_pComHolder;
}

BrComLockHooks *BrComGetLockHooks(void)
{
    return &g_comLock;
}

/* 0x1000C4A0 */
int BrComHolderRelease(void)
{
    BrComHolder *pHolder = g_pComHolder;
    BrComObj    *pObj;
    void        *pArg;
    int          rc;

    if (pHolder == NULL)
        return 0;
    pObj = pHolder->pObj;
    if (pObj == NULL)
        return 0;
    pArg = pHolder->pArg;
    if (pArg == NULL)
        return 0;

    rc = pObj->pVtbl->pfn24(pObj, pArg);

    /* the holder global is re-read here, exactly as the original does --
     * so a callback that swaps 0x10A9D008 clears the NEW holder's +0x08 */
    g_pComHolder->pArg = NULL;
    return rc;
}

/* 0x1000C4D0 */
int BrComCallLocked68(BrComObj *pThis, void *a2, void *a3, void *a4,
                      void *a5, void *a6)
{
    int rc;

    if (g_comLock.pfnEnter != NULL)
        g_comLock.pfnEnter(g_comLock.pCrit);

    /* DEVIATION: the original reads pThis->pVtbl with no NULL check. */
    rc = (pThis != NULL) ? pThis->pVtbl->pfn68(pThis, a2, a3, a4, a5, a6) : 0;

    if (g_comLock.pfnLeave != NULL)
        g_comLock.pfnLeave(g_comLock.pCrit);

    return rc;
}
