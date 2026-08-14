/* test_slice4_51.c -- behaviour tests for another module's three functions.
 *
 * Everything asserted here is either an identity the original's arithmetic
 * guarantees (a rectangle stays a rectangle; halving the divisor halves
 * every output; the two winding branches are exact reverses of each other)
 * or a boundary the original actually contains (the equality test against
 * DPERR_BUFFERTOOSMALL, the untouched out-parameter on failure, the
 * dirty-mask guard).  No test hardcodes a value the code merely happens to
 * produce today.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "slice4_51.h"
#include "slice2_13.h"
#include "slice2_16.h"

static int g_cFail;

#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);           \
            ++g_cFail;                                                       \
        }                                                                    \
    } while (0)

/* =====================================================================
 * Stand-ins for the cross-slice callees.  TEST ONLY.
 * ===================================================================== */

int32_t BrG_0A81C0 = 640;   /* 0x100A81C0 -- the shipped image's value */
int32_t BrG_0A81C4 = 480;   /* 0x100A81C4 */

/* --- slice3_39's 0x1005FF60 / 0x1005FFF0 --------------------------- */
static int g_aMenuOrder[4];
static int g_cMenuOrder;

void BrMenuSub1005FF60(void)
{
    if (g_cMenuOrder < 4)
        g_aMenuOrder[g_cMenuOrder++] = 0x60;
}

void BrMenuSub1005FFF0(void)
{
    if (g_cMenuOrder < 4)
        g_aMenuOrder[g_cMenuOrder++] = 0xF0;
}

/* --- slice2_13's state, for pfnAlloc / pfnFree --------------------- */
static BrDPlayState g_dp;
static int          g_cAlloc, g_cFree;
static uint32_t     g_cbAsked;
static int          g_fAllocFails;
static void        *g_pvLastFreed;

static void *TestAlloc(uint32_t cb)
{
    g_cbAsked = cb;
    if (g_fAllocFails)
        return NULL;
    ++g_cAlloc;
    return calloc(1, cb ? cb : 1);
}

static void TestFree(void *pv)
{
    ++g_cFree;
    g_pvLastFreed = pv;
    free(pv);
}

BrDPlayState *BrDPlayGetState(void)
{
    return &g_dp;
}

/* --- 0x1001D420 ---------------------------------------------------- */
#define MAX_EMIT 16
static BrClipVert g_aEmit[MAX_EMIT];
static int        g_cEmit;

void BrGbiCall1001D420(BrGbiRectVert *pA, BrGbiRectVert *pB,
                       BrGbiRectVert *pC)
{
    BrGbiRectVert *ap[3];
    int i;

    ap[0] = pA; ap[1] = pB; ap[2] = pC;
    for (i = 0; i < 3; ++i) {
        if (g_cEmit < MAX_EMIT)
            g_aEmit[g_cEmit++] = ap[i]->node;
    }
}

/* =====================================================================
 * 1. 0x1003E070
 * ===================================================================== */

static void test_3E070(void)
{
    g_cMenuOrder = 0;
    BrFn1003E070();

    /* `call 0x1005FF60` then `jmp 0x1005FFF0`: both run, in that order. */
    CHECK(g_cMenuOrder == 2);
    CHECK(g_aMenuOrder[0] == 0x60);
    CHECK(g_aMenuOrder[1] == 0xF0);
}

/* =====================================================================
 * 2. 0x1003D0B0
 * ===================================================================== */

static int32_t  g_hr1, g_hr2;
static uint32_t g_cbReport;
static int      g_cGsd;
static void    *g_pvGsd;      /* the buffer handed to the second call */
static uint32_t g_cbGsd;      /* the size the second call was shown */

static int32_t TestGetSessionDesc(BrDPlay4Obj *pThis, void *pvData,
                                  uint32_t *pcbData)
{
    (void)pThis;
    ++g_cGsd;
    if (pvData == NULL) {
        *pcbData = g_cbReport;
        return g_hr1;
    }
    g_pvGsd = pvData;
    g_cbGsd = *pcbData;
    return g_hr2;
}

