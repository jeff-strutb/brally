/* test_slice3_32.c -- behaviour tests for port/src/slice3_32.c.
 *
 * Every cross-slice callee this packet needs is stubbed HERE and nowhere
 * else, per the contract. The stubs record what they were handed so the
 * tests can assert on argument order and on call ORDER, which is where this
 * range's real content lives.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "slice3_32.h"

static int g_nFail = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);         \
            ++g_nFail;                                                     \
        }                                                                  \
    } while (0)

/* ==========================================================================
 * Cross-slice stand-ins
 * ========================================================================== */

int32_t BrFtolTrunc(float f)
{
    /* __ftol: truncate toward zero. */
    return (int32_t)f;
}

static int32_t s_nTick = 0;
static int32_t s_nTickStep = 0;

int32_t BrSub10075020(void)
{
    int32_t v = s_nTick;
    s_nTick += s_nTickStep;
    return v;
}

static int   s_nSleep = 0;
void BrScrSleep(uint32_t ms) { (void)ms; ++s_nSleep; }

static void *s_apDeleted[64];
static int   s_nDeleted = 0;
void BrOperatorDelete(void *p)
{
    if (s_nDeleted < (int)(sizeof s_apDeleted / sizeof s_apDeleted[0]))
        s_apDeleted[s_nDeleted] = p;
    ++s_nDeleted;
}

static int s_n484E0 = 0;
void BrSub100484E0(BrUiPage *pThis) { (void)pThis; ++s_n484E0; }

/* --- 0x1005F5A0: record the five arguments -------------------------------- */
static struct {
    int         nCalls;
    int32_t     x, y, id, f14;
    const void *pRect;
} s_Draw;

void BrSub1005F5A0(int32_t x, int32_t y, int32_t id,
                   const void *pRect, int32_t f14)
{
    ++s_Draw.nCalls;
    s_Draw.x = x; s_Draw.y = y; s_Draw.id = id;
    s_Draw.pRect = pRect; s_Draw.f14 = f14;
}

static int s_n3E310 = 0, s_n6A4A0 = 0, s_n60260 = 0, s_nDik = 0;
static int s_n5F530 = 0, s_n5FCF0 = 0, s_n8B80 = 0;
static int s_n72AF0a = -1, s_n72AF0b = -1, s_n72AF0n = 0;
static int s_nCdTrack = 40;

void BrSub1003E310(void)                       { ++s_n3E310; }
void BrSub1006A4A0(void *pThis, void *pArg)    { (void)pThis; (void)pArg; ++s_n6A4A0; }
void BrSub10060260(void *pThis)                { (void)pThis; ++s_n60260; }
void BrDikPollAndEdge(void)                    { ++s_nDik; }
void BrSub1005F530(void)                       { ++s_n5F530; }
void BrSub1005FCF0(void)                       { ++s_n5FCF0; }
int  BrCdTrackGet(void)                        { return s_nCdTrack; }
void BrSub10072AF0(int a, int b)               { s_n72AF0a = a; s_n72AF0b = b; ++s_n72AF0n; }
void BrStub8B80_1p(const void *p0)             { (void)p0; ++s_n8B80; }

/* --- the two vtable data symbols ------------------------------------------ */
static int s_nPageDtor = 0;
static void *PageVtblF00(BrUiPage *p, int32_t n) { (void)p; (void)n; ++s_nPageDtor; return p; }
static int32_t PageVtblF04(BrUiPage *p)          { (void)p; return 1; }
const BrUiPageVtbl BrUiPageVtbl_1008F6F8 = { PageVtblF00, PageVtblF04 };

static void *PhaseF00(BrPhaseFull *p, int32_t n) { (void)p; (void)n; return p; }
static int32_t PhaseF04(BrPhaseFull *p)          { (void)p; return 1; }
static int32_t PhaseF08(BrPhaseFull *p)          { (void)p; return 1; }
static int32_t PhaseF0C(BrPhaseFull *p)          { (void)p; return 1; }
static void PhaseF14(BrPhaseFull *p)             { (void)p; }
static void PhaseF18(BrPhaseFull *p, void *a)    { (void)p; (void)a; }
static void PhaseF1C(BrPhaseFull *p)             { (void)p; }
static void PhaseF20(BrPhaseFull *p)             { (void)p; }
const BrPhaseFullVtbl BrPhaseVtbl_1008F700 = {
    PhaseF00, PhaseF04, PhaseF08, PhaseF0C, NULL,
    PhaseF14, PhaseF18, PhaseF1C, PhaseF20
};

/* ==========================================================================
 * Local test fixtures
 * ========================================================================== */

static BrUiObj *NewObj(void)
{
    BrUiObj *p = (BrUiObj *)calloc(1, BR_SCR_UIOBJ_ALLOC);
    if (p == NULL) { printf("FAIL out of memory\n"); exit(1); }
    return p;
}

/* --- a recording BrUiObj vtable ------------------------------------------- */
static int   s_aUi[16];              /* per-slot call counts, index = slot/4 */
static int   s_aUiSeq[64];
static int   s_nUiSeq = 0;
static float s_fCurve = 0.0f;        /* what slot +0x28 returns              */
static int32_t s_nRet0C = 1, s_nRet10 = 1, s_nRet20 = 1, s_nRet3C = 0;

static void UiNote(int slot)
{
    ++s_aUi[slot / 4];
    if (s_nUiSeq < 64) s_aUiSeq[s_nUiSeq++] = slot;
}
static void   UiF04(BrUiObj *p) { (void)p; UiNote(0x04); }
static void   UiF08(BrUiObj *p) { (void)p; UiNote(0x08); }
static int32_t UiF0C(BrUiObj *p) { (void)p; UiNote(0x0C); return s_nRet0C; }
static int32_t UiF10(BrUiObj *p) { (void)p; UiNote(0x10); return s_nRet10; }
static void   UiF18(BrUiObj *p, void *q) { (void)p; (void)q; UiNote(0x18); }
static void   UiF1C(BrUiObj *p) { (void)p; UiNote(0x1C); }
static int32_t UiF20(BrUiObj *p) { (void)p; UiNote(0x20); return s_nRet20; }
static float  UiF28(BrUiObj *p, int32_t ms) { (void)p; (void)ms; UiNote(0x28); return s_fCurve; }
static void   UiF30(BrUiObj *p) { (void)p; UiNote(0x30); }
static int32_t UiF3C(BrUiObj *p) { (void)p; UiNote(0x3C); return s_nRet3C; }

static const BrScrUiVtbl s_UiVtbl = {
    NULL, UiF04, UiF08, UiF0C, UiF10, NULL, UiF18, UiF1C,
    UiF20, NULL, UiF28, NULL, UiF30, NULL, NULL, UiF3C
};

/* --- a recording item vtable ---------------------------------------------- */
static int s_nItem04 = 0, s_nItem08 = 0, s_nItem10 = 0, s_nItem28 = 0;
static void ItemF04(void *p) { (void)p; ++s_nItem04; }
static void ItemF08(void *p) { (void)p; ++s_nItem08; }
static void ItemF10(void *p) { (void)p; ++s_nItem10; }
static float ItemF28(void *p) { (void)p; ++s_nItem28; return 3.5f; }
static const BrScrItemVtbl s_ItemVtbl = {
    NULL, ItemF04, ItemF08, NULL, ItemF10, NULL, NULL, NULL,
    NULL, NULL, ItemF28
};

