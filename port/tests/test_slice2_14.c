/* test_slice2_14.c -- behaviour tests for the slice2_14 packet.
 *
 * These assert properties the ORIGINAL has (clamps, orderings, sentinel
 * values, comparator identities), not merely the values this port happens to
 * produce. Where a value is asserted exactly, the inputs are chosen so that
 * every intermediate is exactly representable in binary32.
 */
#include "slice2_14.h"
#include "slice2_17.h"   /* BrPropList */
#include "slice3_40.h"   /* BrNode, BrPathPoint */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static int g_fails;

#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);           \
            g_fails++;                                                       \
        }                                                                    \
    } while (0)

/* ================================================================== */
/* Stand-ins for the cross-slice callees of BrHudDrawText.             */
/* CLEARLY MARKED: these are TEST STUBS, not decompiled code.          */
/* ================================================================== */

typedef struct DrawRec { const char *pText; int x, y, size; } DrawRec;

static DrawRec g_draws[16];
static int     g_nDraws;
static int     g_nColor6;
static int     g_nFlagClear;
static int     g_nAlignCentre;
static int     g_clearArgs[4];
static int     g_nClear;
static int     g_pendingSize;

void BrTextSetColor6(int a, int b, int c, int d, int e, int f);
void BrTextFlag358Clear(void);
void BrTextAlignCentre(void);
void BrSub1003289F(int a, int b, int c, int d);
void BrTextSetSize(int size);
void BrTextDraw(const char *pText, int x, int y);

void BrTextSetColor6(int a, int b, int c, int d, int e, int f)
{
    CHECK(a == 0xff && b == 0xff && c == 0xff);
    CHECK(d == 0xff && e == 0xff && f == 0xff);
    g_nColor6++;
}
void BrTextFlag358Clear(void)   { g_nFlagClear++; }
void BrTextAlignCentre(void)    { g_nAlignCentre++; }
void BrSub1003289F(int a, int b, int c, int d)
{
    g_clearArgs[0] = a; g_clearArgs[1] = b;
    g_clearArgs[2] = c; g_clearArgs[3] = d;
    g_nClear++;
}
void BrTextSetSize(int size)    { g_pendingSize = size; }
void BrTextDraw(const char *pText, int x, int y)
{
    if (g_nDraws < (int)(sizeof g_draws / sizeof g_draws[0])) {
        g_draws[g_nDraws].pText = pText;
        g_draws[g_nDraws].x     = x;
        g_draws[g_nDraws].y     = y;
        g_draws[g_nDraws].size  = g_pendingSize;
    }
    g_nDraws++;
}

/* ================================================================== */
/* 0x10010B00                                                          */
/* ================================================================== */

static void test_lerp_node(void)
{
    BrLerpNode  n0, n1, from, to;
    float       aFrom[8], aTo[8];
    BrLerpNode *p;
    int         i;

    for (i = 0; i < 8; i++) {
        aFrom[i] = (float)(i * 2);        /* 0,2,4,...  */
        aTo[i]   = (float)(i * 2 + 8);    /* +8         */
    }
    from.pData = aFrom;
    to.pData   = aTo;

    /* LIFO pop order, and the head advances by exactly one node. */
    n0.pNext = &n1;
    n1.pNext = NULL;
    g_pBrLerpFree = &n0;

    p = BrLerpNodeAlloc(&from, &to, 0.0f);
    CHECK(p == &n0);
    CHECK(g_pBrLerpFree == &n1);
    /* pData must be re-pointed at the node's own storage. */
    CHECK(p->pData == p->data);
    /* GOTCHA under test: t == 0 yields the FIRST argument, not the third as
     * BrVec3Lerp does. */
    for (i = 0; i < 8; i++) {
        CHECK(p->data[i] == aFrom[i]);
    }

    p = BrLerpNodeAlloc(&from, &to, 1.0f);
    CHECK(p == &n1);
    CHECK(g_pBrLerpFree == NULL);
    for (i = 0; i < 8; i++) {
        CHECK(p->data[i] == aTo[i]);
    }

    /* Midpoint: all eight components move together. */
    n0.pNext = NULL;
    g_pBrLerpFree = &n0;
    p = BrLerpNodeAlloc(&from, &to, 0.5f);
    CHECK(p == &n0);
    for (i = 0; i < 8; i++) {
        CHECK(p->data[i] == aFrom[i] + 4.0f);
    }

    /* DEVIATION under test: an exhausted free list yields NULL rather than
     * the original's write through a null pointer. */
    g_pBrLerpFree = NULL;
    CHECK(BrLerpNodeAlloc(&from, &to, 0.5f) == NULL);
}

