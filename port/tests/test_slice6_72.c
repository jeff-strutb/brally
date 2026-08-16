/* test_slice6_72.c -- behaviour and invariants for slice6_72.c.
 *
 * The assertions are properties of the original, not counts chosen to match
 * whatever the code happens to do: the render-state cache is checked for the
 * invariant "after a fill, nothing is dirty and every applied value equals
 * the wanted one"; the rectangle is checked for the geometric identity "the
 * six vertices span exactly the clamped rect, y flipped"; the bit shuffle is
 * checked for "the two outputs partition the input"; the builders are checked
 * for the family invariants (f38's a4/a5 are always 2 and 5, the owner is
 * always the PHASE and never the screen, every control is registered before
 * the counter moves) plus each documented GOTCHA.
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "slice6_72.h"

static int g_nFail = 0;
static int g_nRun  = 0;

#define CHECK(cond)                                                       \
    do {                                                                  \
        ++g_nRun;                                                         \
        if (!(cond)) {                                                    \
            printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);        \
            ++g_nFail;                                                    \
        }                                                                 \
    } while (0)

/* =====================================================================
 * Cross-slice stand-ins.  They live here and nowhere else.
 * ===================================================================== */

static int  g_cErr;
static int  g_idErrLast;
void BrErrShow(const BrErrHost *pHost, int32_t idx)
{
    (void)pHost;
    ++g_cErr;
    g_idErrLast = (int)idx;
}
const BrErrEnt g_aBrErrTable[BR_ERR_COUNT];

/* operator new does NOT zero -- the constructors are what initialise. */
static int    g_cNew;
static size_t g_cbNewLast;
void *BrOperatorNew(uint32_t cb)
{
    ++g_cNew;
    g_cbNewLast = (size_t)cb;
    return malloc((size_t)cb);
}
void BrOperatorDelete(void *p) { free(p); }

/* The string table.  Returns a stable pointer per id so a test can recover
 * which id a call site asked for. */
#define STR_MAX 0x200
static char g_aszStr[STR_MAX][8];
const char *BrStrGet(int id)
{
    if (id < 0 || id >= STR_MAX) {
        return NULL;
    }
    sprintf(g_aszStr[id], "#%03X", (unsigned)id);
    return g_aszStr[id];
}
static int StrIdOf(const void *p)
{
    int i;
    for (i = 0; i < STR_MAX; ++i) {
        if ((const void *)g_aszStr[i] == p) {
            return i;
        }
    }
    return -1;
}

/* __ftol: truncate toward zero, low dword of a 64-bit fistp. */
int32_t BrFtolTrunc(float f) { return (int32_t)(int64_t)f; }

static int g_c294C;
int BrOptOpen294C(struct BrGameObj *pUnused) { (void)pUnused; ++g_c294C; return 1; }

const BrTextBoxVtbl *g_pBrTextBoxVtbl;

/* =====================================================================
 * The environment
 * ===================================================================== */

static Br72Env      g_env;
static BrUi72Hooks  g_hooks;

/* --- recorded f38 / f34 / list calls ---------------------------------- */
#define REC_MAX 400

typedef struct Rec38 {
    const BrUiCtl_  *pCtl;
    const void      *pOwner;
    float            x, y;
    int32_t          flags, a4, a5, a6, a7;
} Rec38;

typedef struct Rec34 {
    const BrUiCtl_  *pCtl;
    const void      *pText;
    int32_t          a2, a3;
    const void      *pStyle;
} Rec34;

static Rec38 g_a38[REC_MAX];  static int g_c38;
static Rec34 g_a34[REC_MAX];  static int g_c34;
static int   g_cSubF10, g_cSubF14;
static int   g_cItemF04;
static int   g_cGrfEnum;

static void FakeF38(BrUiCtl_ *pThis, BrPhase_ *pOwner, float x, float y,
                    int32_t flags, int32_t a4, int32_t a5,
                    int32_t a6, int32_t a7)
{
    if (g_c38 < REC_MAX) {
        Rec38 *r = &g_a38[g_c38];
        r->pCtl = pThis; r->pOwner = pOwner; r->x = x; r->y = y;
        r->flags = flags; r->a4 = a4; r->a5 = a5; r->a6 = a6; r->a7 = a7;
    }
    ++g_c38;
}

static void FakeF34(BrUiCtl_ *pThis, const void *pText,
                    int32_t a2, int32_t a3, const void *pStyle)
{
    if (g_c34 < REC_MAX) {
        Rec34 *r = &g_a34[g_c34];
        r->pCtl = pThis; r->pText = pText; r->a2 = a2; r->a3 = a3;
        r->pStyle = pStyle;
    }
    ++g_c34;
}

/* br_ui.h's BrUiCtlVtbl_ names all sixteen slots, so the two this packet
 * drives are set by name rather than by position behind a reserved array. */
static const BrUiCtlVtbl_ g_ctlVtbl = {
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL,
    FakeF34,        /* +0x34 */
    FakeF38,        /* +0x38 */
    NULL            /* +0x3C */
};

/* The object at control +0x3838 is slice3_39.h's BrTextList in full (br_ui.h
 * ADJ-6), so these stand in for two slots of ITS vtable, not for a private
 * three-field stub's. */
static void FakeSubF10(BrTextList *pThis, const void *pText, int32_t a2,
                       int32_t a3, const void *pStyle, int32_t a5)
{ (void)pThis; (void)pText; (void)a2; (void)a3; (void)pStyle; (void)a5;
  ++g_cSubF10; }

static void FakeSubF14(BrTextList *pThis, int32_t a1, const void *pStyle,
                       int32_t a3, int32_t a4, int32_t a5)
{ (void)pThis; (void)a1; (void)pStyle; (void)a3; (void)a4; (void)a5;
  ++g_cSubF14; }

static const BrTextListVtbl g_subVtbl = {
    NULL, NULL, NULL, NULL,
    FakeSubF10,     /* +0x10 */
    FakeSubF14,     /* +0x14 */
    NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL
};

/* The item block is slice3_39.h's BrTextBox (ADJ-1/ADJ-2); its "text changed"
 * slot is that object's pfn04. */
static void FakeItemF04(BrTextBox *pThis) { (void)pThis; ++g_cItemF04; }
static const BrTextBoxVtbl g_itemVtbl = {
    NULL, FakeItemF04, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL
};

/* The two constructors.  The real ones zero the object and install vtables;
 * these do the same so the tests see a deterministic starting state. */
static BrUiPage_ *FakePageCtor(BrUiPage_ *p)
{
    memset(p, 0, sizeof(*p));
    return p;
}
static BrUiCtl_ *FakeCtlCtor(BrUiCtl_ *p)
{
    memset(p, 0, sizeof(*p));
    p->pVtbl        = &g_ctlVtbl;
    p->aText[0].pVtbl = &g_itemVtbl;
    p->list.pVtbl     = &g_subVtbl;
    return p;
}

/* --- the .GRF enumerator ---------------------------------------------- */
static BrGrfList     g_grf;
static void FakeGrfEnum(BrGrfList *pThis, const char *pszMask)
{ (void)pThis; (void)pszMask; ++g_cGrfEnum; }
static const BrGrfListVtbl g_grfVtbl = { NULL, FakeGrfEnum };

/* --- the injected callees --------------------------------------------- */
static int g_c795D0, g_c3E2C0, g_c75100, g_c5960, g_c1C620, g_c1C640;
static int g_c307A0, g_c69490;
static void Fake795D0(void) { ++g_c795D0; }
static void Fake3E2C0(void) { ++g_c3E2C0; }
static void Fake75100(void) { ++g_c75100; }
static void Fake5960(void)  { ++g_c5960;  }
static void Fake1C620(BrGfxSurf *p) { (void)p; ++g_c1C620; }
static void Fake1C640(void) { ++g_c1C640; }

static const float *g_pMtxSeen;
static void       *g_pMtxDst;
static void Fake307A0(const float *pM, void *pDst)
{ g_pMtxSeen = pM; g_pMtxDst = pDst; ++g_c307A0; }