static void ResetUi(void)
{
    memset(s_aUi, 0, sizeof s_aUi);
    s_nUiSeq = 0;
    s_nRet0C = s_nRet10 = s_nRet20 = 1;
    s_nRet3C = 0;
    s_fCurve = 0.0f;
}

/* ==========================================================================
 * 1. The three 0x1005F5A0 front ends
 * ========================================================================== */

#define TBL_N 4200
static BrScrRectEnt *s_aTbl;
static BrScrGlobals  s_G;

static void InitTable(void)
{
    int i;
    s_aTbl = (BrScrRectEnt *)calloc(TBL_N, sizeof *s_aTbl);
    if (s_aTbl == NULL) { printf("FAIL out of memory\n"); exit(1); }
    for (i = 0; i < TBL_N; ++i) {
        s_aTbl[i].f00 = i;
        s_aTbl[i].rc.f08 = i * 2;
        s_aTbl[i].rc.f0C = i * 3;
        s_aTbl[i].f14 = (i & 1);
    }
    memset(&s_G, 0, sizeof s_G);
    s_G.aAB568 = s_aTbl;
    s_G.w0AB3DC = 1;
}

static void TestDraw(void)
{
    BrUiObj *p = NewObj();

    BrScrStSlot(p, BR_SCR_SLOT_VTBL, (void *)&s_UiVtbl);

    /* --- 0x10047930: truncation toward zero, and which fields go where. */
    BrScrStF(p, BR_UI_OFF_F3C, 12.9f);
    BrScrStF(p, BR_UI_OFF_F40, -3.9f);
    BrScrSt16(p, BR_UI_OFF_W1E20C, 5u);
    memset(&s_Draw, 0, sizeof s_Draw);
    CHECK(BrUiDrawCode_10047930(&s_G, p) == 1);
    CHECK(s_Draw.nCalls == 1);
    CHECK(s_Draw.x == 12 && s_Draw.y == -3);
    CHECK(s_Draw.id == 5);
    CHECK(s_Draw.pRect == (const void *)&s_aTbl[5].rc);
    CHECK(s_Draw.f14 == s_aTbl[5].f14);

    /* A negative code is a no-op that still reports success. */
    BrScrSt16(p, BR_UI_OFF_W1E20C, 0xFFFFu);
    memset(&s_Draw, 0, sizeof s_Draw);
    CHECK(BrUiDrawCode_10047930(&s_G, p) == 1);
    CHECK(s_Draw.nCalls == 0);

    /* The documented EAX-high-half quirk: it first shows at code 0xAAB,
     * because 0xAAB * 0x18 == 0x10008 while 0xAAA * 0x18 == 0xFFF0. */
    BrScrSt16(p, BR_UI_OFF_W1E20C, 0x0AAAu);
    memset(&s_Draw, 0, sizeof s_Draw);
    (void)BrUiDrawCode_10047930(&s_G, p);
    CHECK(s_Draw.id == 0x0AAA);
    BrScrSt16(p, BR_UI_OFF_W1E20C, 0x0AABu);
    memset(&s_Draw, 0, sizeof s_Draw);
    (void)BrUiDrawCode_10047930(&s_G, p);
    CHECK(s_Draw.id == (int32_t)(0x00010000 | 0x0AAB));

    /* --- 0x10047980: the caller's rect wins, the table's +0x14 does not. */
    {
        int nDummy = 0;
        BrScrSt16(p, BR_UI_OFF_W1E20C, 7u);
        memset(&s_Draw, 0, sizeof s_Draw);
        CHECK(BrUiDrawCodeRect_10047980(&s_G, p, &nDummy) == 1);
        CHECK(s_Draw.pRect == (const void *)&nDummy);
        CHECK(s_Draw.f14 == s_aTbl[7].f14);
        CHECK(s_Draw.id == 7);
        CHECK(s_Draw.x == 12 && s_Draw.y == -3);
    }

    /* --- 0x100479D0: the id is the CALLER'S index, not the entry's f00. */
    s_aTbl[3].f00 = 99;
    memset(&s_Draw, 0, sizeof s_Draw);
    CHECK(BrUiDrawIndex_100479D0(&s_G, 3, 100, 200) == 1);
    CHECK(s_Draw.id == 3);
    CHECK(s_Draw.x == 100 && s_Draw.y == 200);
    CHECK(s_Draw.pRect == (const void *)&s_aTbl[3].rc);
    s_aTbl[3].f00 = 3;

    free(p);
}

/* ==========================================================================
 * 2. 0x10047A10
 * ========================================================================== */

static void TestStepCode(void)
{
    BrUiObj *p = NewObj();
    BrScrStSlot(p, BR_SCR_SLOT_VTBL, (void *)&s_UiVtbl);
    ResetUi();

    /* +0x296C clear -> vtable +0x1C only, code untouched. */
    BrScrSt16(p, BR_UI_OFF_W1E20C, 0x1234u);
    CHECK(BrUiStepCode_10047A10(p) == 1);
    CHECK(s_aUi[0x1C / 4] == 1 && s_aUi[0x18 / 4] == 0);
    CHECK(BrScrLd16u(p, BR_UI_OFF_W1E20C) == 0x1234u);

    /* +0x296C set -> the code for step i comes out of the +0x2A40 table. */
    BrScrSt32(p, BR_SCR_UI_F296C, 1u);
    BrScrSt16(p, BR_SCR_UI_W128, 2u);
    BrScrSt16(p, BR_UI_OFF_W2A40 + 4u, 0x0BEEu);   /* entry [2] */
    BrScrStSlot(p, BR_SCR_SLOT_P1E210, p);           /* any non-NULL base */
    CHECK(BrUiStepCode_10047A10(p) == 1);
    CHECK(s_aUi[0x18 / 4] == 1);
    CHECK(BrScrLd16u(p, BR_UI_OFF_W1E20C) == 0x0BEEu);

    free(p);
}

/* ==========================================================================
 * 3. The tween
 * ========================================================================== */