/* ================================================================== */
/* 0x10010BF0                                                          */
/* ================================================================== */

static void mat_identity(BrMat4 *pM)
{
    int i, j;
    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            pM->m[i][j] = (i == j) ? 1.0f : 0.0f;
        }
    }
}

static void test_keep_nearest(void)
{
    BrMat4     m;
    BrScrPt    aOut[3];
    BrScrPt    in;
    BrDepthRef ref;
    int        aFlags[3];

    memset(&ref, 0, sizeof ref);
    ref.f38 = 1000.0f;

    mat_identity(&m);
    memset(aOut, 0, sizeof aOut);
    memset(aFlags, 0, sizeof aFlags);

    /* Incumbent at 2D distance 3 from (0,0). */
    aOut[1].f0C = 3.0f;
    aOut[1].f10 = 0.0f;

    /* --- rejected: strictly farther --- */
    memset(&in, 0, sizeof in);
    in.f00 = 1.0f; in.f04 = 2.0f; in.f08 = 4.0f;
    in.f0C = 5.0f; in.f10 = 0.0f;
    BrScrPtKeepNearest(&m, aOut, aFlags, 1, &in, 0.0f, 0.0f, &ref);
    CHECK(aFlags[1] == 0);
    CHECK(aOut[1].f0C == 3.0f);
    CHECK(aOut[1].f00 == 0.0f);

    /* --- rejected: exactly equal distance keeps the incumbent --- */
    in.f0C = -3.0f; in.f10 = 0.0f;
    BrScrPtKeepNearest(&m, aOut, aFlags, 1, &in, 0.0f, 0.0f, &ref);
    CHECK(aFlags[1] == 0);
    CHECK(aOut[1].f0C == 3.0f);

    /* --- accepted: strictly nearer --- */
    in.f0C = 1.0f; in.f10 = 0.0f;
    BrScrPtKeepNearest(&m, aOut, aFlags, 1, &in, 0.0f, 0.0f, &ref);
    CHECK(aFlags[1] == 1);
    CHECK(aOut[1].f0C == 1.0f);
    CHECK(aOut[1].f10 == 0.0f);
    /* Identity transform, so the position passes through unchanged. */
    CHECK(aOut[1].f00 == 1.0f);
    CHECK(aOut[1].f04 == 2.0f);
    CHECK(aOut[1].f08 == 4.0f);
    /* Neighbouring records and flags are untouched: the index scale is
     * sizeof(BrScrPt) == 0x20, not something else. */
    CHECK(aFlags[0] == 0 && aFlags[2] == 0);
    CHECK(aOut[0].f00 == 0.0f && aOut[2].f00 == 0.0f);
}

static void test_keep_nearest_depth_guard(void)
{
    BrMat4     m;
    BrScrPt    aOut[1];
    BrScrPt    in;
    BrDepthRef ref;
    int        aFlags[1];

    mat_identity(&m);
    memset(&ref, 0, sizeof ref);

    /* limit == f38 - BR_DEPTH_BIAS == f38 + 1. Identity => oz == in.f08. */
    ref.f38 = 9.0f;

    /* On the boundary: accepted (the guard is >=). */
    memset(aOut, 0, sizeof aOut);
    memset(aFlags, 0, sizeof aFlags);
    aOut[0].f0C = 4.0f;
    memset(&in, 0, sizeof in);
    in.f08 = 10.0f;
    BrScrPtKeepNearest(&m, aOut, aFlags, 0, &in, 0.0f, 0.0f, &ref);
    CHECK(aFlags[0] == 1);
    CHECK(aOut[0].f08 == 10.0f);

    /* Just past it: rejected, and nothing at all is written. */
    memset(aOut, 0, sizeof aOut);
    memset(aFlags, 0, sizeof aFlags);
    aOut[0].f0C = 4.0f;
    in.f08 = 10.5f;
    BrScrPtKeepNearest(&m, aOut, aFlags, 0, &in, 0.0f, 0.0f, &ref);
    CHECK(aFlags[0] == 0);
    CHECK(aOut[0].f08 == 0.0f);
    CHECK(aOut[0].f0C == 4.0f);
}

