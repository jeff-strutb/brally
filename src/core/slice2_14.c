/* slice2_14.c -- decompiled from BRD3D.dll, packet range 0x10010B00-0x100169B0.
 * See slice2_14.h for the recovered layouts and the argument-order notes.
 *
 * Every x87 sequence here was traced through its fxch/faddp chain and the
 * C below preserves the ORIGINAL ASSOCIATION ORDER of the additions, not a
 * tidied-up one, because float addition is not associative.
 *
 * Skipped functions and the reason for each are listed at the bottom of this
 * file so the information does not get lost.
 */
#include "slice2_14.h"
#include "slice2_17.h"   /* BrPropList, BrScenePropsDraw (0x1002FB20)          */
#include "slice3_40.h"   /* BrNode, BrPathPoint, BrG_6C7CB8 -- the AI path root */
#include "slice1_03.h"   /* BrTextDraw                                          */
#include "slice6_78.h"   /* BrTextSetSize, BrTextAlignCentre                    */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/* Layout guards: both of these strides are load-bearing (0x20 is the index
 * scale in 0x10010BF0, 4 is the element size passed to qsort at 0x10010E9E). */
typedef char br_assert_scrpt_stride[(sizeof(BrScrPt) == 0x20) ? 1 : -1];
typedef char br_assert_cell_stride[(sizeof(BrSortCell) == 4) ? 1 : -1];
typedef char br_assert_depthref[(sizeof(BrDepthRef) >= 0x3C) ? 1 : -1];

/* ================================================================== */
/* 0x10010B00 -- free-list pop + 8-component lerp                      */
/* ================================================================== */

BrLerpNode *g_pBrLerpFree = NULL;          /* 0x102E5ECC */

BrLerpNode *BrLerpNodeAlloc(const BrLerpNode *pFrom, const BrLerpNode *pTo,
                            float t)
{
    BrLerpNode *pNode;
    int i;

    pNode = g_pBrLerpFree;
    if (pNode != NULL) {
        g_pBrLerpFree = pNode->pNext;
    }

    /* DEVIATION: the original does NOT return here. With an empty free list
     * it falls straight through to `mov [eax+4], esi` with eax == 0 and
     * stores to address 4, then dereferences it eight times. Returning NULL
     * is the only safe rendering; callers must be prepared for it. */
    if (pNode == NULL) {
        return NULL;
    }

    pNode->pData = pNode->data;

    /* Eight fully unrolled steps in the original, all identical. Note that
     * both inputs are re-read through their +0x04 pointer on every step, so
     * an input whose pData is retargeted mid-call would be observed -- not
     * reachable from C, but that is why pData is loaded inside the loop. */
    for (i = 0; i < 8; i++) {
        const float *pA = pTo->pData;      /* arg2 */
        const float *pB = pFrom->pData;    /* arg1 */
        pNode->pData[i] = (pA[i] - pB[i]) * t + pB[i];
    }

    return pNode;
}

/* ================================================================== */
/* 0x10010BF0 -- conditional transform-and-store                       */
/* ================================================================== */

void BrScrPtKeepNearest(const BrMat4 *pM, BrScrPt *aOut, int *aFlags, int idx,
                        const BrScrPt *pIn, float cx, float cy,
                        const BrDepthRef *pRef)
{
    BrScrPt *pOut;
    float dx1, dy1, dx2, dy2;
    float d1sq, d2sq;
    float x, y, z, ox, oy, oz;
    float limit;

    /* Distances are computed before the destination is touched. */
    dx1 = pIn->f0C - cx;
    dy1 = pIn->f10 - cy;

    pOut = &aOut[idx];
    dx2 = pOut->f0C - cx;
    dy2 = pOut->f10 - cy;

    d1sq = dx1 * dx1 + dy1 * dy1;
    d2sq = dx2 * dx2 + dy2 * dy2;

    /* fcompp / test ah,0x41 / jne -> reject on less-than, equal OR unordered.
     * Spelled as !(a > b) so a NaN rejects here exactly as it does there. */
    if (!(d2sq > d1sq)) {
        return;
    }

    x = pIn->f00;
    y = pIn->f04;
    z = pIn->f08;

    /* Third output component first: it is the depth guard. */
    oz = (pM->m[2][2] * z + pM->m[1][2] * y) + pM->m[0][2] * x;
    oz = oz + pM->m[3][2];

    limit = pRef->f38 - BR_DEPTH_BIAS;

    /* fcomp / test ah,1 / jne -> reject when limit < oz or unordered. */
    if (!(limit >= oz)) {
        return;
    }

    ox = (pM->m[2][0] * z + pM->m[1][0] * y) + pM->m[0][0] * x;
    ox = ox + pM->m[3][0];

    oy = (pM->m[2][1] * z + pM->m[1][1] * y) + pM->m[0][1] * x;
    oy = oy + pM->m[3][1];

    pOut->f00 = ox;
    pOut->f04 = oy;
    pOut->f08 = oz;

    /* Copied as raw dwords by the original (`mov`), which for float storage
     * is the same thing as an assignment. */
    pOut->f0C = pIn->f0C;
    pOut->f10 = pIn->f10;

    aFlags[idx] = 1;
}