static void TestTween(void)
{
    BrUiObj *p = NewObj();
    BrScrStSlot(p, BR_SCR_SLOT_VTBL, (void *)&s_UiVtbl);

    /* --- 0x10047CB0: rate = (hi - lo) / n, and the SNAPSHOT direction. */
    BrScrStF(p, BR_SCR_UI_TWHI, 100.0f);
    BrScrStF(p, BR_SCR_UI_TWLO, 20.0f);
    BrScrStF(p, BR_UI_OFF_F3C, 1.5f);
    BrScrStF(p, BR_UI_OFF_F40, 2.5f);
    BrScrStF(p, BR_SCR_UI_F44, 3.5f);
    CHECK(BrUiTweenBegin_10047CB0(p, 8) == 1);
    CHECK(BrScrLdF(p, BR_SCR_UI_TWRATE) == 10.0f);
    CHECK(BrScrLdF(p, BR_SCR_UI_F30) == 1.5f);
    CHECK(BrScrLdF(p, BR_SCR_UI_F34) == 2.5f);
    CHECK(BrScrLdF(p, BR_SCR_UI_F38) == 3.5f);

    /* --- 0x10047D10 copies back the OTHER way and arms the tween. */
    BrScrStF(p, BR_UI_OFF_F3C, -9.0f);
    BrScrStF(p, BR_UI_OFF_F40, -9.0f);
    BrScrStF(p, BR_SCR_UI_F44, -9.0f);
    CHECK(BrUiTweenReset_10047D10(p) == 1);
    CHECK(BrScrLdF(p, BR_UI_OFF_F3C) == 1.5f);
    CHECK(BrScrLdF(p, BR_UI_OFF_F40) == 2.5f);
    CHECK(BrScrLdF(p, BR_SCR_UI_F44) == 3.5f);
    CHECK(BrScrLd32(p, BR_SCR_UI_TWACTIVE) == 1u);

    /* --- 0x10047CE0: quadratic in n, even in n, zero at n == 0. */
    BrScrStF(p, BR_SCR_UI_TWRATE, 2000.0f);
    CHECK(BrUiTweenCurve_10047CE0(p, 0) == 0.0f);
    CHECK(BrUiTweenCurve_10047CE0(p, 1) == 1.0f);
    CHECK(BrUiTweenCurve_10047CE0(p, 2) == 4.0f);
    CHECK(BrUiTweenCurve_10047CE0(p, -2) == BrUiTweenCurve_10047CE0(p, 2));

    free(p);
}

static void TestTweenStep(void)
{
    BrUiObj *p = NewObj();
    BrScrStSlot(p, BR_SCR_SLOT_VTBL, (void *)&s_UiVtbl);
    ResetUi();

    s_nTick = 1000; s_nTickStep = 0;

    /* Inactive: nothing happens, still returns 1. */
    CHECK(BrUiTweenStep_10047D30(p) == 1);
    CHECK(s_aUi[0x28 / 4] == 0);

    /* Both axes on, both moving up, limit 10, curve returns 4 then 40. */
    BrScrSt32(p, BR_SCR_UI_TWACTIVE, 1u);
    BrScrSt32(p, BR_SCR_UI_TWXON, 1u);
    BrScrSt32(p, BR_SCR_UI_TWYON, 1u);
    BrScrSt8(p, BR_SCR_UI_TWXDIR, 1u);
    BrScrSt8(p, BR_SCR_UI_TWYDIR, 1u);
    BrScrStF(p, BR_SCR_UI_F30, 0.0f);
    BrScrStF(p, BR_SCR_UI_F34, 0.0f);
    BrScrStF(p, BR_SCR_UI_TWXEND, 10.0f);
    BrScrStF(p, BR_SCR_UI_TWYEND, 10.0f);

    s_fCurve = 4.0f;
    s_nTick = 1000; s_nTickStep = 50;
    CHECK(BrUiTweenStep_10047D30(p) == 1);
    CHECK(BrScrLdF(p, BR_UI_OFF_F3C) == 4.0f);
    CHECK(BrScrLdF(p, BR_UI_OFF_F40) == 4.0f);
    CHECK(BrScrLd32(p, BR_SCR_UI_TWACTIVE) == 1u);   /* not finished */
    /* First call seeds the tick, so no time has elapsed yet. */
    CHECK(BrScrLd32(p, BR_SCR_UI_TWMS) == 0u);

    CHECK(BrUiTweenStep_10047D30(p) == 1);
    CHECK(BrScrLd32(p, BR_SCR_UI_TWMS) == 50u);      /* one 50 ms tick */

    /* Overshoot: both axes clamp and the tween disarms and resets its clock. */
    s_fCurve = 40.0f;
    CHECK(BrUiTweenStep_10047D30(p) == 1);
    CHECK(BrScrLdF(p, BR_UI_OFF_F3C) == 10.0f);
    CHECK(BrScrLdF(p, BR_UI_OFF_F40) == 10.0f);
    CHECK(BrScrLd32(p, BR_SCR_UI_TWACTIVE) == 0u);
    CHECK(BrScrLd32(p, BR_SCR_UI_TWMS) == 0u);

    /* An axis with a direction byte outside {0,1,0xFF} NEVER finishes, so
     * the tween stays armed however far the other axis overshoots. */
    BrScrSt32(p, BR_SCR_UI_TWACTIVE, 1u);
    BrScrSt8(p, BR_SCR_UI_TWYDIR, 7u);
    s_fCurve = 999.0f;
    CHECK(BrUiTweenStep_10047D30(p) == 1);
    CHECK(BrScrLd32(p, BR_SCR_UI_TWACTIVE) == 1u);

    /* An axis switched off counts as finished immediately. */
    BrScrSt32(p, BR_SCR_UI_TWYON, 0u);
    CHECK(BrUiTweenStep_10047D30(p) == 1);
    CHECK(BrScrLd32(p, BR_SCR_UI_TWACTIVE) == 0u);

    /* The 0xFF arm walks DOWN from the origin and clamps on <=. */
    BrScrSt32(p, BR_SCR_UI_TWACTIVE, 1u);
    BrScrSt32(p, BR_SCR_UI_TWYON, 1u);
    BrScrSt8(p, BR_SCR_UI_TWXDIR, 0xFFu);
    BrScrSt8(p, BR_SCR_UI_TWYDIR, 0xFFu);
    BrScrStF(p, BR_SCR_UI_F30, 100.0f);
    BrScrStF(p, BR_SCR_UI_F34, 100.0f);
    BrScrStF(p, BR_SCR_UI_TWXEND, 50.0f);
    BrScrStF(p, BR_SCR_UI_TWYEND, 50.0f);
    s_fCurve = 10.0f;
    CHECK(BrUiTweenStep_10047D30(p) == 1);
    CHECK(BrScrLdF(p, BR_UI_OFF_F3C) == 90.0f);
    CHECK(BrScrLd32(p, BR_SCR_UI_TWACTIVE) == 1u);
    s_fCurve = 60.0f;
    CHECK(BrUiTweenStep_10047D30(p) == 1);
    CHECK(BrScrLdF(p, BR_UI_OFF_F3C) == 50.0f);
    CHECK(BrScrLd32(p, BR_SCR_UI_TWACTIVE) == 0u);

    s_nTickStep = 0;
    free(p);
}

/* ==========================================================================
 * 4. 0x10047EB0 / 0x10047FB0
 * ========================================================================== */