static void test_keep_nearest_transform(void)
{
    BrMat4     m;
    BrScrPt    aOut[1];
    BrScrPt    in;
    BrDepthRef ref;
    int        aFlags[1];

    /* Small exact integers so every product and sum is exact. */
    m.m[0][0] =  2.0f; m.m[0][1] =  3.0f; m.m[0][2] =  5.0f; m.m[0][3] = 0.0f;
    m.m[1][0] =  7.0f; m.m[1][1] = 11.0f; m.m[1][2] = 13.0f; m.m[1][3] = 0.0f;
    m.m[2][0] = 17.0f; m.m[2][1] = 19.0f; m.m[2][2] = 23.0f; m.m[2][3] = 0.0f;
    m.m[3][0] = 29.0f; m.m[3][1] = 31.0f; m.m[3][2] = 37.0f; m.m[3][3] = 1.0f;

    memset(&ref, 0, sizeof ref);
    ref.f38 = 200.0f;
    memset(aOut, 0, sizeof aOut);
    memset(aFlags, 0, sizeof aFlags);
    aOut[0].f0C = 3.0f;

    memset(&in, 0, sizeof in);
    in.f00 = 1.0f; in.f04 = 2.0f; in.f08 = 4.0f;
    in.f0C = 1.0f; in.f10 = 0.0f;

    BrScrPtKeepNearest(&m, aOut, aFlags, 0, &in, 0.0f, 0.0f, &ref);
    CHECK(aFlags[0] == 1);
    /* Row-vector convention, translation from row 3. */
    CHECK(aOut[0].f00 == 113.0f);   /* 17*4 + 7*2 + 2*1 + 29 */
    CHECK(aOut[0].f04 == 132.0f);   /* 19*4 + 11*2 + 3*1 + 31 */
    CHECK(aOut[0].f08 == 160.0f);   /* 23*4 + 13*2 + 5*1 + 37 */
    /* Row 3 is a translation, so translating the input by a vector shifts
     * the output by that vector transformed by the upper 3x3 -- check the
     * simplest instance: the translation itself is what a zero input gives. */
    {
        BrScrPt zeroIn;
        memset(aOut, 0, sizeof aOut);
        memset(aFlags, 0, sizeof aFlags);
        aOut[0].f0C = 3.0f;
        memset(&zeroIn, 0, sizeof zeroIn);
        zeroIn.f0C = 1.0f;
        BrScrPtKeepNearest(&m, aOut, aFlags, 0, &zeroIn, 0.0f, 0.0f, &ref);
        CHECK(aFlags[0] == 1);
        CHECK(aOut[0].f00 == 29.0f);
        CHECK(aOut[0].f04 == 31.0f);
        CHECK(aOut[0].f08 == 37.0f);
    }
}

/* ================================================================== */
/* 0x10010D10                                                          */
/* ================================================================== */

static void test_project(void)
{
    BrScrPt p;

    mat_identity(&g_BrScrProjMat);
    memset(&p, 0, sizeof p);
    p.f00 = 6.0f; p.f04 = -7.0f; p.f08 = 8.0f;
    p.f0C = 99.0f; p.f10 = 99.0f;
    BrScrPtProject(&p);
    CHECK(p.f0C == 6.0f);
    CHECK(p.f10 == -7.0f);
    /* The position must be left alone -- the routine is a projection, not a
     * transform-in-place. */
    CHECK(p.f00 == 6.0f && p.f04 == -7.0f && p.f08 == 8.0f);

    /* Same matrix and convention as BrScrPtKeepNearest: the two 2D outputs
     * must equal the first two components of the row-vector transform. */
    g_BrScrProjMat.m[0][0] =  2.0f; g_BrScrProjMat.m[0][1] =  3.0f;
    g_BrScrProjMat.m[1][0] =  7.0f; g_BrScrProjMat.m[1][1] = 11.0f;
    g_BrScrProjMat.m[2][0] = 17.0f; g_BrScrProjMat.m[2][1] = 19.0f;
    g_BrScrProjMat.m[3][0] = 29.0f; g_BrScrProjMat.m[3][1] = 31.0f;

    p.f00 = 1.0f; p.f04 = 2.0f; p.f08 = 4.0f;
    BrScrPtProject(&p);
    CHECK(p.f0C == 113.0f);
    CHECK(p.f10 == 132.0f);
}