static char g_matrixTarget[16];
static void *Fake69490(void) { ++g_c69490; return g_matrixTarget; }

/* --- KERNEL32 stand-ins ------------------------------------------------ */
static int g_cMutex;
static void *FakeCreateMutex(void)
{
    ++g_cMutex;
    /* A distinct non-NULL handle per call. */
    return (void *)(uintptr_t)(0x1000u + (unsigned)g_cMutex);
}
static int g_cHandle, g_cUnlock, g_cFree;
static void *FakeGlobalHandle(void *pv) { ++g_cHandle; return pv; }
static int32_t FakeGlobalUnlock(void *h) { (void)h; ++g_cUnlock; return 1; }
static int32_t FakeGlobalFree(void *h)   { (void)h; ++g_cFree;   return 0; }

/* --- Direct3D stand-ins ------------------------------------------------ */
typedef struct RecRs { int32_t state, value; } RecRs;
static RecRs g_aRs[64];   static int g_cRs;
static BrD3DTLVertex g_aVert[16];  static int g_cVert;  static int g_cDraw;
static int g_cDrawIndexed;

static int32_t FakeSetRs(BrD3DDev *pThis, int32_t state, int32_t value)
{
    (void)pThis;
    if (g_cRs < 64) { g_aRs[g_cRs].state = state; g_aRs[g_cRs].value = value; }
    ++g_cRs;
    return 0;
}
static int32_t FakeDraw(BrD3DDev *pThis, int32_t type, int32_t vtype,
                        const void *pVerts, int32_t cVerts, int32_t flags)
{
    int i;
    (void)pThis; (void)flags;
    CHECK(type  == 4);   /* D3DPT_TRIANGLELIST */
    CHECK(vtype == 3);   /* D3DVT_TLVERTEX     */
    for (i = 0; i < cVerts && g_cVert < 16; ++i, ++g_cVert) {
        g_aVert[g_cVert] = ((const BrD3DTLVertex *)pVerts)[i];
    }
    ++g_cDraw;
    return 0;
}
static int32_t FakeDrawIndexed(BrD3DDev *pThis, int32_t type, int32_t vtype,
                               const void *pVerts, int32_t cVerts,
                               const void *pIndices, int32_t cIndices,
                               int32_t flags)
{
    (void)pThis; (void)type; (void)vtype; (void)pVerts; (void)cVerts;
    (void)pIndices; (void)cIndices; (void)flags;
    ++g_cDrawIndexed;
    return 0;
}
static const BrD3DDevVtbl g_devVtbl = {
    { 0 }, FakeSetRs, { 0 }, FakeDraw, FakeDrawIndexed
};
static BrD3DDev g_dev;

/* --- the software arm -------------------------------------------------- */
static int g_cT20, g_cT30;
static int32_t g_aRectSeen[4];
static int32_t g_nT20Arg;
static void FakeT20(BrGfxTarget *p, int32_t a) { (void)p; g_nT20Arg = a; ++g_cT20; }
static void FakeT30(BrGfxTarget *p, int32_t a1, const int32_t *pRect, int32_t a3)
{
    (void)p; (void)a1; (void)a3;
    memcpy(g_aRectSeen, pRect, sizeof(g_aRectSeen));
    ++g_cT30;
}
static const BrGfxTargetVtbl g_targetVtbl = {
    { 0 }, FakeT20, { 0 }, FakeT30
};
static BrGfxTarget g_target;
static BrGfxSurf   g_surf;
static BrGfxCtx    g_gfx;

/* --- DirectInput stand-in ---------------------------------------------- */
static int32_t g_hrGet1, g_hrGet2, g_hrAcq;
static int     g_cGet, g_cAcq;
static uint8_t g_bStamp;
static int32_t FakeGetState(BrDInputDev *p, uint32_t cb, void *pv)
{
    (void)p;
    CHECK(cb == 0x100u);
    ++g_cGet;
    ((uint8_t *)pv)[0] = g_bStamp++;
    return (g_cGet == 1) ? g_hrGet1 : g_hrGet2;
}
static int32_t FakeAcquire(BrDInputDev *p) { (void)p; ++g_cAcq; return g_hrAcq; }
static const BrDInputDevVtbl g_dikVtbl = { { 0 }, FakeAcquire, NULL, FakeGetState };
static BrDInputDev g_dik;

/* --- DirectPlay stand-in ------------------------------------------------ */
static BrDPSessionUser g_desc;
static int32_t g_hr3D0B0, g_hrSetDesc;
static int     g_cSetDesc, g_c3D0B0;
static int     g_f3D0B0Null;
static int32_t Fake3D0B0(void *pDP, BrDPSessionUser **ppDesc)
{
    (void)pDP;
    ++g_c3D0B0;
    *ppDesc = g_f3D0B0Null ? NULL : &g_desc;
    return g_hr3D0B0;
}
static int32_t FakeSetDesc(void *pDP, BrDPSessionUser *pDesc, uint32_t flags)
{
    (void)pDP; (void)pDesc;
    CHECK(flags == 0u);
    ++g_cSetDesc;
    return g_hrSetDesc;
}

/* --- the display list --------------------------------------------------- */
static Br72GfxCmd g_aDl[16];

/* =====================================================================
 * Setup
 * ===================================================================== */

static int32_t g_aAC520[BR72_AC520_MAX];
static char    g_blkDF30, g_blkDFD8, g_blkE080, g_blkE128;
static char    g_style[16];
static char    g_text39B720[8];
static char    g_textAD300[8];
static char    g_textAD274[8];

static void EnvReset(void)
{
    int i;

    memset(&g_env,   0, sizeof(g_env));
    memset(&g_hooks, 0, sizeof(g_hooks));
    memset(&g_gfx,   0, sizeof(g_gfx));
    memset(&g_surf,  0, sizeof(g_surf));
    memset(g_aDl,    0, sizeof(g_aDl));

    /* Give every hook slot a distinct non-NULL value so a mixed-up
     * assignment is visible. */
    {
        BrUiCtlHookFn_ *p = (BrUiCtlHookFn_ *)(void *)&g_hooks;
        size_t n = sizeof(g_hooks) / sizeof(BrUiCtlHookFn_);
        size_t k;
        for (k = 0; k < n; ++k) {
            /* A function pointer synthesised from an integer is not
             * portable to call, and nothing here calls them. */
            p[k] = (BrUiCtlHookFn_)(void *)((char *)&g_hooks + k);
        }
    }

    g_dev.pVtbl    = &g_devVtbl;
    g_target.pVtbl = &g_targetVtbl;
    g_dik.pVtbl    = &g_dikVtbl;
    g_grf.pVtbl    = &g_grfVtbl;
    for (i = 0; i < BR72_GRF_COUNT; ++i) {
        sprintf(g_grf.aName[i], "ghost%02d.grf", i);
    }
    g_gfx.pSurf   = &g_surf;
    g_gfx.pTarget = &g_target;

    g_env.pHooks      = &g_hooks;
    g_env.pfnPageCtor = FakePageCtor;
    g_env.pfnCtlCtor  = FakeCtlCtor;
    g_env.pfn100795D0 = Fake795D0;
    g_env.pfn1003E2C0 = Fake3E2C0;
    g_env.pfn10075100 = Fake75100;
    g_env.pfn10005960 = Fake5960;
    g_env.pfn1001C620 = Fake1C620;
    g_env.pfn1001C640 = Fake1C640;
    g_env.pfn100307A0 = Fake307A0;
    g_env.pfn10069490 = Fake69490;
    g_env.pfnCreateMutex  = FakeCreateMutex;
    g_env.pfnGlobalHandle = FakeGlobalHandle;
    g_env.pfnGlobalUnlock = FakeGlobalUnlock;
    g_env.pfnGlobalFree   = FakeGlobalFree;
    g_env.pfn1003D0B0     = Fake3D0B0;
    g_env.pfnDPSetSessionDesc = FakeSetDesc;

    g_env.pDev277368  = &g_dev;
    g_env.pCtx27736C  = &g_gfx;
    g_env.pDik18ABDD0 = &g_dik;
    g_env.pDPlay277B40 = (void *)&g_desc;   /* any non-NULL */

    g_env.p0AB438 = &g_style[0];   g_env.p0AB448 = &g_style[1];
    g_env.p0AB458 = &g_style[2];   g_env.p0AB478 = &g_style[3];
    g_env.p0AB488 = &g_style[4];   g_env.p0AB4A8 = &g_style[5];
    g_env.p0AB4B8 = &g_style[6];   g_env.p0AB4C8 = &g_style[7];
    g_env.p0AB4D8 = &g_style[8];   g_env.p0AB4F8 = &g_style[9];
    g_env.p0AB508 = &g_style[10];
    g_env.p0AD274 = g_textAD274;
    g_env.p0AD300 = g_textAD300;
    g_env.p39B720 = g_text39B720;
    g_env.pszTimeAttackMask = "TimeAttack*.GRF";

    g_env.pB4DF30 = &g_blkDF30;  g_env.pB4DFD8 = &g_blkDFD8;
    g_env.pB4E080 = &g_blkE080;  g_env.pB4E128 = &g_blkE128;
    g_env.aAC520  = g_aAC520;
    g_env.cAC520  = BR72_AC520_MAX;

    g_env.pDlCursor = g_aDl;
    g_env.n0A81C4   = 480;

    g_c38 = g_c34 = 0; g_cSubF10 = g_cSubF14 = g_cItemF04 = 0;
    g_cGrfEnum = 0;
    g_cErr = g_cNew = 0; g_idErrLast = -1;
    g_c795D0 = g_c3E2C0 = g_c75100 = g_c5960 = g_c1C620 = g_c1C640 = 0;
    g_c307A0 = g_c69490 = 0;
    g_cMutex = g_cHandle = g_cUnlock = g_cFree = 0;
    g_cRs = g_cVert = g_cDraw = g_cDrawIndexed = 0;
    g_cT20 = g_cT30 = 0;
    g_cGet = g_cAcq = 0; g_bStamp = 0;
    g_cSetDesc = g_c3D0B0 = 0; g_f3D0B0Null = 0;
    g_c294C = 0;

    g_pBr72Env = &g_env;
}