static void TestItemInit(void)
{
    BrUiObj *p = NewObj();
    int32_t aSrc[3];

    aSrc[0] = 111; aSrc[1] = 222; aSrc[2] = 333;
    BrScrStSlot(p, BR_SCR_SLOT_ITEMVTBL, (void *)&s_ItemVtbl);
    BrScrStF(p, BR_UI_OFF_F3C, 5.0f);
    BrScrStF(p, BR_UI_OFF_F40, 7.75f);
    BrScrSt32(p, BR_SCR_UI_ITEMFLAGS, 0x80u);
    BrScrSt16(p, BR_SCR_UI_ITEMW40A, 0x1111u);
    BrScrSt16(p, BR_SCR_UI_ITEMW40C, 0x2222u);
    s_nItem04 = s_nItem08 = s_nItem28 = 0;

    BrUiItemInit_10047EB0(p, "hello", 0x02u, 1u, aSrc);

    CHECK(strcmp((const char *)p + BR_SCR_UI_ITEMTEXT, "hello") == 0);
    CHECK(BrScrLd32(p, BR_SCR_UI_ITEMFLAGS) == 0x82u);   /* OR, not store */
    CHECK(BrScrLd8(p, BR_SCR_UI_ITEMKIND) == 1u);
    CHECK(s_nItem04 == 1 && s_nItem08 == 0);             /* kind != 3 */
    CHECK(s_nItem28 == 0);                               /* flags bit 0 clear */
    /* The two words are zeroed before the dispatch, so they are 0 after it. */
    CHECK(BrScrLd16u(p, BR_SCR_UI_ITEMW40A) == 0u);
    CHECK(BrScrLd16u(p, BR_SCR_UI_ITEMW40C) == 0u);
    CHECK(BrScrLd32(p, BR_SCR_UI_ITEMF424) == 111u);
    CHECK(BrScrLd32(p, BR_SCR_UI_ITEMF42C) == 333u);     /* pSrc[2], not [1] */
    CHECK(BrScrLdF(p, BR_SCR_UI_ITEMF410) == 5.0f);
    CHECK(BrScrLdF(p, BR_SCR_UI_ITEMF414) == 7.75f);
    CHECK(BrScrLd32(p, BR_SCR_UI_F54) == 7u);            /* ftol(7.75) */
    CHECK(BrScrLd32(p, BR_SCR_UI_F5C) == 7u);            /* + w40C, now 0 */
    CHECK(BrScrLd32(p, BR_SCR_UI_F50) == 111u);
    CHECK(BrScrLd32(p, BR_SCR_UI_F58) == 333u);
    CHECK(BrScrLd16u(p, BR_SCR_UI_W48) == 0u);
    CHECK(BrScrLd16u(p, BR_SCR_UI_W4A) == 0u);

    /* kind == 3 picks the OTHER hook; flags bit 0 fires +0x28 for effect. */
    s_nItem04 = s_nItem08 = s_nItem28 = 0;
    BrUiItemInit_10047EB0(p, "x", 0x01u, 3u, aSrc);
    CHECK(s_nItem08 == 1 && s_nItem04 == 0);
    CHECK(s_nItem28 == 1);
    CHECK(strcmp((const char *)p + BR_SCR_UI_ITEMTEXT, "x") == 0);

    free(p);
}

static void TestInit(void)
{
    BrUiObj *p = NewObj();
    BrPhaseFull ph;

    memset(&ph, 0, sizeof ph);
    BrScrSt32(p, BR_UI_OFF_FLAGS, 0x1u);
    BrScrSt32(p, BR_SCR_UI_FLAGS24, 0x2u);
    BrScrSt32(p, BR_SCR_UI_FLAGS28, 0x4u);

    BrUiInit_10047FB0(p, &ph, 1.25f, -2.5f, 0x10u, 0x20u, 0x40u, 7u, 0x0ABCu);

    CHECK(BrScrLdSlot(p, BR_SCR_SLOT_PHASE) == (void *)&ph);
    CHECK(BrScrLd32(p, BR_UI_OFF_FLAGS)   == 0x11u);   /* OR */
    CHECK(BrScrLd32(p, BR_SCR_UI_FLAGS24) == 0x22u);
    CHECK(BrScrLd32(p, BR_SCR_UI_FLAGS28) == 0x44u);
    CHECK(BrScrLd32(p, BR_SCR_UI_F2968)   == 7u);      /* stored, not OR-ed */
    CHECK(BrScrLdF(p, BR_UI_OFF_F3C) == 1.25f);
    CHECK(BrScrLdF(p, BR_UI_OFF_F40) == -2.5f);
    /* one word, two destinations */
    CHECK(BrScrLd16u(p, BR_UI_OFF_W2A40)  == 0x0ABCu);
    CHECK(BrScrLd16u(p, BR_UI_OFF_W1E20C) == 0x0ABCu);

    free(p);
}

/* ==========================================================================
 * 5. 0x10048010 / 0x10048060 / 0x100480A0
 * ========================================================================== */

static void TestEnter(void)
{
    BrUiObj *p = NewObj();

    BrScrStSlot(p, BR_SCR_SLOT_VTBL, (void *)&s_UiVtbl);
    BrScrStSlot(p, BR_SCR_SLOT_ITEMVTBL, (void *)&s_ItemVtbl);
    ResetUi();
    s_nItem10 = 0;

    /* +0x28 bit 0 clear: nothing runs at all. */
    CHECK(BrUiEnter_10048010(p) == 1);
    CHECK(s_nItem10 == 0 && s_aUi[0x10 / 4] == 0);

    /* 0x100000 -> the ITEM's vtable +0x10. */
    BrScrSt32(p, BR_SCR_UI_FLAGS28, 1u);
    BrScrSt32(p, BR_UI_OFF_FLAGS, BR_SCR_F1C_100000);
    CHECK(BrUiEnter_10048010(p) == 1);
    CHECK(s_nItem10 == 1 && s_aUi[0x10 / 4] == 0);

    /* 0x200000 (and not 0x100000) -> nothing, still 1. */
    BrScrSt32(p, BR_UI_OFF_FLAGS, BR_SCR_F1C_200000);
    CHECK(BrUiEnter_10048010(p) == 1);
    CHECK(s_nItem10 == 1 && s_aUi[0x10 / 4] == 0);

    /* Neither bit -> the OBJECT's vtable +0x10 decides the return value. */
    BrScrSt32(p, BR_UI_OFF_FLAGS, 0u);
    s_nRet10 = 1;
    CHECK(BrUiEnter_10048010(p) == 1);
    CHECK(s_aUi[0x10 / 4] == 1);
    s_nRet10 = 0;
    CHECK(BrUiEnter_10048010(p) == 0);   /* the ONLY zero return */
    CHECK(s_aUi[0x10 / 4] == 2);

    free(p);
}

static void TestCheckOther(void)
{
    BrUiObj *pA = NewObj();
    BrUiObj *pB = NewObj();
    BrPhaseFull ph;

    memset(&ph, 0, sizeof ph);
    BrScrStSlot(pA, BR_SCR_SLOT_PHASE, &ph);

    s_G.pAA29C0 = NULL;
    s_G.nAA2858 = 9;
    CHECK(BrUiCheckOther_10048060(&s_G, pB) == 0);
    CHECK(s_G.nAA2858 == 0);

    s_G.pAA29C0 = pA;
    ph.aFlags[1] = 0;
    s_G.nAA2858 = 9;
    CHECK(BrUiCheckOther_10048060(&s_G, pB) == 0);
    CHECK(s_G.nAA2858 == 0);

    /* Same object: 0, and nAA2858 is deliberately LEFT ALONE. */
    ph.aFlags[1] = 1;
    s_G.nAA2858 = 9;
    CHECK(BrUiCheckOther_10048060(&s_G, pA) == 0);
    CHECK(s_G.nAA2858 == 9);

    s_G.nAA2858 = 9;
    CHECK(BrUiCheckOther_10048060(&s_G, pB) == 1);
    CHECK(s_G.nAA2858 == 1);

    s_G.pAA29C0 = NULL;
    free(pA);
    free(pB);
}