/* ================================================================== */
/* 0x10010D90                                                          */
/* ================================================================== */

static int sgn(int v) { return (v > 0) - (v < 0); }

static void test_sort_cell(void)
{
    static const short keys[] = { 0, 1, -1, 300, -300, 32767, -32768, 7 };
    const int n = (int)(sizeof keys / sizeof keys[0]);
    BrSortCell a, b, arr[8];
    int i, j;

    /* Exactly 1 / 0 / -1, and antisymmetric -- the two properties any user
     * of qsort() relies on. */
    for (i = 0; i < n; i++) {
        for (j = 0; j < n; j++) {
            int r, rr;
            a.c00 = 0xAA; a.c01 = 0xBB; a.key = keys[i];
            b.c00 = 0x11; b.c01 = 0x22; b.key = keys[j];
            r  = BrSortCellCompare(&a, &b);
            rr = BrSortCellCompare(&b, &a);
            CHECK(r == 1 || r == 0 || r == -1);
            CHECK(sgn(r) == -sgn(rr));
            /* The other two bytes must not participate. */
            CHECK(sgn(r) == sgn((keys[i] > keys[j]) - (keys[i] < keys[j])));
        }
    }

    /* And it really does sort ascending, negatives included (the key is
     * signed -- an unsigned read would put -1 last). */
    for (i = 0; i < n; i++) {
        arr[i].c00 = (unsigned char)i;
        arr[i].c01 = 0;
        arr[i].key = keys[i];
    }
    qsort(arr, (size_t)n, sizeof arr[0], BrSortCellCompare);
    for (i = 1; i < n; i++) {
        CHECK(arr[i - 1].key <= arr[i].key);
    }
    CHECK(arr[0].key == -32768);
    CHECK(arr[n - 1].key == 32767);
}

/* ================================================================== */
/* 0x10013A10                                                          */
/* ================================================================== */

static void test_raster_select(void)
{
    struct { BrRasterHdr h; unsigned char payload[8]; } blob;

    blob.h.f00 = 0x1111;
    blob.h.f02 = 0x2222;
    blob.h.f04 = 0x3333;
    blob.h.f06 = 0x4444;

    memset(&g_BrRaster, 0, sizeof g_BrRaster);
    BrRasterSelect(&blob.h);

    /* GOTCHA under test: the mapping is crossed. */
    CHECK(g_BrRaster.f10 == 0x3333);   /* <- +0x04 */
    CHECK(g_BrRaster.f14 == 0x2222);   /* <- +0x02 */
    CHECK(g_BrRaster.f18 == 0x4444);   /* <- +0x06 */
    CHECK(g_BrRaster.p1C == (const void *)((const unsigned char *)&blob.h + 8));
    CHECK(g_BrRaster.p1C == (const void *)blob.payload);
}

/* ================================================================== */
/* 0x10015BD0                                                          */
/* ================================================================== */

static void hud_reset(void)
{
    g_nDraws = 0;
    g_nColor6 = 0;
    g_nFlagClear = 0;
    g_nAlignCentre = 0;
    g_nClear = 0;
    g_pendingSize = -1;
    memset(g_draws, 0, sizeof g_draws);
}