/* ================================================================== */
/* 0x10010D10 -- project position into the 2D key fields               */
/* ================================================================== */

BrMat4 g_BrScrProjMat;                     /* 0x106C0860 */

void BrScrPtProject(BrScrPt *pPt)
{
    float x = pPt->f00;
    float y = pPt->f04;
    float z = pPt->f08;
    float u, v;

    u = (g_BrScrProjMat.m[2][0] * z + g_BrScrProjMat.m[0][0] * x)
        + g_BrScrProjMat.m[1][0] * y;
    u = u + g_BrScrProjMat.m[3][0];

    v = (g_BrScrProjMat.m[2][1] * z + g_BrScrProjMat.m[0][1] * x)
        + g_BrScrProjMat.m[1][1] * y;
    v = v + g_BrScrProjMat.m[3][1];

    pPt->f0C = u;
    pPt->f10 = v;
}

/* ================================================================== */
/* 0x10010D90 -- qsort comparator                                      */
/* ================================================================== */

int BrSortCellCompare(const void *pA, const void *pB)
{
    short a = ((const BrSortCell *)pA)->key;
    short b = ((const BrSortCell *)pB)->key;

    if (a > b) {
        return 1;
    }
    /* xor edx,edx / setge dl / dec edx: 0 when equal, -1 when less. */
    return (a >= b) ? 0 : -1;
}

/* ================================================================== */
/* 0x10013A10 -- image header latch                                    */
/* ================================================================== */

BrRasterState g_BrRaster;

void BrRasterSelect(const BrRasterHdr *pHdr)
{
    /* Store order follows the original: 0x71C, 0x714, 0x710, 0x718. */
    g_BrRaster.p1C = (const void *)((const unsigned char *)pHdr + 8);
    g_BrRaster.f14 = pHdr->f02;
    g_BrRaster.f10 = pHdr->f04;
    g_BrRaster.f18 = pHdr->f06;
}

/* ================================================================== */
/* 0x10015BD0 -- debug text list                                       */
/* ================================================================== */

/* 0x100192A0: stores arg1..arg3 to 0x100A74A8/AC/B0, sets 0x104B0364 to 1,
 * and stores arg4..arg6 to 0x104B0368/6C/70. Two colour triples. */
/* XSLICE 0x100192A0 */
extern void BrTextSetColor6(int a, int b, int c, int d, int e, int f);

/* 0x10019260: writes byte 0 to 0x104B0358. */
/* XSLICE 0x10019260 */
extern void BrTextFlag358Clear(void);

/* 0x10019270: writes byte 2 to 0x104B035C. The siblings 0x10019280 and
 * 0x10019290 write 0 and 1 to the same byte, and 0x10019300 switches on it,
 * so it is an alignment mode; 2 is the centring branch. */
/* XSLICE 0x10019270 */
extern void BrTextAlignCentre(void);

/* 0x1003289F: four int arguments, purpose not established. NOTE the odd
 * (unaligned) entry address -- worth confirming it is a real function entry
 * and not a mid-function label the scanner picked up. */
/* XSLICE 0x1003289F */
extern void BrSub1003289F(int a, int b, int c, int d);

/* 0x100192F0: stores its argument to 0x104B0348, which 0x10019300 feeds to
 * the width measurement at 0x100193C0. */
/* XSLICE 0x100192F0 */
extern void BrTextSetSize(int size);

/* 0x10019300: (text, x, y). */
/* XSLICE 0x10019300 */
extern void BrTextDraw(const char *pText, int x, int y);