static void TestTickSteps(void)
{
    BrUiObj *p = NewObj();

    s_nTick = 0; s_nTickStep = 0;

    /* +0x2968 clear: nothing at all. */
    CHECK(BrUiTickSteps_100480A0(p) == 1);
    CHECK(BrScrLd32(p, BR_SCR_UI_F2974) == 0u);

    /* +0x296C clear arm: needs STRICTLY more than 0x3C ms. */
    BrScrSt32(p, BR_SCR_UI_F2968, 1u);
    s_nTick = 0;
    CHECK(BrUiTickSteps_100480A0(p) == 1);       /* seeds nothing: dt = 0 */
    s_nTick = 60;                                /* exactly 0x3C */
    CHECK(BrUiTickSteps_100480A0(p) == 1);
    CHECK(BrScrLd32(p, BR_SCR_UI_F2974) == 60u);
    CHECK((BrScrLd32(p, BR_UI_OFF_FLAGS) & BR_SCR_BIT100) == 0u);
    s_nTick = 61;
    CHECK(BrUiTickSteps_100480A0(p) == 1);
    CHECK(BrScrLd32(p, BR_SCR_UI_F2974) == 0u);
    CHECK((BrScrLd32(p, BR_UI_OFF_FLAGS) & BR_SCR_BIT100) != 0u);
    CHECK((BrScrLd32(p, BR_SCR_UI_F3850) & BR_SCR_BIT100) != 0u);

    /* +0x296C set arm: walks the duration table and wraps to 0 at the end. */
    BrScrSt32(p, BR_SCR_UI_F296C, 1u);
    BrScrSt32(p, BR_UI_OFF_FLAGS, 0u);
    BrScrSt32(p, BR_SCR_UI_F2974, 0u);
    BrScrSt16(p, BR_SCR_UI_W128, 0u);
    BrScrSt32(p, BR_SCR_UI_A2978 + 0u, 100u);
    BrScrSt32(p, BR_SCR_UI_A2978 + 4u, 100u);
    BrScrSt32(p, BR_SCR_UI_A2978 + 8u, 0u);      /* terminator */
    s_nTick = 0;
    BrScrSt32(p, BR_SCR_UI_F2970, 0u);
    s_nTick = 100;
    CHECK(BrUiTickSteps_100480A0(p) == 1);
    CHECK(BrScrLd16u(p, BR_SCR_UI_W128) == 0u);  /* jle: 100 <= 100 */
    s_nTick = 201;
    CHECK(BrUiTickSteps_100480A0(p) == 1);
    CHECK(BrScrLd16u(p, BR_SCR_UI_W128) == 1u);
    BrScrSt32(p, BR_SCR_UI_F2974, 0u);
    s_nTick = 400;
    CHECK(BrUiTickSteps_100480A0(p) == 1);
    /* Step 2 has length 0, so the index wraps back to 0 instead of stopping. */
    CHECK(BrScrLd16u(p, BR_SCR_UI_W128) == 0u);

    free(p);
}

/* ==========================================================================
 * 6. 0x10048180 -- the sentinel returns and which tail runs
 * ========================================================================== */

static int32_t Hook_ret(BrUiObj *p);
static int32_t s_nHookRet = 0;
static int     s_nHookCalls = 0;
static int32_t Hook_ret(BrUiObj *p) { (void)p; ++s_nHookCalls; return s_nHookRet; }

static void TestFrame(void)
{
    BrUiObj *p = NewObj();

    BrScrStSlot(p, BR_SCR_SLOT_VTBL, (void *)&s_UiVtbl);
    BrScrStSlot(p, BR_SCR_SLOT_ITEMVTBL, (void *)&s_ItemVtbl);

    /* Flag 0x10 short-circuits to vtable +0x08 alone. */
    ResetUi();
    BrScrSt32(p, BR_UI_OFF_FLAGS, BR_SCR_F1C_0010);
    CHECK(BrUiFrame_10048180(&s_G, p) == 1);
    CHECK(s_aUi[0x08 / 4] == 1 && s_aUi[0x3C / 4] == 0);

    /* vtable +0x3C saying "yes" also short-circuits, but only after it ran. */
    ResetUi();
    BrScrSt32(p, BR_UI_OFF_FLAGS, 0u);
    s_nRet3C = 1;
    CHECK(BrUiFrame_10048180(&s_G, p) == 1);
    CHECK(s_aUi[0x3C / 4] == 1 && s_aUi[0x08 / 4] == 1 && s_aUi[0x04 / 4] == 0);

    /* The +0x04 field hook's sentinels. */
    ResetUi();
    BrScrStSlot(p, BR_SCR_SLOT_PFN04, (void *)Hook_ret);
    s_nHookRet = -1; s_nHookCalls = 0;
    CHECK(BrUiFrame_10048180(&s_G, p) == 0);
    CHECK(s_nHookCalls == 1);
    CHECK(s_aUi[0x08 / 4] == 0);           /* no tail on the -1 path */

    ResetUi();
    s_nHookRet = -2; s_nHookCalls = 0;
    CHECK(BrUiFrame_10048180(&s_G, p) == 1);
    CHECK(s_aUi[0x08 / 4] == 0);           /* nor on the -2 path */

    /* vtable +0x20 refusing sends it down the 0x100483D3 tail, which clears
     * the step index and always ends in vtable +0x08. */
    ResetUi();
    s_nHookRet = 0; s_nHookCalls = 0;
    s_nRet20 = 0;
    BrScrSt16(p, BR_SCR_UI_W128, 5u);
    BrScrSt32(p, BR_UI_OFF_FLAGS, BR_SCR_F1C_400000);
    BrScrSt16(p, BR_UI_OFF_W2A40, 0x0777u);
    BrScrSt16(p, BR_UI_OFF_W1E20C, 0u);
    CHECK(BrUiFrame_10048180(&s_G, p) == 1);
    CHECK(BrScrLd16u(p, BR_UI_OFF_W1E20C) == 0x0777u);
    CHECK(BrScrLd16u(p, BR_SCR_UI_W128) == 0u);
    CHECK(s_aUi[0x08 / 4] == 1);

    /* Bit 4 present -> the 0x1004842F arm runs the +0x0C hook instead and
     * leaves the step index alone. */
    ResetUi();
    BrScrSt16(p, BR_SCR_UI_W128, 5u);
    BrScrSt32(p, BR_UI_OFF_FLAGS, BR_SCR_F1C_0004);
    BrScrStSlot(p, BR_SCR_SLOT_PFN0C, (void *)Hook_ret);
    s_nHookCalls = 0;
    CHECK(BrUiFrame_10048180(&s_G, p) == 1);
    CHECK(s_nHookCalls == 2);              /* +0x04 and +0x0C */
    CHECK(BrScrLd16u(p, BR_SCR_UI_W128) == 5u);

    BrScrStSlot(p, BR_SCR_SLOT_PFN04, NULL);
    BrScrStSlot(p, BR_SCR_SLOT_PFN0C, NULL);
    free(p);
}