static void test_hud(void)
{
    static const int32_t clear[4] = { 10, 20, 30, 40 };
    BrTextItem items[6];
    BrHudCtx   ctx;

    memset(items, 0, sizeof items);
    ctx.aItems = items;
    ctx.yLimit = 200;               /* upper cut is yLimit + 0x28 == 240 */
    ctx.width  = 641;

    items[0].y = BR_HUD_Y_MIN;      /* -80: excluded (jle) */
    items[0].size = 1; items[0].pText = "a";
    items[1].y = BR_HUD_Y_MIN + 1;  /* -79: included */
    items[1].size = 2; items[1].pText = "b";
    items[2].y = 240;               /* == yLimit+0x28: excluded (jge) */
    items[2].size = 3; items[2].pText = "c";
    items[3].y = 239;               /* included */
    items[3].size = 4; items[3].pText = "d";
    items[4].y = 0;                 /* included */
    items[4].size = 5; items[4].pText = "e";
    items[5].pText = NULL;          /* terminator */

    hud_reset();
    BrHudDrawText(&ctx, clear);

    CHECK(g_nColor6 == 1);
    CHECK(g_nFlagClear == 1);
    CHECK(g_nAlignCentre == 1);
    CHECK(g_nClear == 1);
    CHECK(g_clearArgs[0] == 10 && g_clearArgs[1] == 20);
    CHECK(g_clearArgs[2] == 30 && g_clearArgs[3] == 40);

    CHECK(g_nDraws == 3);
    if (g_nDraws == 3) {
        CHECK(strcmp(g_draws[0].pText, "b") == 0 && g_draws[0].y == -79);
        CHECK(strcmp(g_draws[1].pText, "d") == 0 && g_draws[1].y == 239);
        CHECK(strcmp(g_draws[2].pText, "e") == 0 && g_draws[2].y == 0);
        /* Each item's own size is latched immediately before its draw. */
        CHECK(g_draws[0].size == 2);
        CHECK(g_draws[1].size == 4);
        CHECK(g_draws[2].size == 5);
        /* x is width/2, truncating (641/2 == 320). */
        CHECK(g_draws[0].x == 320);
    }

    /* Truncation toward zero for a negative width (sar-after-cdq, not sar). */
    hud_reset();
    ctx.width = -641;
    BrHudDrawText(&ctx, clear);
    CHECK(g_nDraws == 3);
    if (g_nDraws == 3) {
        CHECK(g_draws[0].x == -320);
    }

    /* A first item with no text draws nothing -- but the colour/mode/clear
     * calls still happen, because the early-out sits after them. */
    hud_reset();
    ctx.width = 640;
    items[0].pText = NULL;
    BrHudDrawText(&ctx, clear);
    CHECK(g_nDraws == 0);
    CHECK(g_nColor6 == 1 && g_nFlagClear == 1);
    CHECK(g_nAlignCentre == 1 && g_nClear == 1);
}

/* ================================================================== */
/* 0x10016910 / 0x10016990 / 0x100169B0                                */
/* ================================================================== */