static void ResetDp(void)
{
    g_cAlloc = g_cFree = g_cGsd = 0;
    g_fAllocFails = 0;
    g_pvGsd = NULL;
    g_pvLastFreed = NULL;
    g_cbAsked = 0;
    g_cbGsd = 0;
}

static void test_3D0B0(void)
{
    BrDPlay4Vtbl vtbl;
    BrDPlay4Obj  obj;
    void        *pvSentinel = (void *)&vtbl;   /* recognisable non-NULL */
    void        *pv;
    int32_t      hr;

    memset(&vtbl, 0, sizeof vtbl);
    /* byte offset 0x58 == aSlots10[(0x58 - 0x28) / 4] */
    {
        int32_t (*pfn)(BrDPlay4Obj *, void *, uint32_t *) = TestGetSessionDesc;
        void *pvFn;

        memcpy(&pvFn, &pfn, sizeof pvFn);
        vtbl.aSlots10[(0x58 - 0x28) / 4] = pvFn;
    }
    obj.pVtbl = &vtbl;

    g_dp.os.pfnAlloc = TestAlloc;
    g_dp.os.pfnFree  = TestFree;

    /* --- happy path ------------------------------------------------ */
    ResetDp();
    g_hr1 = BR_DP_E_BUFFERTOOSMALL;
    g_hr2 = 0;
    g_cbReport = 0x30;
    pv = pvSentinel;
    hr = BrSub1003D0B0(&obj, &pv);

    CHECK(hr == 0);
    CHECK(g_cGsd == 2);
    CHECK(g_cAlloc == 1);
    CHECK(g_cFree == 0);
    CHECK(g_cbAsked == 0x30);       /* the size the sizing call reported */
    CHECK(g_cbGsd == 0x30);         /* and it is shown to the real call */
    CHECK(pv != pvSentinel);
    CHECK(pv == g_pvGsd);           /* the caller gets the filled buffer */
    free(pv);                       /* the real caller frees via pfnFree */

    /* --- the equality GOTCHA: S_OK from the sizing call bails out --- */
    ResetDp();
    g_hr1 = 0;                      /* not DPERR_BUFFERTOOSMALL */
    pv = pvSentinel;
    hr = BrSub1003D0B0(&obj, &pv);

    CHECK(hr == 0);
    CHECK(g_cGsd == 1);             /* no second call */
    CHECK(g_cAlloc == 0);           /* nothing allocated */
    CHECK(pv == pvSentinel);        /* out-parameter untouched */

    /* --- an ordinary error from the sizing call is passed through --- */
    ResetDp();
    g_hr1 = (int32_t)0x88770020;
    pv = pvSentinel;
    hr = BrSub1003D0B0(&obj, &pv);

    CHECK(hr == (int32_t)0x88770020);
    CHECK(g_cGsd == 1);
    CHECK(g_cAlloc == 0);
    CHECK(pv == pvSentinel);

    /* --- allocation failure ---------------------------------------- */
    ResetDp();
    g_hr1 = BR_DP_E_BUFFERTOOSMALL;
    g_cbReport = 0x30;
    g_fAllocFails = 1;
    pv = pvSentinel;
    hr = BrSub1003D0B0(&obj, &pv);

    CHECK(hr == BR_DP_E_OUTOFMEMORY);
    CHECK(g_cGsd == 1);             /* the second call never happens */
    CHECK(g_cFree == 0);            /* and nothing is freed */
    CHECK(pv == pvSentinel);

    /* --- the second call fails: the buffer is released -------------- */
    ResetDp();
    g_hr1 = BR_DP_E_BUFFERTOOSMALL;
    g_hr2 = (int32_t)0x8007000E;
    g_cbReport = 0x30;
    pv = pvSentinel;
    hr = BrSub1003D0B0(&obj, &pv);

    CHECK(hr == (int32_t)0x8007000E);
    CHECK(g_cAlloc == 1);
    CHECK(g_cFree == 1);
    CHECK(g_pvLastFreed == g_pvGsd);
    CHECK(pv == pvSentinel);        /* still untouched */

    /* --- a non-negative-but-not-zero result still counts as success - */
    ResetDp();
    g_hr1 = BR_DP_E_BUFFERTOOSMALL;
    g_hr2 = 1;
    g_cbReport = 0x10;
    pv = pvSentinel;
    hr = BrSub1003D0B0(&obj, &pv);

    CHECK(hr == 1);
    CHECK(g_cFree == 0);
    CHECK(pv == g_pvGsd);
    free(pv);
}