/* Free every control and page a builder allocated, so the tests do not leak
 * ~12 MB per run. */
static void PhaseFree(BrPhase_ *pPhase)
{
    unsigned i, j;
    for (i = 0; i < pPhase->nPages && i < BR_PHASE_PAGES; ++i) {
        BrUiPage_ *pScr = pPhase->aPages[i];
        if (pScr == NULL) { continue; }
        for (j = 0; j < pScr->cCtl && j < BR72_PAGE_CTL_MAX; ++j) {
            free(pScr->apCtl[j]);
        }
        free(pScr);
    }
}

/* =====================================================================
 * 0x10044540
 * ===================================================================== */
static void Test44540(void)
{
    static const int32_t aFlag[5] = { 0x102, 0x81, 0x4050, 0x202C, 0x1E00 };
    static const int32_t aMode[5] = { 1, 0, 6, 3, 0x0B };
    int i;

    for (i = 0; i < 5; ++i) {
        EnvReset();
        g_env.nAA2A18 = i;
        g_env.nAA2A44 = -1;             /* force "changed" */
        BrSub10044540();
        CHECK(g_env.n0AB3E8 == aFlag[i]);
        CHECK(g_env.n0AC654 == aMode[i]);
        CHECK(g_env.nAA2A44 == i);
    }

    /* Out of range, both directions, takes the case-0 pair. */
    EnvReset();
    g_env.nAA2A18 = 5;  g_env.nAA2A44 = -1;
    BrSub10044540();
    CHECK(g_env.n0AB3E8 == 0x102 && g_env.n0AC654 == 1);

    EnvReset();
    g_env.nAA2A18 = -1; g_env.nAA2A44 = 0;   /* `ja` is UNSIGNED */
    BrSub10044540();
    CHECK(g_env.n0AB3E8 == 0x102 && g_env.n0AC654 == 1);

    /* The guard: a second call with the same selector writes nothing, even
     * if something else has since clobbered the outputs. */
    EnvReset();
    g_env.nAA2A18 = 2; g_env.nAA2A44 = -1;
    BrSub10044540();
    g_env.n0AB3E8 = 0x1234; g_env.n0AC654 = 0x5678;
    BrSub10044540();
    CHECK(g_env.n0AB3E8 == 0x1234 && g_env.n0AC654 == 0x5678);
}

/* =====================================================================
 * 0x1003E3A0
 * ===================================================================== */
static void Test3E3A0(void)
{
    int i;
    const void *aExp[5];

    EnvReset();
    aExp[0] = &g_blkDF30;  /* 0 -> default */
    aExp[1] = &g_blkDFD8;
    aExp[2] = &g_blkE080;
    aExp[3] = &g_blkE128;
    aExp[4] = &g_blkDF30;  /* 4 -> default */

    for (i = 0; i < 5; ++i) {
        EnvReset();
        g_aAC520[0]    = i;
        g_env.nAA2A0C  = 0;
        BrSub1003E3A0();
        CHECK(g_env.nB4E1D0 == i);
        CHECK(g_env.pB4E1D4 == aExp[i]);
    }

    /* The four "is zero" flags. */
    EnvReset();
    g_aAC520[0] = 0;
    g_env.nB4E1E0 = 0; g_env.nB4E1D8 = 7; g_env.nB4E1DC = 0; g_env.nB4E7A0 = 9;
    BrSub1003E3A0();
    CHECK(g_env.nAA2A1C == 1);
    CHECK(g_env.nAA2A20 == 0);
    CHECK(g_env.nAA2A24 == 1);
    CHECK(g_env.nAA2A28 == 0);
    CHECK(g_c3E2C0 == 1);

    /* The string copy. */
    EnvReset();
    g_aAC520[0]    = 0;
    g_env.pszB4E1E4 = "Rally";
    BrSub1003E3A0();
    CHECK(strcmp(g_env.szA9CDF0, "Rally") == 0);

    /* GOTCHA: a config value of 1 is promoted to 2, so 1 is unreachable. */
    EnvReset();
    g_aAC520[0] = 0;
    g_env.cfgB4E710.nB4E728 = 1;
    BrSub1003E3A0();
    CHECK(g_env.nAA2A0C == 2);

    EnvReset();
    g_aAC520[0] = 0;
    g_env.cfgB4E710.nB4E728 = 3;
    BrSub1003E3A0();
    CHECK(g_env.nAA2A0C == 3);

    /* GOTCHA: 0x100AB3E4 is OR-ed WORD wide with the LOW HALF of a dword, so
     * the upper half of the source never reaches it. */
    EnvReset();
    g_aAC520[0] = 0;
    g_env.w0AB3E4 = 0x0001u;
    g_env.cfgB4E710.nB4E730 = (int32_t)0xABCD0010;
    BrSub1003E3A0();
    CHECK(g_env.w0AB3E4 == 0x0011u);
    CHECK(g_env.nAA2A10 == (int32_t)0xABCD0010);

    /* 0x100AB3EC is OR-ed FULL width from a different source. */
    EnvReset();
    g_aAC520[0] = 0;
    g_env.n0AB3EC = 0x00010000;
    g_env.cfgB4E710.nB4E734 = 0x02000001;
    BrSub1003E3A0();
    CHECK(g_env.n0AB3EC == 0x02010001);
    CHECK(g_env.nAA2A14 == 0x02000001);

    /* The straight republications. */
    EnvReset();
    g_aAC520[0] = 0;
    g_env.cfgB4E710.nB4E710 = 11; g_env.cfgB4E710.nB4E714 = 12;
    g_env.cfgB4E710.nB4E718 = 13; g_env.cfgB4E710.nB4E71C = 14;
    g_env.cfgB4E710.nB4E720 = 15; g_env.cfgB4E710.nB4E724 = 16;
    g_env.cfgB4E710.nB4E72C = 17; g_env.cfgB4E710.nB4E738 = 18;
    g_env.cfgB4E710.nB4E73C = 19;
    BrSub1003E3A0();
    CHECK(g_env.n0AC648 == 11 && g_env.nAA2A00 == 12 && g_env.nAA2A08 == 13);
    CHECK(g_env.n0AC64C == 14 && g_env.n0AC650 == 15 && g_env.n0AC654 == 16);
    CHECK(g_env.n0AC658 == 17 && g_env.n0AC65C == 18 && g_env.nAA2A18 == 19);
}