static void test_lru(void)
{
    BrLru lru;
    int   i, r;

    memset(&lru, 0xCD, sizeof lru);
    lru.inited = 0;
    BrLruInit(&lru);

    /* -1, not 0, is the "none" marker. */
    CHECK(lru.cur == -1 && lru.prev == -1 && lru.prev2 == -1);
    CHECK(lru.f14 == -1 && lru.f5C == -1);
    CHECK(lru.fA66E8 == 1);
    CHECK(lru.f08 == 0 && lru.f0C == 0 && lru.f3C == 0);
    for (i = 0; i < BR_LRU_SLOTS; i++) {
        CHECK(lru.aLocked[i] == 0);
        CHECK(lru.aStamp[i] == 0u);
    }
    CHECK(lru.inited == 1);

    /* Init is one-shot: a second call must change nothing. */
    lru.cur = 7;
    BrLruInit(&lru);
    CHECK(lru.cur == 7);
    lru.cur = -1;

    /* All stamps tie at 0, so the LAST index wins; cur was -1, so the base
     * stamp is 0 and the new stamp is 1. */
    r = BrLruAcquire(&lru);
    CHECK(r == BR_LRU_SLOTS - 1);
    CHECK(lru.cur == BR_LRU_SLOTS - 1);
    CHECK(lru.prev == -1 && lru.prev2 == -1);
    CHECK(lru.aStamp[BR_LRU_SLOTS - 1] == 1u);

    /* The current slot is never re-picked, and the history rotates. */
    r = BrLruAcquire(&lru);
    CHECK(r == BR_LRU_SLOTS - 2);
    CHECK(lru.prev == BR_LRU_SLOTS - 1);
    CHECK(lru.prev2 == -1);
    CHECK(lru.aStamp[BR_LRU_SLOTS - 2] == 2u);

    r = BrLruAcquire(&lru);
    CHECK(r == BR_LRU_SLOTS - 3);
    CHECK(lru.prev == BR_LRU_SLOTS - 2);
    CHECK(lru.prev2 == BR_LRU_SLOTS - 1);
    CHECK(lru.aStamp[BR_LRU_SLOTS - 3] == 3u);

    /* Stamps must stay strictly increasing over a long run, and the picked
     * slot must never be the one that was current. */
    {
        uint32_t last = lru.aStamp[lru.cur];
        for (i = 0; i < 40; i++) {
            int before = lru.cur;
            r = BrLruAcquire(&lru);
            CHECK(r >= 0 && r < BR_LRU_SLOTS);
            CHECK(r != before);
            CHECK(lru.aStamp[r] == last + 1u);
            last = lru.aStamp[r];
        }
    }

    /* Locked slots are skipped. Lock everything except one non-current slot
     * and that slot must be chosen every time. */
    BrLruShutdown(&lru);
    CHECK(lru.inited == 0);
    BrLruInit(&lru);
    CHECK(lru.inited == 1);

    lru.cur = 0;
    for (i = 0; i < BR_LRU_SLOTS; i++) {
        lru.aLocked[i] = (i == 2) ? 0 : 1;
    }
    r = BrLruAcquire(&lru);
    CHECK(r == 2);
    lru.cur = 0;
    r = BrLruAcquire(&lru);
    CHECK(r == 2);

    /* Nothing selectable: the original would scribble at base - 0x2E0F0.
     * The port returns -1 and leaves every stamp alone (DEVIATION). */
    BrLruShutdown(&lru);
    BrLruInit(&lru);
    lru.cur = 3;
    for (i = 0; i < BR_LRU_SLOTS; i++) {
        lru.aLocked[i] = 1;
        lru.aStamp[i]  = 0x5000u + (uint32_t)i;
    }
    r = BrLruAcquire(&lru);
    CHECK(r == -1);
    CHECK(lru.cur == -1);
    CHECK(lru.prev == 3);
    for (i = 0; i < BR_LRU_SLOTS; i++) {
        CHECK(lru.aStamp[i] == 0x5000u + (uint32_t)i);
    }

    /* The stamp comparison is UNSIGNED: a stamp with the top bit set must
     * sort BELOW a small one, so slot 1 wins here even though a signed read
     * would treat it as the most negative. */
    BrLruShutdown(&lru);
    BrLruInit(&lru);
    lru.cur = 0;
    lru.aStamp[1] = 0x80000000u;
    lru.aStamp[2] = 0x00000005u;
    lru.aStamp[3] = 0x00000006u;
    lru.aStamp[4] = 0x00000007u;
    lru.aStamp[0] = 0x00000004u;
    r = BrLruAcquire(&lru);
    CHECK(r == 2);
    CHECK(lru.aStamp[2] == 0x00000005u);   /* base(cur=0)=4, +1 */
}

/* ================================================================== */
/* 0x100147B0 BrModelLightsDraw                                        */
/* ================================================================== */
/* The draw reads six cross-slice globals; defined here so the suite links
 * without dragging in the race/scene/gfx modules, and captures the one
 * BrScenePropsDraw call so its matrix can be checked. The math helpers
 * (BrVec3Direction/Cross/MulAdd, BrMat4Scale/Mul) ARE the real modules --
 * see build.d/test_slice2_14.deps. */
int32_t   g_brRaceHudB;
float     g_brRaceFade;
void     *BrG_0AA730;
uint32_t *BrG_6C0680;
void     *g_BrMtxSlot;
BrNode   *BrG_6C7CB8;

static const BrPropList *s_mlList;
static BrMat4            s_mlMtx;
static int               s_mlCount;

void BrScenePropsDraw(const BrPropList *pList, const BrMat4 *pViewMtx)
{
    s_mlList = pList;
    s_mlMtx  = *pViewMtx;
    s_mlCount++;
}