/* =====================================================================
 * 3. 0x10021560
 * ===================================================================== */

static BrGbiState g_gbi;

static uint32_t FloatBits(float f)
{
    uint32_t u;

    memcpy(&u, &f, sizeof u);
    return u;
}

/* --- a fake IDirect3DDevice2 --------------------------------------- */
#define MAX_RS 32
static uint32_t g_aRsState[MAX_RS], g_aRsValue[MAX_RS];
static int      g_cRs;
static int      g_cDraw;
static uint32_t g_drawArgs[6];
static void    *g_drawVerts;
static uint16_t *g_drawIndices;

static int32_t TestSetRenderState(BrD3DDev2 *pThis, uint32_t s, uint32_t v)
{
    (void)pThis;
    if (g_cRs < MAX_RS) {
        g_aRsState[g_cRs] = s;
        g_aRsValue[g_cRs] = v;
        ++g_cRs;
    }
    return 0;
}

static int32_t TestDrawIndexed(BrD3DDev2 *pThis, uint32_t dpt, uint32_t dvt,
                               void *pv, uint32_t cv, uint16_t *pw,
                               uint32_t ci, uint32_t fl)
{
    (void)pThis;
    ++g_cDraw;
    g_drawArgs[0] = dpt; g_drawArgs[1] = dvt; g_drawArgs[2] = cv;
    g_drawArgs[3] = ci;  g_drawArgs[4] = fl;
    g_drawVerts = pv;
    g_drawIndices = pw;
    return 0;
}

static BrD3DDev2Vtbl g_devVtbl;
static BrD3DDev2     g_dev;

static void SetupGbi(float e, float z)
{
    BrGbiRectState *pSt = BrGbiRectGetState();

    memset(&g_gbi, 0, sizeof g_gbi);
    g_gbi.f0A79E8 = FloatBits(e);
    g_gbi.f4C5174 = FloatBits(z);
    g_gbi.scissor.ulx = 1;
    g_gbi.scissor.uly = 2;
    g_gbi.scissor.lrx = 3;
    g_gbi.scissor.lry = 4;
    g_gbi.light.off[0] = 0.25f;
    g_gbi.light.off[1] = 0.50f;
    g_gbi.light.off[2] = 0.75f;

    memset(pSt, 0, sizeof *pSt);
    pSt->pGbi = &g_gbi;

    g_devVtbl.SetRenderState = TestSetRenderState;
    g_devVtbl.DrawIndexedPrimitive = TestDrawIndexed;
    g_dev.pVtbl = &g_devVtbl;
    pSt->pDev = &g_dev;

    g_cEmit = 0;
    g_cRs = 0;
    g_cDraw = 0;
}

/* The four corners, in the order 0x10021560 builds them. */
enum { V0, V1, V2, V3 };

static void GrabCorners(BrClipVert *av)
{
    /* The emit order for the G_CULL_FRONT branch is v3,v0,v1 / v0,v2,v1. */
    av[V3] = g_aEmit[0];
    av[V0] = g_aEmit[1];
    av[V1] = g_aEmit[2];
    av[V2] = g_aEmit[4];
}