void BrHudDrawText(const BrHudCtx *pCtx, const int32_t aClear[4])
{
    const BrTextItem *pItem;
    int yMax;

    BrTextSetColor6(0xff, 0xff, 0xff, 0xff, 0xff, 0xff);
    BrTextFlag358Clear();
    BrTextAlignCentre();

    /* Pushed in reverse (+0x0C, +0x08, +0x04, +0x00), i.e. cdecl order 0..3. */
    BrSub1003289F(aClear[0], aClear[1], aClear[2], aClear[3]);

    /* The original's early-out reads 0x100A66FC, which IS aItems[0].pText. */
    if (pCtx->aItems[0].pText == NULL) {
        return;
    }

    yMax = pCtx->yLimit + BR_HUD_Y_SLOP;
    pItem = pCtx->aItems;

    for (;;) {
        if (pItem->y > BR_HUD_Y_MIN && pItem->y < yMax) {
            BrTextSetSize(pItem->size);
            /* `cdq / sub / sar 1` == signed division by two, truncating
             * toward zero, so a negative width halves toward zero too. */
            BrTextDraw(pItem->pText, pCtx->width / 2, pItem->y);
        }

        /* Termination reads the NEXT record's pText before advancing. */
        if (pItem[1].pText == NULL) {
            break;
        }
        pItem++;
    }
}

/* ================================================================== */
/* 0x10016910 / 0x10016990 / 0x100169B0 -- 5-slot LRU cache            */
/* ================================================================== */

void BrLruInit(BrLru *pLru)
{
    int i;

    if (pLru->inited != 0) {
        return;
    }

    pLru->f14   = -1;
    pLru->f5C   = -1;
    pLru->cur   = -1;
    pLru->prev  = -1;
    pLru->prev2 = -1;

    pLru->fA66E8 = 1;
    pLru->f3C    = 0;
    pLru->f0C    = 0;
    pLru->f08    = 0;

    for (i = 0; i < BR_LRU_SLOTS; i++) {
        pLru->aLocked[i] = 0;
        pLru->aStamp[i]  = 0;
    }

    pLru->inited = 1;
}

void BrLruShutdown(BrLru *pLru)
{
    if (pLru->inited != 0) {
        pLru->inited = 0;
    }
}

int BrLruAcquire(BrLru *pLru)
{
    int      cur = pLru->cur;
    int      best = -1;
    uint32_t bestStamp = 0xFFFFFFFFu;
    uint32_t base;
    int      i;

    for (i = 0; i < BR_LRU_SLOTS; i++) {
        uint32_t stamp;

        if (pLru->aLocked[i] != 0) {
            continue;
        }
        if (i == cur) {
            continue;
        }
        stamp = pLru->aStamp[i];
        /* `jb` -- UNSIGNED. Ties are taken (>=), so the later index wins. */
        if (bestStamp < stamp) {
            continue;
        }
        best      = i;
        bestStamp = stamp;
    }

    base = (cur >= 0) ? pLru->aStamp[cur] : 0u;

    pLru->prev2 = pLru->prev;
    pLru->prev  = cur;
    pLru->cur   = best;

    /* DEVIATION: with best == -1 the original still executes
     * `mov [edx + 0x1039B760], ecx` with edx = -1 * 0x2E0F0, scribbling
     * 0x2E0F0 bytes below the slot array. Suppressed here; the -1 is still
     * stored to `cur` and returned, so callers see the same state. */
    if (best >= 0) {
        pLru->aStamp[best] = base + 1u;
    }

    return best;
}

/* ================================================================== */
/* 0x100147B0 -- draw the model-lights marker at the head of the path  */
/* ================================================================== */
/*
 * Glide twin 0x10011D20 (byte-identical structure). One of the race
 * render frontier's draw functions (a direct callee of 0x10011FA0).
 *
 * Gated twice: by g_brRaceHudB (a render toggle) and by the AI path
 * root BrG_6C7CB8 (0x106C7CB8, glide 0x106EED48 == track header +0x70)
 * existing. It builds a Z-up orientation frame M at the first path
 * point pts[0]:
 *
 *   fwd  (row 1) = BrVec3Direction(pts[0].pos -> pts[0].f0C vec)
 *   up   (row 2) = (0, 0, 1)
 *   right(row 0) = fwd x up
 *   fwd  (row 1) = up x right           (re-orthogonalise)
 *   transl(row 3)= pts[0].pos, with z lifted by g_brRaceFade and the
 *                  .rdata constant 0x1008F2D4 == -2.0 (so z += fade + 2),
 *                  then pulled back 0.3*right and 0.6*fwd.
 *
 * Two G_MTX display-list commands are emitted through the command
 * cursor BrG_6C0680 -- the base modelview (the float identity carried
 * by BrG_0AA730) and the current projection (the pooled slot
 * g_BrMtxSlot) -- exactly as the sibling draw functions do. The frame
 * is then scaled to 1/1024 (g_BrScrProjMat as scratch), folded into M
 * (M = scale * M; BrMat4Mul's output is arg3, and it copies through a
 * temp because pB == pOut), and the modelLights prop list is drawn
 * through M.
 *
 * NOTE the aliasing the range comment below used to flag: the three
 * basis rows and the translation all live in the one 0x40-byte M, and
 * 0x1003ADA0 is BrVec3Direction (slice2_21.c) -- both now resolved.
 */