/* =====================================================================
 * 0x10035FC0 -- the partition identity
 * ===================================================================== */
static void Test35FC0(void)
{
    static const uint32_t aA[] = { 0u, 0xFFFFFFFFu, 0xA5A5A5A5u, 1u, 0x80000000u };
    static const uint32_t aB[] = { 0xFFFFFFFFu, 0u, 0x0F0F0F0Fu, 1u, 0x7FFFFFFFu };
    size_t i;

    for (i = 0; i < sizeof(aA) / sizeof(aA[0]); ++i) {
        uint32_t p[2];
        uint32_t a = aA[i], b = aB[i];
        p[0] = a; p[1] = b;
        BrEnt35FC0(p);
        /* The two outputs partition the input exactly. */
        CHECK((p[0] | p[1]) == a);
        CHECK((p[0] & p[1]) == 0u);
        /* p[1] is what a and b agree on; p[0] is what only a has. */
        CHECK(p[1] == (a & b));
        CHECK((p[0] & b) == 0u);
    }

    /* Applying it twice: the second pass sees (a&~b, a&b), which are
     * disjoint, so the "agreed" half collapses to zero. */
    {
        uint32_t p[2];
        p[0] = 0xF0F0F0F0u; p[1] = 0xFF00FF00u;
        BrEnt35FC0(p);
        BrEnt35FC0(p);
        CHECK(p[1] == 0u);
        CHECK(p[0] == (0xF0F0F0F0u & ~0xFF00FF00u));
    }
}

/* =====================================================================
 * 0x1005B0C0
 * ===================================================================== */
static void Test5B0C0(void)
{
    static const BrTextBoxVtbl vt;
    BrTextBox box;

    EnvReset();
    memset(&box, 0xCD, sizeof(box));
    g_pBrTextBoxVtbl = &vt;
    BrTextBoxDtor(&box);
    CHECK(box.pVtbl == &vt);
    /* Nothing else is touched -- the byte after the pointer is untouched. */
    CHECK(box.f04 == 0xCDCDCDCDu);
}

/* =====================================================================
 * 0x100771B0
 * ===================================================================== */
static void Test771B0(void)
{
    uint8_t aState[256];

    /* No device: 1, and the buffer is not written.  GOTCHA -- 1 is POSITIVE,
     * so a caller testing >= 0 proceeds with a stale buffer. */
    EnvReset();
    g_env.pDik18ABDD0 = NULL;
    memset(aState, 0x77, sizeof(aState));
    CHECK(BrDikGetDeviceState(aState) == 1);
    CHECK(aState[0] == 0x77);
    CHECK(g_cGet == 0);

    /* Straight success. */
    EnvReset();
    g_hrGet1 = 0;
    CHECK(BrDikGetDeviceState(aState) == 0);
    CHECK(g_cGet == 1 && g_cAcq == 0);

    /* An error that is NOT DIERR_NOTACQUIRED: no retry. */
    EnvReset();
    g_hrGet1 = (int32_t)0x80004005;
    CHECK(BrDikGetDeviceState(aState) == (int32_t)0x80004005);
    CHECK(g_cGet == 1 && g_cAcq == 0);

    /* DIERR_NOTACQUIRED: re-acquire, then retry, and the retry's value wins. */
    EnvReset();
    g_hrGet1 = BR72_DIERR_NOTACQUIRED;
    g_hrAcq  = 0;
    g_hrGet2 = 0;
    CHECK(BrDikGetDeviceState(aState) == 0);
    CHECK(g_cGet == 2 && g_cAcq == 1);

    /* GOTCHA: when the re-acquire fails, ITS hresult comes back, not
     * DIERR_NOTACQUIRED, and there is no second read. */
    EnvReset();
    g_hrGet1 = BR72_DIERR_NOTACQUIRED;
    g_hrAcq  = (int32_t)0x8007000E;
    CHECK(BrDikGetDeviceState(aState) == (int32_t)0x8007000E);
    CHECK(g_cGet == 1 && g_cAcq == 1);
}

/* =====================================================================
 * 0x1002B2A0 -- comparison polarity
 * ===================================================================== */
static void Test2B2A0(void)
{
    EnvReset();
    g_env.f575514 = 1.0f;  g_env.n575530 = 0;
    CHECK(BrSub_1002B2A0() == 0);

    EnvReset();
    g_env.f575514 = 0.0f;  g_env.n575530 = 0;
    CHECK(BrSub_1002B2A0() == 0);          /* 0 >= 0 is the false side */

    EnvReset();
    g_env.f575514 = -0.5f; g_env.n575530 = 0;
    CHECK(BrSub_1002B2A0() == 1);

    EnvReset();
    g_env.f575514 = 1.0f;  g_env.n575530 = 1;
    CHECK(BrSub_1002B2A0() == 1);

    /* The polarity property: C0 is also set for UNORDERED, so a NaN takes
     * the "return 1" side even though it is not "less than". */
    EnvReset();
    g_env.f575514 = (float)NAN; g_env.n575530 = 0;
    CHECK(BrSub_1002B2A0() == 1);
}

/* =====================================================================
 * 0x1003407D
 * ===================================================================== */
static void Test3407D(void)
{
    int i;

    EnvReset();
    BrSub_1003407D(4.0f, 8.0f);

    CHECK(g_env.aMtx6C29A8[0]  ==  0.5f);    /* 2 / 4 */
    CHECK(g_env.aMtx6C29A8[5]  ==  0.25f);   /* 2 / 8 */
    CHECK(g_env.aMtx6C29A8[12] == -1.0f);
    CHECK(g_env.aMtx6C29A8[13] == -1.0f);
    CHECK(g_env.aMtx6C29A8[14] ==  0.0f);
    CHECK(g_env.aMtx6C29A8[15] ==  1.0f);
    for (i = 1; i < 12; ++i) {
        if (i == 5) { continue; }
        CHECK(g_env.aMtx6C29A8[i] == 0.0f);
    }

    /* Two 8-byte commands, in order, cursor advanced by exactly two. */
    CHECK(g_env.pDlCursor == g_aDl + 2);
    CHECK(g_aDl[0].w0 == 0xBC00000Eu);
    CHECK(g_aDl[1].w0 == 0x01030040u);
    CHECK(g_aDl[1].p1 == g_matrixTarget);
    CHECK(g_env.p6C32D0 == g_matrixTarget);
    CHECK(g_c69490 == 1 && g_c307A0 == 1);
    CHECK(g_pMtxSeen == g_env.aMtx6C29A8);
    CHECK(g_pMtxDst  == g_matrixTarget);

    /* The word is ZERO-extended into the second dword. */
    EnvReset();
    g_env.w6C067C = 0xBEEFu;
    BrSub_1003407D(1.0f, 1.0f);
    CHECK(g_aDl[0].w1 == 0xBEEFu);

    /* GOTCHA: a zero divisor is not checked; x87 has the exception masked so
     * the original produces an infinity rather than trapping. */
    EnvReset();
    BrSub_1003407D(0.0f, 1.0f);
    CHECK(g_env.aMtx6C29A8[0] > 0.0f && !(g_env.aMtx6C29A8[0] < 1e30f));
}

/* =====================================================================
 * 0x1001BE90
 * ===================================================================== */