/* BrVec3Direction (slice2_21.c) and BrMat4Mul (slice1_05.c) live in grab-bag
 * objects that drag the trig/span/pool subsystems into the link. They have
 * their own suites; reproduced here as faithful stubs so this suite stays
 * self-contained. BrVec3Cross/MulAdd (br_vec) and BrMat4Scale (br_mat) ARE
 * the real modules -- see build.d/test_slice2_14.deps. */
void BrVec3Direction(BrVec3 *pOut, const BrVec3 *pFrom, const BrVec3 *pTo)
{
    float dx = pTo->x - pFrom->x;
    float dy = pTo->y - pFrom->y;
    float dz = pTo->z - pFrom->z;
    float len = sqrtf(dx * dx + dy * dy + dz * dz);

    if (len == 0.0f) {
        pOut->x = 0.0f; pOut->y = 0.0f; pOut->z = 1.0f;
        return;
    }
    len = 1.0f / len;
    pOut->x = len * dx; pOut->y = len * dy; pOut->z = len * dz;
}

void BrMat4Mul(const BrMat4 *pA, const BrMat4 *pB, BrMat4 *pOut)
{
    BrMat4 tmp;
    int i, j, k;

    for (i = 0; i < 4; ++i) {
        for (j = 0; j < 4; ++j) {
            float s = 0.0f;
            for (k = 0; k < 4; ++k) {
                s += pA->m[i][k] * pB->m[k][j];
            }
            tmp.m[i][j] = s;
        }
    }
    *pOut = tmp;
}

static int mclose(float a, float b) { return fabsf(a - b) < 1e-5f; }

static void test_model_lights(void)
{
    /* one BrNode + one BrPathPoint of trailing storage (pts is a FAM). */
    unsigned char buf[sizeof(BrNode) + sizeof(BrPathPoint)];
    BrNode      *pNode = (BrNode *)buf;
    BrPropList   list;
    uint32_t     dl[8];
    const float  s = 1.0f / 1024.0f;

    memset(buf, 0, sizeof buf);
    /* pts[0].pos = origin; the +0x0C vec = (1,0,0) so fwd = +X. */
    pNode->pts[0].pos.x = 0.0f;
    pNode->pts[0].pos.y = 0.0f;
    pNode->pts[0].pos.z = 0.0f;
    pNode->pts[0].f0C   = 1.0f;
    pNode->pts[0].f10   = 0.0f;
    pNode->pts[0].f14   = 0.0f;

    BrG_0AA730     = (void *)(uintptr_t)0x100AA730u;
    g_BrMtxSlot    = (void *)(uintptr_t)0x106C32D0u;
    g_BrModelLights = &list;
    g_brRaceFade   = 0.0f;

    /* Gate 1: the toggle off -- nothing drawn, cursor untouched. */
    g_brRaceHudB = 0;
    BrG_6C7CB8   = pNode;
    BrG_6C0680   = dl;
    s_mlCount    = 0;
    BrModelLightsDraw();
    CHECK(s_mlCount == 0);
    CHECK(BrG_6C0680 == dl);

    /* Gate 2: no path root -- still nothing. */
    g_brRaceHudB = 1;
    BrG_6C7CB8   = NULL;
    BrModelLightsDraw();
    CHECK(s_mlCount == 0);
    CHECK(BrG_6C0680 == dl);

    /* Full path: two G_MTX commands emitted, then one props draw. */
    g_brRaceHudB = 1;
    BrG_6C7CB8   = pNode;
    BrG_6C0680   = dl;
    BrModelLightsDraw();

    CHECK(BrG_6C0680 == dl + 4);               /* two 8-byte commands       */
    CHECK(dl[0] == 0x01060040u);               /* G_MTX push|load modelview */
    CHECK(dl[1] == 0x100AA730u);               /* identity payload (low 32)  */
    CHECK(dl[2] == 0x01030040u);               /* G_MTX load projection      */
    CHECK(dl[3] == 0x106C32D0u);               /* pool-slot payload (low 32) */

    CHECK(s_mlCount == 1);
    CHECK(s_mlList == &list);

    /* M = scale(1/1024) * frame. Frame basis at the origin point:
     *   right = fwd x up = (0,-1,0), fwd = up x right = (1,0,0), up = +Z.
     * The top three rows are scaled; the translation row is not:
     *   transl = (0,0,0+fade+2) - 0.3*right - 0.6*fwd = (-0.6, 0.3, 2). */
    CHECK(mclose(s_mlMtx.m[0][0], 0.0f) && mclose(s_mlMtx.m[0][1], -s) &&
          mclose(s_mlMtx.m[0][2], 0.0f) && mclose(s_mlMtx.m[0][3], 0.0f));
    CHECK(mclose(s_mlMtx.m[1][0],  s)   && mclose(s_mlMtx.m[1][1], 0.0f) &&
          mclose(s_mlMtx.m[1][2], 0.0f) && mclose(s_mlMtx.m[1][3], 0.0f));
    CHECK(mclose(s_mlMtx.m[2][0], 0.0f) && mclose(s_mlMtx.m[2][1], 0.0f) &&
          mclose(s_mlMtx.m[2][2],  s)   && mclose(s_mlMtx.m[2][3], 0.0f));
    CHECK(mclose(s_mlMtx.m[3][0], -0.6f) && mclose(s_mlMtx.m[3][1], 0.3f) &&
          mclose(s_mlMtx.m[3][2],  2.0f) && mclose(s_mlMtx.m[3][3], 1.0f));
}