/* 0x10680944 (glide 0x105BCAEC) -- holds the pointer to the modelLights
 * prop list seeded from misc\\modelLights.blob (br_racebegin.c). NULL
 * until that load runs; BrScenePropsDraw is handed whatever it holds. */
BrPropList *g_BrModelLights;

/* The globals this draw shares with the gfx/race packets. Declared here
 * in the file's local style (cf. g_BrScrProjMat) rather than dragging in
 * their whole headers. */
extern int32_t   g_brRaceHudB;   /* 0x105CCB78 (d3d 0x106909D0) render toggle  */
extern float     g_brRaceFade;   /* 0x105BC764 (d3d 0x106805BC) per-frame fade */
extern void     *BrG_0AA730;     /* 0x100AA730 float identity modelview        */
extern uint32_t *BrG_6C0680;     /* 0x106C0680 display-list write cursor       */
extern void     *g_BrMtxSlot;    /* 0x106C32D0 current projection pool slot     */
void BrVec3Direction(BrVec3 *pOut, const BrVec3 *pFrom, const BrVec3 *pTo);
void BrMat4Mul(const BrMat4 *pA, const BrMat4 *pB, BrMat4 *pOut);

void BrModelLightsDraw(void)
{
    BrNode   *pNode;
    BrMat4    M;
    uint32_t *pCmd;

    if (g_brRaceHudB == 0) {
        return;
    }
    pNode = BrG_6C7CB8;
    if (pNode == NULL) {
        return;
    }

    /* The w column is (0,0,0,1); up is seeded before the crosses run. */
    M.m[0][3] = 0.0f;
    M.m[1][3] = 0.0f;
    M.m[2][0] = 0.0f;
    M.m[2][1] = 0.0f;
    M.m[2][2] = 1.0f;
    M.m[2][3] = 0.0f;
    M.m[3][3] = 1.0f;

    BrVec3Direction((BrVec3 *)M.m[1], &pNode->pts[0].pos,
                    (const BrVec3 *)&pNode->pts[0].f0C);
    BrVec3Cross((BrVec3 *)M.m[0], (const BrVec3 *)M.m[1],
                (const BrVec3 *)M.m[2]);
    BrVec3Cross((BrVec3 *)M.m[1], (const BrVec3 *)M.m[2],
                (const BrVec3 *)M.m[0]);

    M.m[3][0] = pNode->pts[0].pos.x;
    M.m[3][1] = pNode->pts[0].pos.y;
    /* fld z; fadd fade; fsub (-2.0) -- association order preserved. */
    M.m[3][2] = (pNode->pts[0].pos.z + g_brRaceFade) - (-2.0f);
    BrVec3MulAdd((BrVec3 *)M.m[3], (const BrVec3 *)M.m[3],
                 (const BrVec3 *)M.m[0], -0.3f);
    BrVec3MulAdd((BrVec3 *)M.m[3], (const BrVec3 *)M.m[3],
                 (const BrVec3 *)M.m[1], -0.6f);

    pCmd = BrG_6C0680;
    BrG_6C0680 += 2;
    pCmd[0] = 0x01060040u;                                  /* G_MTX push|load */
    pCmd[1] = (uint32_t)(uintptr_t)BrG_0AA730;              /* base modelview  */

    pCmd = BrG_6C0680;
    BrG_6C0680 += 2;
    pCmd[0] = 0x01030040u;                                  /* G_MTX load proj */
    pCmd[1] = (uint32_t)(uintptr_t)g_BrMtxSlot;

    BrMat4Scale(&g_BrScrProjMat, 1.0f / 1024.0f, 1.0f / 1024.0f,
                1.0f / 1024.0f);
    BrMat4Mul(&g_BrScrProjMat, &M, &M);                     /* M = scale * M   */

    BrScenePropsDraw(g_BrModelLights, &M);
}