/* ==========================================================================
 * 7. BrUiPage
 * ========================================================================== */

static void TestPage(void)
{
    BrUiPage *pg = (BrUiPage *)malloc(sizeof *pg);
    int i, fAllNull = 1;

    if (pg == NULL) { printf("FAIL out of memory\n"); exit(1); }
    memset(pg, 0xAB, sizeof *pg);          /* operator new does NOT zero */

    CHECK(BrUiPageCtor_10048470(pg) == pg);
    CHECK(pg->pVtbl == &BrUiPageVtbl_1008F6F8);
    CHECK(pg->f10 == 0 && pg->nItems == 0);
    CHECK(pg->pfn04 == NULL && pg->pfn08 == NULL && pg->pfn0C == NULL);
    for (i = 0; i < BR_UI_PAGE_ITEMS; ++i)
        if (pg->aItems[i] != NULL) fAllNull = 0;
    CHECK(fAllNull);
    CHECK(pg->f338 == 0.0f && pg->f33C == 0.0f);
    CHECK(pg->pOwner == NULL && pg->f344 == 0 && pg->f346 == 0);

    /* 0x100484C0: the body always runs; the free only with bit 0. */
    s_n484E0 = 0; s_nDeleted = 0;
    CHECK(BrUiPageDelete_100484C0(pg, 0) == (void *)pg);
    CHECK(s_n484E0 == 1 && s_nDeleted == 0);
    CHECK(BrUiPageDelete_100484C0(pg, 2) == (void *)pg);
    CHECK(s_n484E0 == 2 && s_nDeleted == 0);   /* bit 1 is not bit 0 */
    CHECK(BrUiPageDelete_100484C0(pg, 1) == (void *)pg);
    CHECK(s_n484E0 == 3 && s_nDeleted == 1 && s_apDeleted[0] == (void *)pg);

    free(pg);
}

static void TestPageSelect(void)
{
    BrUiPage pg;

    memset(&pg, 0, sizeof pg);
    pg.f344 = 6;

    /* cursor >= modulus -> wrap to 0, and the global IS written. */
    s_G.wAA286C = 6;
    CHECK(BrUiPageSelect_100484F0(&s_G, &pg) == 1);
    CHECK(pg.f346 == 0 && s_G.wAA286C == 0);

    /* in range -> copied to +0x346, global untouched. */
    s_G.wAA286C = 3;
    CHECK(BrUiPageSelect_100484F0(&s_G, &pg) == 1);
    CHECK(pg.f346 == 3 && s_G.wAA286C == 3);

    /* negative -> wraps to modulus-1, and the global IS written. */
    s_G.wAA286C = 0xFFFFu;                       /* -1 */
    CHECK(BrUiPageSelect_100484F0(&s_G, &pg) == 1);
    CHECK(pg.f346 == 5 && s_G.wAA286C == 5);

    /* A zero modulus makes every non-negative cursor wrap to 0. */
    pg.f344 = 0;
    s_G.wAA286C = 0;
    CHECK(BrUiPageSelect_100484F0(&s_G, &pg) == 1);
    CHECK(pg.f346 == 0 && s_G.wAA286C == 0);
}

static void TestPageFrame(void)
{
    BrUiPage pg;
    BrPhaseFull ph;
    BrUiObj *pA = NewObj();
    BrUiObj *pB = NewObj();
    static int s_nHook04 = 0, s_nHook08 = 0;

    memset(&pg, 0, sizeof pg);
    memset(&ph, 0, sizeof ph);
    pg.pOwner = &ph;
    pg.pVtbl  = &BrUiPageVtbl_1008F6F8;
    pg.nItems = 2;
    pg.aItems[0] = pA;
    pg.aItems[1] = pB;
    pg.f344 = 2;
    BrScrStSlot(pA, BR_SCR_SLOT_VTBL, (void *)&s_UiVtbl);
    BrScrStSlot(pB, BR_SCR_SLOT_VTBL, (void *)&s_UiVtbl);

    ResetUi();
    s_G.wAA286C = 0;
    s_G.wAA2870 = 77;
    CHECK(BrUiPageFrame_10048530(&s_G, &pg) == 1);
    CHECK(s_G.wAA2870 == 0);                   /* zeroed up front */
    CHECK(pg.f346 == 0);                       /* the select ran */
    CHECK(s_aUi[0x0C / 4] == 2);               /* both items drawn */

    /* A NULL slot stops the page dead and reports failure, and the tail
     * hook does NOT run. */
    pg.aItems[1] = NULL;
    ResetUi();
    (void)s_nHook04; (void)s_nHook08;
    CHECK(BrUiPageFrame_10048530(&s_G, &pg) == 0);
    CHECK(s_aUi[0x0C / 4] == 1);
    pg.aItems[1] = pB;

    /* vtable +0x0C refusing also fails, and clears bAA28A8. */
    ResetUi();
    s_nRet0C = 0;
    s_G.bAA28A8 = 5;
    CHECK(BrUiPageFrame_10048530(&s_G, &pg) == 0);
    CHECK(s_G.bAA28A8 == 0);

    /* Flag 0x800 skips an item's +0x0C entirely without failing. */
    ResetUi();
    BrScrSt32(pA, BR_UI_OFF_FLAGS, BR_SCR_F1C_0800);
    BrScrSt32(pB, BR_UI_OFF_FLAGS, BR_SCR_F1C_0800);
    CHECK(BrUiPageFrame_10048530(&s_G, &pg) == 1);
    CHECK(s_aUi[0x0C / 4] == 0);
    BrScrSt32(pA, BR_UI_OFF_FLAGS, 0u);
    BrScrSt32(pB, BR_UI_OFF_FLAGS, 0u);

    free(pA);
    free(pB);
}

/* ==========================================================================
 * 8. BrPhaseFull
 * ========================================================================== */

/* A recording phase vtable, distinct from the 0x1008F700 stand-in so the
 * shutdown test can watch the order of the two calls per slot. */
static int   s_aSeq[128];
static int   s_nSeq = 0;
static void  Seq(int v) { if (s_nSeq < 128) s_aSeq[s_nSeq++] = v; }

static void  RecF1C(BrPhaseFull *p) { Seq(0x1C0000 | p->f0C); }
static void *RecF00(BrPhaseFull *p, int32_t n)
{
    (void)n;
    Seq(0x000000 | p->f0C);
    return p;
}
static const BrPhaseFullVtbl s_RecVtbl = {
    RecF00, PhaseF04, PhaseF08, PhaseF0C, NULL,
    PhaseF14, PhaseF18, RecF1C, PhaseF20
};