static void SetupFill(void)
{
    EnvReset();
    g_env.n4C516C = 0;    g_env.n4C5170 = 0;
    g_env.n4C5164 = 639;  g_env.n4C01A0 = 479;
    g_env.n4C1694 = 0x504340;   /* the Direct3D arm */
    g_env.n4C5158 = 0u;         /* not the magic pair -> the byte source */
    g_env.n4C515C = 0u;
    g_env.b4BBF00 = 0x11;  /* R */
    g_env.b4BC194 = 0x22;  /* G */
    g_env.b4C5150 = 0x33;  /* B */
    g_env.b4C15CC = 0x44;  /* A */
}

static void Test1BE90D3D(void)
{
    int i;
    float xlo, xhi, ylo, yhi;

    SetupFill();
    BrSub_1001BE90(10, 20, 100, 200);

    /* Two triangles, six D3DTLVERTEX. */
    CHECK(g_cDraw == 2);
    CHECK(g_cVert == 6);

    /* The colour is packed A:R:G:B. */
    CHECK(g_aVert[0].diffuse  == 0x44112233u);
    CHECK(g_aVert[0].specular == 0xFF0000FFu);

    /* Geometry: every vertex is a corner of the rect with y flipped about
     * 0x100A81C4, and all four corners appear. */
    xlo = 10.0f; xhi = 100.0f;
    ylo = (float)(480 - 200); yhi = (float)(480 - 20);
    for (i = 0; i < 6; ++i) {
        CHECK(g_aVert[i].x == xlo || g_aVert[i].x == xhi);
        CHECK(g_aVert[i].y == ylo || g_aVert[i].y == yhi);
        CHECK(g_aVert[i].z == 0.0f);
        CHECK(g_aVert[i].rhw == 1.0f);
        /* The texture coordinate always tracks the corner. */
        CHECK(g_aVert[i].tu == ((g_aVert[i].x == xhi) ? 1.0f : 0.0f));
        CHECK(g_aVert[i].tv == ((g_aVert[i].y == yhi) ? 1.0f : 0.0f));
    }

    /* After the call the cache is consistent: nothing dirty, and every state
     * the function published has been applied. */
    CHECK(g_env.nDirty277370 == 0u);
    for (i = 0; i < BR72_RS_COUNT; ++i) {
        CHECK(g_env.aHave2773F8[i] == g_env.aWant277378[i]);
    }
    CHECK(g_env.aWant277378[2] == 5 && g_env.aWant277378[3] == 6);
    CHECK(g_env.aWant277378[5] == 8 && g_env.aWant277378[7] == 8);
    CHECK(g_env.aWant277378[10] == 1);
    CHECK(g_env.n4BBE28 == 8 && g_env.n4C16A0 == 8);

    /* The last render-state write restores 0x16 from the APPLIED cache. */
    CHECK(g_cRs >= 2);
    CHECK(g_aRs[g_cRs - 1].state == 0x16);
    CHECK(g_aRs[g_cRs - 1].value == g_env.aHave2773F8[4]);

    /* A second identical call finds nothing dirty and so issues no cache
     * flush at all -- only the three unconditional states. */
    {
        int cFirst = g_cRs;
        g_cRs = 0;
        BrSub_1001BE90(10, 20, 100, 200);
        CHECK(cFirst > g_cRs);
        CHECK(g_cRs == 3);
        CHECK(g_aRs[0].state == 1    && g_aRs[0].value == 0);
        CHECK(g_aRs[1].state == 0x16 && g_aRs[1].value == 1);
    }
}

static void Test1BE90Clamp(void)
{
    SetupFill();
    g_env.n4C516C = 50;   /* x1 floor */
    g_env.n4C5170 = 60;   /* y1 floor */
    g_env.n4C5164 = 200;  /* x2 ceiling */
    g_env.n4C01A0 = 300;  /* y2 ceiling */

    BrSub_1001BE90(0, 0, 999, 999);

    CHECK(g_aVert[0].x == 50.0f);
    CHECK(g_aVert[1].x == 200.0f);
    /* y1 = 60 and y2 = 300 after clamping, flipped about 480. */
    CHECK(g_aVert[0].y == (float)(480 - 300));
    CHECK(g_aVert[1].y == (float)(480 - 60));
}

static void Test1BE90Float(void)
{
    SetupFill();
    /* The two magic numbers select the float colour source; __ftol
     * truncates toward zero, so 0.999*255 == 254. */
    g_env.n4C5158 = 0xFCFFFFFFu;
    g_env.n4C515C = 0xFFFDF6FBu;
    g_env.f4C5154 = 1.0f;        /* R */
    g_env.f4C5160 = 0.5f;        /* G -> 127 */
    g_env.f4C1690 = 0.0f;        /* B */
    g_env.f4C0BA8 = 0.999f;      /* A -> 254, not 255 */
    BrSub_1001BE90(0, 0, 10, 10);
    CHECK(g_aVert[0].diffuse == 0xFEFF7F00u);
}

static void Test1BE90Soft(void)
{
    SetupFill();
    g_env.n4C1694 = 0;           /* not the magic value -> the other arm */
    g_env.n4C518C = 0;
    g_surf.f50 = 0x5150;

    BrSub_1001BE90(10, 20, 100, 200);

    CHECK(g_cDraw == 0);         /* no Direct3D at all on this arm */
    CHECK(g_cT20 == 1 && g_cT30 == 1);
    CHECK(g_nT20Arg == 0x5150);
    CHECK(g_surf.f4C == 1 && g_surf.f58 == 1);
    CHECK(g_c1C620 == 1);

    /* The rect is {x1, h-y2, x2, h-y1}. */
    CHECK(g_aRectSeen[0] == 10);
    CHECK(g_aRectSeen[1] == 480 - 200);
    CHECK(g_aRectSeen[2] == 100);
    CHECK(g_aRectSeen[3] == 480 - 20);

    /* The colour is scaled by 1/255, not 1/256. */
    CHECK(fabs((double)g_surf.f04 - 0x11 / 255.0) < 1e-6);
    CHECK(fabs((double)g_surf.f08 - 0x22 / 255.0) < 1e-6);
    CHECK(fabs((double)g_surf.f0C - 0x33 / 255.0) < 1e-6);

    /* The pending-geometry flush only fires when there is something to flush,
     * and it is the same on both arms. */
    SetupFill();
    g_env.n4C1694 = 0;
    g_env.n4C518C = 3;
    BrSub_1001BE90(0, 0, 1, 1);
    CHECK(g_cDrawIndexed == 1 && g_c1C640 == 1);
}

/* =====================================================================
 * 0x10005B10
 * ===================================================================== */
static void Test5B10(void)
{
    int i, j;

    EnvReset();
    BrSub10005B10(1);

    CHECK(g_cMutex == BR72_MUTEX_BANK + BR72_MUTEX_EXTRA);   /* 26 */
    CHECK(g_c75100 == 1 && g_c5960 == 1);
    CHECK(g_env.n221310 == 0 && g_env.n220DD8 == 0);

    /* Every slot got its own handle: no slot was left NULL and none repeats. */
    for (i = 0; i < BR72_MUTEX_BANK; ++i) {
        CHECK(g_env.aMutexBank[i] != NULL);
        for (j = 0; j < i; ++j) {
            CHECK(g_env.aMutexBank[i] != g_env.aMutexBank[j]);
        }
    }
    for (i = 0; i < BR72_MUTEX_EXTRA; ++i) {
        CHECK(g_env.aMutexExtra[i] != NULL);
    }
    /* The first four extras are created BEFORE 0x10075100 and the last six
     * after it, so the handle numbering straddles the call. */
    CHECK(g_env.aMutexExtra[3] < g_env.aMutexExtra[4]);
}

/* =====================================================================
 * 0x100440D0 -- the thunk
 * ===================================================================== */
static void Test440D0(void)
{
    EnvReset();
    BrExt_100440D0(1);
    CHECK(g_c294C == 1);
    BrExt_100440D0(0);
    CHECK(g_c294C == 2);   /* the ignored argument changes nothing */
}

/* =====================================================================
 * 0x1003CDA0
 * ===================================================================== */