/* ================================================================== */
/* 0x10011EA0 (glide) / 0x10014930 (d3d) -- the FPS readout.          */
/* ================================================================== */

void    *g_BrFpsGuard;                  /* glide 0x10B73538 */
int32_t  g_BrFpsGateA;                  /* glide 0x100A935C */
int32_t  g_BrFpsCountA;                 /* glide 0x100A9358 */
int32_t *g_BrFpsSamplesA;               /* glide 0x105BC900 */
float    g_BrFpsValueA;                 /* glide 0x100A64B0 */
int32_t  g_BrFpsGateB;                  /* glide 0x100B4C2C */
int32_t  g_BrFpsCountB;                 /* glide 0x100B4C28 */
int32_t *g_BrFpsSamplesB;               /* glide 0x10B73348 */
float    g_BrFpsValueB;                 /* glide 0x100A64B4 */
int32_t  g_BrFpsScreenW;                /* glide 0x100A7514 */
int32_t  g_BrFpsScreenH;                /* glide 0x100A7518 */

/* @implements 0x10011EA0 glide BrFpsReadout */
void BrFpsReadout(void)
{
    char buf[0x108];
    int x;

    if (g_BrFpsGuard == NULL)
        return;

    if (g_BrFpsGateA == 0 && g_BrFpsCountA > 0) {
        double sum = 0.0;
        int i;
        for (i = 0; i < g_BrFpsCountA; ++i)
            sum += g_BrFpsSamplesA[i];
        g_BrFpsValueA = (float)((double)g_BrFpsCountA * 1000.0 / sum);
    }

    if (g_BrFpsGateB == 0 && g_BrFpsCountB > 0) {
        double sum = 0.0;
        int i;
        for (i = 0; i < g_BrFpsCountB; ++i)
            sum += g_BrFpsSamplesB[i];
        g_BrFpsValueB = (float)((double)g_BrFpsCountB * 1000.0 / sum);
    }

    sprintf(buf, "%6.2f FPS", (double)g_BrFpsValueB);
    BrTextSetSize(0x0F);
    BrTextAlignCentre();
    x = (g_BrFpsScreenW / 2);
    BrTextDraw(buf, x, g_BrFpsScreenH - 10);
}

/* ================================================================== */
/* Skipped, and why                                                    */
/* ================================================================== */
/*
 * 0x10010DC0 (1991 bytes) -- the frame's visibility/sort pass. Touches ~25
 *   distinct globals (0x10364000 cell table, 0x102E5EC0/5F24/5ED0/5ED8,
 *   0x1038B4C0 flag bytes, 0x10362F28 id list, the 0x2B68-stride entity
 *   array), calls out to 0x1007E2A0 (qsort), 0x10060780, 0x1003A950,
 *   0x10002E90/0x10002F40 and 0x1003AEE0/0x1003AC90. Rendering it would mean
 *   inventing a state object for the whole subsystem, and several of the
 *   flag globals are shared with other passes' packets. Left out on purpose.
 *   Its two leaf helpers, 0x10010D90 and 0x10010BF0, ARE done above.
 *
 * 0x10013A40, 0x10013D90, 0x100140E0, 0x10014450 (~840 bytes each) -- F3D
 *   display-list emitters. They append pairs of dwords through the global
 *   command cursor 0x106C0680, with literal opcodes (0xE7000000, 0xBA001402,
 *   0xB900031D, 0xB7000000, 0xB6000000, 0x01030040, 0x01060040, 0xF2002002,
 *   0xBB000001, 0xBD000000). Transcribing them is mechanical but the output
 *   is only meaningful against a command-buffer type that does not exist in
 *   port/include yet, and 0x100b36f8 / 0x106c32d0 / 0x11829108 are all
 *   cross-packet. These want a single owner for the F3D emitter state.
 *
 * 0x10014930 (252 bytes) -- the FPS readout.  DONE above as BrFpsReadout,
 *   tagged with the Glide address 0x10011EA0.  The timer globals it uses
 *   are declared in slice2_14.h under their Glide addresses.
 */