/* A +0x1C that clears its own global, exercising the documented re-read. */
static BrPhaseFull **s_ppSelfClear = NULL;
static void SelfClearF1C(BrPhaseFull *p) { Seq(0x1C0000 | p->f0C); *s_ppSelfClear = NULL; }
static const BrPhaseFullVtbl s_SelfClearVtbl = {
    RecF00, PhaseF04, PhaseF08, PhaseF0C, NULL,
    PhaseF14, PhaseF18, SelfClearF1C, PhaseF20
};

/* A refcounted stand-in: slot 0 is the scalar deleting destructor. */
static int    s_nRel = 0;
static void  *s_apRel[8];
static int32_t s_aRelFlag[8];
static void *RelF00(BrScrRef *p, int32_t n)
{
    if (s_nRel < 8) { s_apRel[s_nRel] = p; s_aRelFlag[s_nRel] = n; }
    ++s_nRel;
    return p;
}
static const BrScrRefVtbl s_RelVtbl = { RelF00 };

static void TestPhaseDtor(void)
{
    BrPhaseFull ph;
    BrScrRef refA, refB;

    refA.pVtbl = &s_RelVtbl;
    refB.pVtbl = &s_RelVtbl;

    memset(&ph, 0, sizeof ph);
    ph.fC0 = &refA;
    ph.fC4 = &refB;
    s_nRel = 0;
    BrPhaseDtor_10048870(&ph);
    CHECK(ph.pVtbl == &BrPhaseVtbl_1008F700);
    CHECK(s_nRel == 2);
    CHECK(s_apRel[0] == (void *)&refA && s_apRel[1] == (void *)&refB);
    CHECK(s_aRelFlag[0] == 1 && s_aRelFlag[1] == 1);
    CHECK(ph.fC0 == NULL && ph.fC4 == NULL);

    /* Both NULL: nothing released, vtable still re-seated. */
    s_nRel = 0;
    BrPhaseDtor_10048870(&ph);
    CHECK(s_nRel == 0);

    /* 0x10048850 delegates and then frees only with bit 0. */
    s_nDeleted = 0;
    CHECK(BrPhaseDelete_10048850(&ph, 0) == (void *)&ph);
    CHECK(s_nDeleted == 0);
    CHECK(BrPhaseDelete_10048850(&ph, 1) == (void *)&ph);
    CHECK(s_nDeleted == 1);
}

static void TestPhaseTick(void)
{
    BrPhaseFull ph, phCur, phOther;
    BrUiPage pg;
    BrUiObj *pItem = NewObj();
    BrObjAA2E80 obj;

    memset(&ph, 0, sizeof ph);
    memset(&phCur, 0, sizeof phCur);
    memset(&phOther, 0, sizeof phOther);
    memset(&pg, 0, sizeof pg);
    memset(&obj, 0, sizeof obj);

    ph.pVtbl = &BrPhaseVtbl_1008F700;
    phCur.aPages[0] = &pg;
    pg.aItems[BR_UI_PAGE_ITEM334] = pItem;
    BrScrStSlot(pItem, BR_SCR_SLOT_VTBL, (void *)&s_UiVtbl);

    obj.f00 = -12;
    obj.f04 = 34;
    s_G.pAA2E80 = &obj;
    s_G.pAA2908 = &phCur;
    s_G.pAA2904 = &phOther;
    s_G.n0940A4 = 0;
    s_G.nAA2A4C = 0;
    s_G.nAA2874 = 1;
    s_nCdTrack = 40;
    ResetUi();

    /* +0x08 bit 4 set -> immediate 0, nothing runs. */
    ph.pfn08 = (void *)(uintptr_t)0x10u;
    CHECK(BrPhaseTick_100488C0(&s_G, &ph) == 0);
    CHECK(s_aUi[0x0C / 4] == 0);
    CHECK(s_G.nAA2A4C == 0);

    /* Normal: the counter advances and the CD track is re-read on the 0th. */
    ph.pfn08 = NULL;
    CHECK(BrPhaseTick_100488C0(&s_G, &ph) == 1);
    CHECK(s_G.nAA2A4C == 1);
    CHECK(s_G.nAA2A34 == 38);                        /* track - 2 */
    CHECK(s_aUi[0x0C / 4] == 1);
    CHECK(BrScrLdF(pItem, BR_UI_OFF_F3C) == -12.0f); /* fild, signed */
    CHECK(BrScrLdF(pItem, BR_UI_OFF_F40) == 34.0f);
    CHECK(s_G.pAA2904 == &phOther);                  /* restored */

    /* Not on a multiple of 0x78 -> no re-read, counter still advances. */
    s_G.nAA2A34 = 0;
    CHECK(BrPhaseTick_100488C0(&s_G, &ph) == 1);
    CHECK(s_G.nAA2A34 == 0);
    CHECK(s_G.nAA2A4C == 2);

    /* n0940A4 == 2 re-reads EVERY time and does NOT advance the counter. */
    s_G.n0940A4 = 2;
    CHECK(BrPhaseTick_100488C0(&s_G, &ph) == 1);
    CHECK(s_G.nAA2A34 == 38);
    CHECK(s_G.nAA2A4C == 2);
    s_G.n0940A4 = 0;

    free(pItem);
}

static void TestPhaseReleasePages(void)
{
    BrPhaseFull ph;
    BrUiPage pg0;
    BrScrRef item0, item1;

    memset(&ph, 0, sizeof ph);
    memset(&pg0, 0, sizeof pg0);
    item0.pVtbl = &s_RelVtbl;
    item1.pVtbl = &s_RelVtbl;

    pg0.pVtbl = &BrUiPageVtbl_1008F6F8;
    pg0.aItems[0]   = (BrUiObj *)&item0;
    pg0.aItems[199] = (BrUiObj *)&item1;

    ph.nPages = 2;
    ph.aPages[0] = &pg0;
    ph.aPages[1] = NULL;     /* the DEVIATION path: skipped, not crashed */

    s_nPageDtor = 0;
    s_nRel = 0;
    s_G.wAA286C = 9;
    BrPhaseReleasePages_10048AA0(&s_G, &ph);
    CHECK(s_nRel == 2);               /* only the two non-NULL slots */
    CHECK(s_aRelFlag[0] == 1 && s_aRelFlag[1] == 1);
    CHECK(s_nPageDtor == 1);          /* only the non-NULL page */
    CHECK(s_G.wAA286C == 0);
    CHECK(pg0.aItems[0] == NULL && pg0.aItems[BR_UI_PAGE_ITEMS - 1] == NULL);
}