static void Test3CDA0(void)
{
    /* No DirectPlay object: nothing happens at all. */
    EnvReset();
    g_env.pDPlay277B40 = NULL;
    BrExt_1003CDA0();
    CHECK(g_c3D0B0 == 0 && g_cSetDesc == 0 && g_cFree == 0);

    /* Success: the four user fields are published, 0x10044540 runs, and the
     * descriptor is released exactly once. */
    EnvReset();
    g_env.n0B380C = 1; g_env.n22B350 = 2;
    g_env.nAA2A18 = 3; g_env.n0AC658 = 4;
    g_env.nAA2A44 = -1;
    g_hr3D0B0 = 0; g_hrSetDesc = 0;
    BrExt_1003CDA0();
    CHECK(g_desc.dwUser1 == 1 && g_desc.dwUser2 == 2);
    CHECK(g_desc.dwUser3 == 3 && g_desc.dwUser4 == 4);
    CHECK(g_cSetDesc == 1);
    CHECK(g_cUnlock == 1 && g_cFree == 1);
    /* GlobalHandle is called twice on the same pointer, as the original does. */
    CHECK(g_cHandle == 2);
    /* BrSub10044540 ran with the descriptor's dwUser3 selector (3). */
    CHECK(g_env.n0AB3E8 == 0x202C && g_env.n0AC654 == 3);

    /* SetSessionDesc fails: still released. */
    EnvReset();
    g_hr3D0B0 = 0; g_hrSetDesc = (int32_t)0x80004005;
    BrExt_1003CDA0();
    CHECK(g_cSetDesc == 1 && g_cFree == 1);

    /* The builder fails and hands back no descriptor: nothing is released,
     * and SetSessionDesc is never reached. */
    EnvReset();
    g_hr3D0B0 = (int32_t)0x80004005;
    g_f3D0B0Null = 1;
    BrExt_1003CDA0();
    CHECK(g_cSetDesc == 0 && g_cFree == 0 && g_cHandle == 0);
}

/* =====================================================================
 * The builders -- shared invariants
 * ===================================================================== */

/* Every f38 site in the family passes 2 and 5 as a4/a5 and the PHASE (never
 * the screen) as the owner; every control that received an f34 also received
 * an f38 first; and each control appears at most once in each list. */
static void CheckFamilyInvariants(const BrPhase_ *pPhase)
{
    int i, j;

    CHECK(g_c38 <= REC_MAX && g_c34 <= REC_MAX);
    for (i = 0; i < g_c38; ++i) {
        CHECK(g_a38[i].a4 == 2);
        CHECK(g_a38[i].a5 == 5);
        CHECK(g_a38[i].pOwner == (const void *)pPhase);
        CHECK(g_a38[i].pCtl != NULL);
        for (j = 0; j < i; ++j) {
            CHECK(g_a38[j].pCtl != g_a38[i].pCtl);
        }
    }
    for (i = 0; i < g_c34; ++i) {
        int fPlaced = 0;
        for (j = 0; j < g_c38; ++j) {
            if (g_a38[j].pCtl == g_a34[i].pCtl) { fPlaced = 1; }
        }
        CHECK(fPlaced);
        CHECK(g_a34[i].pStyle != NULL);
    }
}

/* Every registered slot is non-NULL and cCtl counts exactly the controls the
 * builder placed; cSel never exceeds cCtl. */
static void CheckPageConsistent(const BrUiPage_ *pScr)
{
    unsigned i;
    CHECK(pScr->cCtl <= BR72_PAGE_CTL_MAX);
    CHECK(pScr->cSel <= pScr->cCtl);
    for (i = 0; i < pScr->cCtl; ++i) {
        CHECK(pScr->apCtl[i] != NULL);
        CHECK(pScr->apCtl[i]->pVtbl == &g_ctlVtbl);
    }
    CHECK(pScr->f10 == 0);
    CHECK(pScr->fY == 130.0f);
}

static int TotalCtl(const BrPhase_ *pPhase)
{
    unsigned i;
    int n = 0;
    for (i = 0; i < pPhase->nPages && i < BR_PHASE_PAGES; ++i) {
        n += (int)pPhase->aPages[i]->cCtl;
    }
    return n;
}

static void Test56A10(void)
{
    BrPhase_ ph;
    BrUiPage_ *pScr;

    EnvReset();
    memset(&ph, 0, sizeof(ph));
    strcpy(g_env.szA9CDF0, "Team Name");

    BrOptFn10056A10(&ph);

    CHECK(ph.nPages == 1);
    CHECK(ph.iPage  == 0);
    CHECK(ph.aFlags[0] == 1);
    pScr = ph.aPages[0];
    CHECK(pScr != NULL);
    CheckPageConsistent(pScr);
    CheckFamilyInvariants(&ph);

    /* GOTCHA: the only builder in the family whose fX is 190. */
    CHECK(pScr->fX == 190.0f);
    CHECK(pScr->pOwner == &ph);

    CHECK(pScr->cCtl == 8);
    CHECK(pScr->cSel == 3);
    CHECK(g_c38 == 8);
    CHECK(TotalCtl(&ph) == 8);

    /* The root is placed at the origin with a6 = a7 = 0. */
    CHECK(g_a38[0].x == 0.0f && g_a38[0].y == 0.0f);
    CHECK(g_a38[0].flags == 9 && g_a38[0].a6 == 0 && g_a38[0].a7 == 0);

    /* The title. */
    CHECK(g_a38[1].y == 10.0f && g_a38[1].x == 190.0f);
    CHECK(StrIdOf(g_a34[0].pText) == 0x5D);
    CHECK(g_a34[0].pStyle == g_env.p0AB508);

    /* GOTCHA: control 3 uses f1E20C 0x34 and f34's third argument 4. */
    CHECK(g_a34[1].a3 == 4);
    CHECK(pScr->apCtl[2]->w1E20C == 0x34);

    /* The item block: text copied from 0x10A9CDF0, the item hook fired, and
     * the rectangle and its item mirror agree. */
    {
        const BrUiCtl_ *p = pScr->apCtl[4];
        CHECK(strcmp(p->aText[0].sz, "Team Name") == 0);
        CHECK(g_cItemF04 == 1);
        CHECK(p->rcLeft == 0x9B && p->aText[0].left == 0x9B);
        CHECK(p->rcTop == 0xAC && p->aText[0].f428 == 0xAC);
        CHECK(p->rcRight == 0x15B && p->aText[0].right == 0x15B);
        CHECK(p->rcBottom == 0xBC && p->aText[0].f430 == 0xBC);
        /* GOTCHA: the width is 16-bit wide: 0x15B - 0x9B - 0x10. */
        CHECK(p->aText[0].f41C == (uint16_t)(0x15B - 0x9B - 0x10));
        CHECK(p->pfn10 == g_hooks.p1003F020);   /* +0x10, only here */
    }

    /* GOTCHA: control 6 has flags 0x102011, not 0x102001, and its label goes
     * out with a3 = 0. */
    CHECK(g_a38[5].flags == 0x102011);
    CHECK(g_a34[3].a3 == 0);
    CHECK(pScr->apCtl[5]->w1E20C == 2);
    CHECK(g_env.pAA29E8 == pScr->apCtl[5]);

    /* The two row offsets are additions: fY + 95 then fY + 114. */
    CHECK(g_a38[5].y == 130.0f + 95.0f);
    CHECK(g_a38[6].y == 130.0f + 114.0f);

    /* The last control is placed only. */
    CHECK(g_a38[7].x == 80.0f && g_a38[7].y == 46.0f && g_a38[7].a7 == 7);

    PhaseFree(&ph);
}