static void test_21560_geometry(void)
{
    BrClipVert av[4];
    BrClipVert aHalf[4];
    int i;

    /* e = 1.0 keeps the divisor out of the way; z is arbitrary. */
    SetupGbi(1.0f, 0.5f);
    g_gbi.geo.cur = 0x1000u;        /* pick a fixed winding branch */

    /* 10.2 fixed point: 0 and 640*4 span the full width, 0 and 480*4 the
     * full height. */
    BrGbiCall10021560(0, 0, 2560, 1920, 7);
    CHECK(g_cEmit == 6);
    GrabCorners(av);

    /* It is a rectangle: two distinct x values, two distinct y values, and
     * the corners pair up. */
    CHECK(av[V0].f04 == av[V2].f04);
    CHECK(av[V1].f04 == av[V3].f04);
    CHECK(av[V0].f08 == av[V3].f08);
    CHECK(av[V1].f08 == av[V2].f08);
    CHECK(av[V0].f04 != av[V1].f04);
    CHECK(av[V0].f08 != av[V1].f08);

    /* The screen edges map onto the NDC cube: x/y of -1 and +1. */
    CHECK(av[V1].f04 == -1.0f);     /* lrs = 0        -> left  */
    CHECK(av[V0].f04 ==  1.0f);     /* uls = 640 * 4  -> right */
    CHECK(av[V0].f08 ==  1.0f);     /* lrt = 0        -> top   */
    CHECK(av[V1].f08 == -1.0f);     /* ult = 480 * 4  -> bottom */

    /* z and w are shared by all four and come from f4C5174 and 1.0f. */
    for (i = 0; i < 4; ++i) {
        CHECK(av[i].f0C == 0.5f);
        CHECK(av[i].f18 == 1.0f);
    }

    /* Doubling the divisor at 0x100A79E8 halves every one of x, y, z and w:
     * it is applied once to each, after everything else. */
    SetupGbi(2.0f, 0.5f);
    g_gbi.geo.cur = 0x1000u;
    BrGbiCall10021560(0, 0, 2560, 1920, 7);
    CHECK(g_cEmit == 6);
    GrabCorners(aHalf);

    for (i = 0; i < 4; ++i) {
        CHECK(aHalf[i].f04 == av[i].f04 / 2.0f);
        CHECK(aHalf[i].f08 == av[i].f08 / 2.0f);
        CHECK(aHalf[i].f0C == av[i].f0C / 2.0f);
        CHECK(aHalf[i].f18 == av[i].f18 / 2.0f);
    }
}

static void test_21560_uv_and_colour(void)
{
    BrGbiRectState *pSt;
    BrClipVert av[4];
    int i;

    /* --- texture coordinates come from the scissor, times 8 -------- */
    SetupGbi(1.0f, 0.0f);
    g_gbi.geo.cur = 0x1000u;
    BrGbiCall10021560(0, 0, 2560, 1920, 0);
    GrabCorners(av);

    CHECK(av[V1].f10 == (float)g_gbi.scissor.ulx * 8.0f);
    CHECK(av[V3].f10 == (float)g_gbi.scissor.ulx * 8.0f);
    CHECK(av[V0].f10 == (float)g_gbi.scissor.lrx * 8.0f);
    CHECK(av[V2].f10 == (float)g_gbi.scissor.lrx * 8.0f);
    CHECK(av[V1].f14 == (float)g_gbi.scissor.uly * 8.0f);
    CHECK(av[V2].f14 == (float)g_gbi.scissor.uly * 8.0f);
    CHECK(av[V0].f14 == (float)g_gbi.scissor.lry * 8.0f);
    CHECK(av[V3].f14 == (float)g_gbi.scissor.lry * 8.0f);

    /* The x/u pairing follows the corner, not the vertex index. */
    CHECK((av[V1].f04 == av[V3].f04) && (av[V1].f10 == av[V3].f10));
    CHECK((av[V0].f04 == av[V2].f04) && (av[V0].f10 == av[V2].f10));

    /* --- 0x104C0DC0 clear: every channel is the literal 1.0f ------- */
    for (i = 0; i < 4; ++i) {
        CHECK(av[i].f1C == 1.0f);
        CHECK(av[i].f20 == 1.0f);
        CHECK(av[i].f24 == 1.0f);
    }

    /* --- 0x104C0DC0 set: two triples, split by which x they carry --- */
    SetupGbi(1.0f, 0.0f);
    pSt = BrGbiRectGetState();
    pSt->f4C0DC0 = 1;
    pSt->aRgb4BBF04[0] = 0.1f;
    pSt->aRgb4BBF04[1] = 0.2f;
    pSt->aRgb4BBF04[2] = 0.3f;
    g_gbi.geo.cur = 0x1000u;
    BrGbiCall10021560(0, 0, 2560, 1920, 0);
    GrabCorners(av);

    CHECK(av[V0].f1C == 0.1f && av[V0].f20 == 0.2f && av[V0].f24 == 0.3f);
    CHECK(av[V3].f1C == 0.1f && av[V3].f20 == 0.2f && av[V3].f24 == 0.3f);
    CHECK(av[V1].f1C == g_gbi.light.off[0]);
    CHECK(av[V1].f20 == g_gbi.light.off[1]);
    CHECK(av[V1].f24 == g_gbi.light.off[2]);
    CHECK(av[V2].f1C == g_gbi.light.off[0]);
    CHECK(av[V2].f20 == g_gbi.light.off[1]);
    CHECK(av[V2].f24 == g_gbi.light.off[2]);
}