static void TestShutdown(void)
{
    BrPhaseFull a, b, c, d;
    void *ap[BR_SCR_A9E3D0_COUNT];
    int i;

    memset(&a, 0, sizeof a); a.pVtbl = &s_RecVtbl; a.f0C = 1;
    memset(&b, 0, sizeof b); b.pVtbl = &s_RecVtbl; b.f0C = 2;
    memset(&c, 0, sizeof c); c.pVtbl = &s_RecVtbl; c.f0C = 3;
    memset(&d, 0, sizeof d); d.pVtbl = &s_RecVtbl; d.f0C = 4;

    memset(&s_G, 0, sizeof s_G);
    s_G.aAB568 = s_aTbl;
    s_G.w0AB3DC = 1;
    for (i = 0; i < (int)BR_SCR_A9E3D0_COUNT; ++i) ap[i] = NULL;
    ap[0] = &a;                                /* just a non-NULL sentinel */
    s_G.apA9E3D0 = ap;
    s_G.nA9E3D0  = (int32_t)BR_SCR_A9E3D0_COUNT;

    s_G.pAA2940 = &a;
    s_G.pAA290C = &b;
    s_G.pAA2914 = &c;
    s_G.nA9CFFC = 7; s_G.nAA29AC = 7;
    s_G.pAA29B4 = &d;
    s_G.pAA2908 = &d;
    s_G.nAA2854 = 0;

    s_nTick = 0; s_nTickStep = 1;
    s_nSeq = 0; s_nSleep = 0; s_nDeleted = 0; s_n5F530 = 0; s_n5FCF0 = 0;

    /* pArg NOT NULL: no A9E3D0 walk, 0x10AA2908 kept. */
    BrPhaseShutdown_10048B20(&s_G, (void *)&i);
    CHECK(s_n5F530 == 0);
    CHECK(ap[0] == &a);
    CHECK(s_G.pAA2908 == &d);
    CHECK(s_G.pAA2940 == NULL && s_G.pAA290C == NULL && s_G.pAA2914 == NULL);
    CHECK(s_G.nA9CFFC == 0 && s_G.nAA29AC == 0);
    CHECK(s_G.pAA29B4 == NULL);
    /* +0x1C first, then +0x00, per slot, in the original's slot order. */
    CHECK(s_nSeq == 6);
    CHECK(s_aSeq[0] == (0x1C0000 | 1) && s_aSeq[1] == 1);
    CHECK(s_aSeq[2] == (0x1C0000 | 2) && s_aSeq[3] == 2);
    CHECK(s_aSeq[4] == (0x1C0000 | 3) && s_aSeq[5] == 3);

    /* 0x10AA2940 appears twice in the chain, but the second visit finds a
     * cleared slot, so it fires exactly once. */
    {
        int nA = 0;
        for (i = 0; i < s_nSeq; ++i) if (s_aSeq[i] == (0x1C0000 | 1)) ++nA;
        CHECK(nA == 1);
    }

    /* pArg NULL: the table walk, 0x10AA2908 and 0x10AA2900 all run. */
    s_G.pAA2900 = &a;
    s_nSeq = 0; s_nDeleted = 0; s_n8B80 = 0;
    BrPhaseShutdown_10048B20(&s_G, NULL);
    CHECK(s_n5F530 == 1 && s_n5FCF0 == 1);
    CHECK(ap[0] == NULL);
    CHECK(s_nDeleted == 2);                    /* ap[0] and pAA2900 */
    CHECK(s_n8B80 == 1);
    CHECK(s_G.pAA2900 == NULL);
    CHECK(s_G.pAA2908 == NULL);
    CHECK(s_G.pAA2904 == NULL);

    /* A +0x1C that clears its own slot skips the release -- the re-read. */
    memset(&s_G, 0, sizeof s_G);
    s_G.apA9E3D0 = ap; s_G.nA9E3D0 = (int32_t)BR_SCR_A9E3D0_COUNT;
    a.pVtbl = &s_SelfClearVtbl;
    s_G.pAA2940 = &a;
    s_ppSelfClear = &s_G.pAA2940;
    s_nSeq = 0;
    BrPhaseShutdown_10048B20(&s_G, (void *)&i);
    CHECK(s_nSeq == 1);                        /* +0x1C ran, +0x00 did not */
    CHECK(s_aSeq[0] == (0x1C0000 | 1));
    CHECK(s_G.pAA2940 == NULL);
    CHECK(s_G.nA9CFFC == 0);                   /* the extra clear still ran */

    a.pVtbl = &s_RecVtbl;
    s_nTickStep = 0;
}

static void TestPhaseRun(void)
{
    BrPhaseFull ph, phOther;
    BrUiPage pg0, pg1;

    memset(&ph, 0, sizeof ph);
    memset(&phOther, 0, sizeof phOther);
    memset(&pg0, 0, sizeof pg0);
    memset(&pg1, 0, sizeof pg1);
    ph.pVtbl = &BrPhaseVtbl_1008F700;
    pg0.pVtbl = &BrUiPageVtbl_1008F6F8;
    pg1.pVtbl = &BrUiPageVtbl_1008F6F8;

    memset(&s_G, 0, sizeof s_G);
    s_G.aAB568 = s_aTbl;
    s_G.pAA2908 = &phOther;
    s_G.pAA2904 = &ph;

    /* +0x68 clear: the bail-out path, return 0. */
    s_n3E310 = s_n6A4A0 = 0;
    ph.f68 = 0;
    ph.iPage = 4;
    CHECK(BrPhaseRun_100489A0(&s_G, &ph) == 0);
    CHECK(s_n3E310 == 1 && s_n6A4A0 == 1);
    CHECK(ph.iPage == 0);

    /* Normal run: pages with a set flag get their +0x04, the current phase
     * is borrowed and restored, and nAA2868 records the comparison. */
    ph.f68 = 1;
    ph.nPages = 2;
    ph.aPages[0] = &pg0;
    ph.aPages[1] = &pg1;
    ph.aFlags[0] = 1;
    ph.aFlags[1] = 0;
    s_n60260 = s_nDik = 0;
    CHECK(BrPhaseRun_100489A0(&s_G, &ph) == 1);
    CHECK(s_n60260 == 1 && s_nDik == 1);
    CHECK(s_G.pAA2904 == &ph);                 /* restored */
    CHECK(s_G.nAA2868 == 0);                   /* pAA2904 != pAA2908 */
    CHECK(ph.pCur == &pg1);
    CHECK(ph.iPage == 1);

    /* A NULL page stops the run AND leaves pCur NULL -- the write happens
     * before the test. */
    ph.aPages[1] = NULL;
    CHECK(BrPhaseRun_100489A0(&s_G, &ph) == 0);
    CHECK(ph.pCur == NULL);

    /* 0x100488B0 just forwards to vtable +0x20 and reports 1. */
    CHECK(BrPhaseFn_100488B0(&ph) == 1);
}

/* ==========================================================================
 * main
 * ========================================================================== */

int main(void)
{
    InitTable();

    TestDraw();
    TestStepCode();
    TestTween();
    TestTweenStep();
    TestItemInit();
    TestInit();
    TestEnter();
    TestCheckOther();
    TestTickSteps();
    TestFrame();
    TestPage();
    TestPageSelect();
    TestPageFrame();
    TestPhaseDtor();
    TestPhaseTick();
    TestPhaseReleasePages();
    TestShutdown();
    TestPhaseRun();

    free(s_aTbl);

    if (g_nFail == 0) {
        printf("slice3_32: all tests passed\n");
        return 0;
    }
    printf("slice3_32: %d failure(s)\n", g_nFail);
    return 1;
}