static void Test57C10(void)
{
    BrPhase_ ph;
    BrUiPage_ *pScr;
    int cWith, cWithout;

    /* GOTCHA: the four extra controls need 0x1022AF18 == 2 EXACTLY. */
    EnvReset();
    memset(&ph, 0, sizeof(ph));
    g_env.n22AF18 = 2;
    BrOptFn10057C10(&ph);
    pScr = ph.aPages[0];
    CheckPageConsistent(pScr);
    CheckFamilyInvariants(&ph);
    CHECK(pScr->fX == 195.0f);
    cWith = (int)pScr->cCtl;
    CHECK(cWith == 16);
    CHECK(pScr->cSel == 6);
    /* The gated rows walk -19, -38, -57 relative to fY, and the first sits on
     * fY itself. */
    CHECK(g_a38[2].y == 130.0f);
    CHECK(g_a38[3].y == 130.0f + 19.0f);
    CHECK(g_a38[4].y == 130.0f + 38.0f);
    CHECK(g_a38[5].y == 130.0f + 57.0f);
    /* GOTCHA: the first gated row is the one control whose +0x0C hook is not
     * 0x10047360. */
    CHECK(pScr->apCtl[2]->pfn0C == g_hooks.p100474B0);
    CHECK(pScr->apCtl[3]->pfn0C == g_hooks.p10047360);
    PhaseFree(&ph);

    EnvReset();
    memset(&ph, 0, sizeof(ph));
    g_env.n22AF18 = 1;              /* non-zero but not 2 */
    BrOptFn10057C10(&ph);
    pScr = ph.aPages[0];
    cWithout = (int)pScr->cCtl;
    CHECK(cWithout == cWith - 4);
    CHECK(pScr->cSel == 2);
    PhaseFree(&ph);

    /* The two id swaps.  Both take their alternate id when the global is
     * non-zero. */
    EnvReset();
    memset(&ph, 0, sizeof(ph));
    g_env.nAA2884 = 0; g_env.nA9D000 = 0;
    BrOptFn10057C10(&ph);
    CHECK(StrIdOf(g_a34[1].pText) == 0x1E);
    CHECK(StrIdOf(g_a34[2].pText) == 0x0C);
    PhaseFree(&ph);

    EnvReset();
    memset(&ph, 0, sizeof(ph));
    g_env.nAA2884 = 1; g_env.nA9D000 = 1;
    BrOptFn10057C10(&ph);
    CHECK(StrIdOf(g_a34[1].pText) == 0x66);
    CHECK(StrIdOf(g_a34[2].pText) == 0x67);
    /* Only this builder writes +0x18. */
    CHECK(ph.aPages[0]->apCtl[2]->pfn18 == g_hooks.p100437D0);
    CHECK(g_env.pAA29B8 == ph.aPages[0]->apCtl[3]);
    PhaseFree(&ph);
}

static void Test52030(void)
{
    BrPhase_ ph;
    BrUiPage_ *pScr;
    const BrUiCtl_ *r1, *r2, *r3;

    EnvReset();
    memset(&ph, 0, sizeof(ph));
    g_env.nAB428 = 100;      /* fild -- an INT, not a float */
    g_env.nAB42C = 200;
    BrExt_10052030(&ph);

    pScr = ph.aPages[0];
    CheckPageConsistent(pScr);
    CheckFamilyInvariants(&ph);
    CHECK(pScr->fX == 195.0f);
    CHECK(pScr->cCtl == 23);
    CHECK(pScr->cSel == 3);      /* only the three 0x102001 rows */

    /* The row constants run -19, -95, -114: three slots are skipped. */
    CHECK(g_a38[2].y == 130.0f + 19.0f);
    CHECK(g_a38[3].y == 130.0f + 95.0f);
    CHECK(g_a38[4].y == 130.0f + 114.0f);

    /* GOTCHA: control 4 alone uses f1E20C 2 and f34 a3 = 0. */
    CHECK(pScr->apCtl[3]->w1E20C == 2);
    CHECK(g_a34[2].a3 == 0);

    /* The three rects.  fA/fB come from the two INT globals. */
    r1 = pScr->apCtl[5];
    r2 = pScr->apCtl[6];
    r3 = pScr->apCtl[7];
    CHECK(g_a38[5].x == 100.0f && g_a38[5].y == 200.0f);
    CHECK(r1->rcLeft == 100 && r1->rcTop == 200);
    CHECK(r1->rcRight == 100 + 0x7F);
    CHECK(r1->rcBottom == 200 + 0x21);
    /* GOTCHA: rect 2 reuses rect 1's truncated x and right edge. */
    CHECK(r2->rcLeft == r1->rcLeft && r2->rcRight == r1->rcRight);
    /* ... and its y cursor advanced by 33. */
    CHECK(g_a38[6].y == 233.0f);
    CHECK(r2->rcTop == 233);
    /* Rect 3 steps once more and then, uniquely, does NOT advance the cursor
     * afterwards -- unobservable, since nothing after it reads fB. */
    CHECK(g_a38[7].y == 266.0f);
    CHECK(r3->rcTop == 266);
    CHECK(r3->rcLeft == r2->rcLeft && r3->rcRight == r2->rcRight);
    CHECK(r3->rcBottom == 266 + 0x21);
    /* f2A42 is always a7 + 1 on the rects. */
    CHECK(r1->aStepId[1] == (uint16_t)(g_a38[5].a7 + 1));
    CHECK(r2->aStepId[1] == (uint16_t)(g_a38[6].a7 + 1));
    CHECK(r3->aStepId[1] == (uint16_t)(g_a38[7].a7 + 1));
    CHECK(r1->f2968 == 0 && r2->f2968 == 0 && r3->f2968 == 0);

    /* GOTCHA: the last control's row constant is POSITIVE, so it moves UP. */
    CHECK(g_a38[g_c38 - 1].y == 130.0f - 19.0f);
    CHECK(pScr->apCtl[22]->w1E20C == 0x34);

    /* The seven label sites that take a text POINTER, not a string id. */
    {
        int i, cPtr = 0;
        for (i = 0; i < g_c34; ++i) {
            if (g_a34[i].pText == g_env.p0AD300 ||
                g_a34[i].pText == g_env.p39B720) {
                ++cPtr;
                CHECK(StrIdOf(g_a34[i].pText) == -1);
            }
        }
        CHECK(cPtr == 7);
    }

    /* This builder publishes no pointer global at all. */
    CHECK(g_env.pAA29B8 == NULL && g_env.pAA29C4 == NULL);
    CHECK(g_env.pAA29C8 == NULL && g_env.pAA29E8 == NULL);

    PhaseFree(&ph);
}

static void Test59760(void)
{
    BrPhase_ ph;
    BrUiPage_ *pScr;

    EnvReset();
    memset(&ph, 0, sizeof(ph));
    BrExt_10059760(&ph);

    pScr = ph.aPages[0];
    CheckPageConsistent(pScr);
    CheckFamilyInvariants(&ph);
    CHECK(pScr->fX == 195.0f);
    CHECK(pScr->cCtl == 6);
    CHECK(pScr->cSel == 3);

    /* Rows: fY, fY+19, then straight to fY+114. */
    CHECK(g_a38[2].y == 130.0f);
    CHECK(g_a38[3].y == 130.0f + 19.0f);
    CHECK(g_a38[4].y == 130.0f + 114.0f);

    /* GOTCHA: the last control breaks the pattern four ways at once. */
    CHECK(g_a38[5].x == 80.0f && g_a38[5].y == 46.0f);
    CHECK(g_a38[5].flags == 9 && g_a38[5].a6 == 0 && g_a38[5].a7 == 8);
    CHECK(pScr->apCtl[5]->w1E20C == 0);
    CHECK(pScr->apCtl[5]->pfn08 == NULL);
    /* Four labels, none for the root or the last control. */
    CHECK(g_c34 == 4);

    /* This builder never touches pfn04 or pfn18. */
    {
        unsigned i;
        for (i = 0; i < pScr->cCtl; ++i) {
            CHECK(pScr->apCtl[i]->pfn04 == NULL);
            CHECK(pScr->apCtl[i]->pfn18 == NULL);
        }
    }

    PhaseFree(&ph);
}