static void test_21560_winding(void)
{
    BrClipVert aSet[6], aClear[6];
    int i, j;

    SetupGbi(1.0f, 0.0f);
    g_gbi.geo.cur = 0x1000u;                /* G_CULL_FRONT */
    BrGbiCall10021560(0, 0, 2560, 1920, 0);
    CHECK(g_cEmit == 6);
    memcpy(aSet, g_aEmit, sizeof aSet);

    SetupGbi(1.0f, 0.0f);
    g_gbi.geo.cur = 0u;
    BrGbiCall10021560(0, 0, 2560, 1920, 0);
    CHECK(g_cEmit == 6);
    memcpy(aClear, g_aEmit, sizeof aClear);

    /* Both branches emit the same two triangles; the only difference is
     * that each one's vertex order is reversed. */
    for (i = 0; i < 2; ++i) {
        for (j = 0; j < 3; ++j) {
            const BrClipVert *pA = &aSet[i * 3 + j];
            const BrClipVert *pB = &aClear[i * 3 + (2 - j)];

            CHECK(pA->f04 == pB->f04);
            CHECK(pA->f08 == pB->f08);
            CHECK(pA->f10 == pB->f10);
            CHECK(pA->f14 == pB->f14);
        }
    }

    /* And the two triangles are not the same triangle. */
    CHECK(!(aSet[0].f04 == aSet[3].f04 && aSet[0].f08 == aSet[3].f08 &&
            aSet[1].f04 == aSet[4].f04 && aSet[1].f08 == aSet[4].f08 &&
            aSet[2].f04 == aSet[5].f04 && aSet[2].f08 == aSet[5].f08));
}