/* ================================================================== */
/* BrFpsReadout (0x10011EA0 glide / 0x10014930 d3d)                    */
/* ================================================================== */

static void test_fps_guard(void)
{
    g_BrFpsGuard = NULL;
    g_nDraws = 0;
    BrFpsReadout();
    CHECK(g_nDraws == 0);
}

static void test_fps_computation(void)
{
    int32_t samples[4] = { 100, 100, 100, 100 };

    g_BrFpsGuard    = &g_BrFpsGuard;
    g_BrFpsGateA    = 0;
    g_BrFpsCountA   = 4;
    g_BrFpsSamplesA = samples;
    g_BrFpsValueA   = 0.0f;
    g_BrFpsGateB    = 0;
    g_BrFpsCountB   = 4;
    g_BrFpsSamplesB = samples;
    g_BrFpsValueB   = 0.0f;
    g_BrFpsScreenW  = 640;
    g_BrFpsScreenH  = 480;
    g_nDraws        = 0;

    BrFpsReadout();

    CHECK(fabsf(g_BrFpsValueA - 10.0f) < 0.01f);
    CHECK(fabsf(g_BrFpsValueB - 10.0f) < 0.01f);
    CHECK(g_nDraws == 1);
    CHECK(g_draws[0].x == 320);
    CHECK(g_draws[0].y == 470);
    CHECK(g_draws[0].size == 0x0F);
}

static void test_fps_gate_skips(void)
{
    int32_t samples[2] = { 50, 50 };

    g_BrFpsGuard    = &g_BrFpsGuard;
    g_BrFpsGateA    = 1;
    g_BrFpsGateB    = 1;
    g_BrFpsCountA   = 2;
    g_BrFpsCountB   = 2;
    g_BrFpsSamplesA = samples;
    g_BrFpsSamplesB = samples;
    g_BrFpsValueA   = 99.0f;
    g_BrFpsValueB   = 99.0f;
    g_BrFpsScreenW  = 640;
    g_BrFpsScreenH  = 480;
    g_nDraws        = 0;

    BrFpsReadout();

    CHECK(g_BrFpsValueA == 99.0f);
    CHECK(g_BrFpsValueB == 99.0f);
    CHECK(g_nDraws == 1);
}

int main(void)
{
    test_lerp_node();
    test_keep_nearest();
    test_keep_nearest_depth_guard();
    test_keep_nearest_transform();
    test_project();
    test_sort_cell();
    test_raster_select();
    test_hud();
    test_lru();
    test_model_lights();
    test_fps_guard();
    test_fps_computation();
    test_fps_gate_skips();

    if (g_fails != 0) {
        printf("%d FAILURES\n", g_fails);
        return 1;
    }
    printf("test_slice2_14: all checks passed\n");
    return 0;
}