static void Test5A6E0(void)
{
    BrPhase_ ph;
    BrUiPage_ *pS0, *pS1;

    EnvReset();
    memset(&ph, 0, sizeof(ph));
    g_env.pAA2908 = &ph;         /* the receiver is the GLOBAL phase */
    ph.fC4 = &g_grf;

    BrExt_1005A6E0(&ph);

    /* The extra prologue. */
    CHECK(g_cGrfEnum == 1);
    CHECK(g_env.n0AB3F4 == -1);
    CHECK(g_env.nAA28E8 == 0);

    /* TWO screens. */
    CHECK(ph.nPages == 2);
    pS0 = ph.aPages[0];
    pS1 = ph.aPages[1];
    CHECK(pS0 != NULL && pS1 != NULL && pS0 != pS1);
    CheckPageConsistent(pS0);
    CheckPageConsistent(pS1);
    CheckFamilyInvariants(&ph);

    /* GOTCHA: screen 2's flag is ZERO where every other screen writes 1. */
    CHECK(ph.aFlags[0] == 1);
    CHECK(ph.aFlags[1] == 0);

    CHECK(pS0->cCtl == 9);
    CHECK(pS0->cSel == 2);
    CHECK(pS1->cCtl == 1);
    CHECK(pS1->cSel == 0);

    /* The list is configured once and fed all 100 slots -- GOTCHA: the
     * guard tests an ADDRESS, so the empty ones go in too. */
    CHECK(g_cSubF14 == 1);
    CHECK(g_cSubF10 == BR72_GRF_COUNT);
    CHECK(pS0->apCtl[2]->list.f1A99C[8].i == 1);
    CHECK(pS0->apCtl[2]->list.f04 == g_hooks.p10042740);
    CHECK(pS0->apCtl[2]->list.f14 == g_hooks.p10042560);

    /* The one row offset is -95; the readout column uses absolute y. */
    CHECK(g_a38[3].y == 130.0f + 95.0f);
    CHECK(g_a38[4].y == 208.0f && g_a38[4].x == 440.0f);
    CHECK(g_a38[5].y == 224.0f);
    CHECK(g_a38[6].y == 240.0f);
    CHECK(g_a38[7].y == 256.0f);

    /* GOTCHA: screen 2's only control uses the literal 0.0f for x, not the
     * screen's fX, which is 195 and never read. */
    CHECK(pS1->fX == 195.0f);
    CHECK(g_a38[g_c38 - 1].x == 0.0f);
    CHECK(g_a38[g_c38 - 1].y == 232.0f);
    CHECK(pS1->apCtl[0]->pfn14 == g_hooks.p1003E7A0);   /* +0x14, only here */
    CHECK(g_env.pAA29C4 == pS1->apCtl[0]);

    PhaseFree(&ph);
}

static void Test4E830(void)
{
    BrPhase_ ph;
    BrUiPage_ *pScr;

    /* Force feedback present. */
    EnvReset();
    memset(&ph, 0, sizeof(ph));
    g_env.n18ABDBC = 1;
    BrExt_1004E830(&ph);

    pScr = ph.aPages[0];
    CheckPageConsistent(pScr);
    CheckFamilyInvariants(&ph);
    CHECK(pScr->fX == 195.0f);
    CHECK(pScr->cCtl == 16);
    CHECK(pScr->cSel == 5);
    CHECK(g_c795D0 == 1);

    CHECK(g_a38[2].flags == 0x102001);
    CHECK(pScr->apCtl[2]->w1E20C == 3);
    CHECK(g_a34[1].a3 == 1);
    CHECK(StrIdOf(g_a34[1].pText) == 0x30);

    /* Rows -19, -38, -57, then a jump to -114. */
    CHECK(g_a38[2].y == 130.0f);
    CHECK(g_a38[3].y == 130.0f + 19.0f);
    CHECK(g_a38[4].y == 130.0f + 38.0f);
    CHECK(g_a38[5].y == 130.0f + 57.0f);
    CHECK(g_a38[6].y == 130.0f + 114.0f);
    CHECK(g_env.pAA29C8 == pScr->apCtl[6]);
    PhaseFree(&ph);

    /* Force feedback absent: the SAME string id, but different flags,
     * different f1E20C and a different third argument. */
    EnvReset();
    memset(&ph, 0, sizeof(ph));
    g_env.n18ABDBC = 0;
    BrExt_1004E830(&ph);
    pScr = ph.aPages[0];
    CHECK(pScr->cCtl == 16);          /* the control count does not change */
    CHECK(pScr->cSel == 5);
    CHECK(g_a38[2].flags == 0x102011);
    CHECK(pScr->apCtl[2]->w1E20C == 2);
    CHECK(g_a34[1].a3 == 0);
    CHECK(StrIdOf(g_a34[1].pText) == 0x30);
    PhaseFree(&ph);
}

/* Allocation failure.  In the original, error index 4 is FATAL, so BrErrShow
 * does not return and the NULL dereference that follows is unreachable; the
 * port returns instead.  Driving a constructor to NULL exercises exactly the
 * same branch as an allocator that returns NULL.
 *
 * Both sites are covered: the page and the first control. */
static BrUiPage_ *NullPageCtor(BrUiPage_ *p) { free(p); return NULL; }
static BrUiCtl_ *NullCtlCtor(BrUiCtl_ *p)  { free(p); return NULL; }

static void TestAllocFailure(void)
{
    BrPhase_ ph;

    /* The page fails.  The counter still advances and the slot still holds
     * the NULL -- both are what the original does before it faults. */
    EnvReset();
    memset(&ph, 0, sizeof(ph));
    ph.nPages = 2;
    g_env.pfnPageCtor = NullPageCtor;
    BrExt_10059760(&ph);
    CHECK(ph.nPages == 3);
    CHECK(ph.aPages[2] == NULL);
    CHECK(ph.aFlags[2] == 1);
    CHECK(ph.iPage == 0);
    CHECK(g_cErr == 1 && g_idErrLast == 4);
    CHECK(g_c38 == 0);            /* nothing was placed */

    /* The first control fails.  The page exists, the slot holds the NULL,
     * and the counter has NOT moved -- the original increments cCtl only
     * after a control has been configured. */
    EnvReset();
    memset(&ph, 0, sizeof(ph));
    g_env.pfnCtlCtor = NullCtlCtor;
    BrExt_10059760(&ph);
    CHECK(ph.nPages == 1);
    CHECK(ph.aPages[0] != NULL);
    CHECK(ph.aPages[0]->cCtl == 0);
    CHECK(ph.aPages[0]->apCtl[0] == NULL);
    CHECK(g_cErr == 1 && g_idErrLast == 4);
    CHECK(g_c38 == 0);
    free(ph.aPages[0]);

    /* The allocation sizes are never the bare 32-bit literals: on this host
     * the structs may be larger, and the port must ask for the larger of the
     * two so a 64-bit build cannot under-allocate. */
    EnvReset();
    memset(&ph, 0, sizeof(ph));
    BrExt_10059760(&ph);
    CHECK(g_cbNewLast >= BR72_CTL_ORIG_SIZE);
    CHECK(g_cbNewLast >= sizeof(BrUiCtl_));
    CHECK(BR72_ALLOC(BrUiPage_, BR72_PAGE_ORIG_SIZE) >= sizeof(BrUiPage_));
    CHECK(BR72_ALLOC(BrUiPage_, BR72_PAGE_ORIG_SIZE) >= BR72_PAGE_ORIG_SIZE);
    /* The error reporter is silent on the happy path. */
    CHECK(g_cErr == 0);
    PhaseFree(&ph);
}

/* =====================================================================
 * main
 * ===================================================================== */
int main(void)
{
    Test44540();
    Test3E3A0();
    Test35FC0();
    Test5B0C0();
    Test771B0();
    Test2B2A0();
    Test3407D();
    Test1BE90D3D();
    Test1BE90Clamp();
    Test1BE90Float();
    Test1BE90Soft();
    Test5B10();
    Test440D0();
    Test3CDA0();
    Test56A10();
    Test57C10();
    Test52030();
    Test59760();
    Test5A6E0();
    Test4E830();
    TestAllocFailure();

    printf("%s: %d checks, %d failures\n",
           g_nFail ? "FAIL" : "PASS", g_nRun, g_nFail);
    return g_nFail ? 1 : 0;
}