static void test_21560_flush(void)
{
    BrGbiRectState *pSt;
    unsigned char   aObj[2][0x70];
    void           *apObj[2];
    int             i;

    /* --- dirty == 0: nothing is flushed at all -------------------- */
    SetupGbi(1.0f, 0.0f);
    pSt = BrGbiRectGetState();
    pSt->cIndices = 12;
    pSt->aShadow[0] = 0xAAu;
    BrGbiCall10021560(0, 0, 2560, 1920, 0);

    CHECK(g_cRs == 0);
    CHECK(g_cDraw == 0);
    CHECK(pSt->cIndices == 12);         /* the batch is left alone */
    CHECK(pSt->aShadow[0] == 0xAAu);

    /* --- dirty set, empty batch: states only --------------------- */
    SetupGbi(1.0f, 0.0f);
    pSt = BrGbiRectGetState();
    pSt->cIndices = 0;
    pSt->dirty = (1u << 2) | (1u << 10);
    pSt->aPending[2]  = 0x1234u;
    pSt->aPending[10] = 0x5678u;
    pSt->aShadow[1]   = 0x99u;          /* an untouched entry */
    BrGbiCall10021560(0, 0, 2560, 1920, 0);

    CHECK(g_cDraw == 0);
    CHECK(g_cRs == 2);
    /* bit order, not numeric order: bit 2 is SRCBLEND, bit 10 is
     * ALPHABLENDENABLE, and 0x1B < 0x13 is not how they come out. */
    CHECK(g_aRsState[0] == BrGbiRectRenderState[2]);
    CHECK(g_aRsState[1] == BrGbiRectRenderState[10]);
    CHECK(g_aRsState[0] == 0x13u);
    CHECK(g_aRsState[1] == 0x1Bu);
    CHECK(g_aRsValue[0] == 0x1234u);
    CHECK(g_aRsValue[1] == 0x5678u);
    CHECK(pSt->aShadow[2]  == 0x1234u);
    CHECK(pSt->aShadow[10] == 0x5678u);
    CHECK(pSt->aShadow[1]  == 0x99u);   /* clean entries stay put */
    CHECK(pSt->dirty == 0);             /* and the mask is cleared */

    /* --- dirty set, non-empty batch: the draw runs first ---------- */
    SetupGbi(1.0f, 0.0f);
    pSt = BrGbiRectGetState();
    memset(aObj, 0, sizeof aObj);
    apObj[0] = aObj[0];
    apObj[1] = aObj[1];
    pSt->ap4C0BC0  = apObj;
    pSt->c4C5190   = 2;
    pSt->pvVertices = aObj[0];
    pSt->cVertices  = 7;
    pSt->pwIndices  = (uint16_t *)(void *)aObj[1];
    pSt->cIndices   = 12;
    pSt->dirty      = 1u;
    pSt->aPending[0] = 0x4444u;
    BrGbiCall10021560(0, 0, 2560, 1920, 0);

    CHECK(g_cDraw == 1);
    CHECK(g_drawArgs[0] == 4u);         /* D3DPT_TRIANGLELIST */
    CHECK(g_drawArgs[1] == 3u);         /* D3DVT_TLVERTEX     */
    CHECK(g_drawArgs[2] == 7u);         /* vertex count       */
    CHECK(g_drawArgs[3] == 12u);        /* index count        */
    CHECK(g_drawArgs[4] == 0xCu);       /* flags              */
    CHECK(g_drawVerts == pSt->pvVertices);
    CHECK(g_drawIndices == pSt->pwIndices);

    /* every entry of the 0x104C0BC0 array gets -1 at +0x68 */
    for (i = 0; i < 2; ++i) {
        CHECK(aObj[i][0x68] == 0xFFu);
        CHECK(aObj[i][0x69] == 0xFFu);
        CHECK(aObj[i][0x6A] == 0xFFu);
        CHECK(aObj[i][0x6B] == 0xFFu);
        CHECK(aObj[i][0x67] == 0x00u);  /* and nothing either side */
        CHECK(aObj[i][0x6C] == 0x00u);
    }

    CHECK(pSt->cIndices == 0);
    CHECK(pSt->cVertices == 0);
    CHECK(pSt->c4C5190 == 0);
    CHECK(g_cRs == 1);
    CHECK(g_aRsState[0] == 0x0Eu);      /* ZWRITEENABLE */
    CHECK(pSt->dirty == 0);
}

/* A null device must not stop the bookkeeping: the port only skips the two
 * COM calls, never the state updates. */
static void test_21560_no_device(void)
{
    BrGbiRectState *pSt;

    SetupGbi(1.0f, 0.0f);
    pSt = BrGbiRectGetState();
    pSt->pDev = NULL;
    pSt->dirty = 1u;
    pSt->aPending[0] = 0x77u;
    BrGbiCall10021560(0, 0, 2560, 1920, 0);

    CHECK(pSt->aShadow[0] == 0x77u);
    CHECK(pSt->dirty == 0);
    CHECK(g_cEmit == 6);                /* and the geometry still goes out */
}

int main(void)
{
    test_3E070();
    test_3D0B0();
    test_21560_geometry();
    test_21560_uv_and_colour();
    test_21560_winding();
    test_21560_flush();
    test_21560_no_device();

    if (g_cFail != 0) {
        printf("%d failure(s)\n", g_cFail);
        return 1;
    }
    printf("slice4_51: all checks passed\n");
    return 0;
}
