/* test_slice3_39.c -- behaviour tests for slice 3, a later pass.
 *
 * These assert properties the disassembly forces, not numbers copied out of
 * this port: the space/glyph split in the measurers, the fact that `height`
 * is seeded and only grows, the fact that a non-ASCII byte truncates the
 * walk, the centring identity, the press/hold/release shape of the edge
 * detectors, and the argument order of BrMemFill.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "slice1_07.h"   /* BrDevSlot */
#include "slice3_39.h"

static int g_fails = 0;
static int g_checks = 0;

#define CHECK(cond) do {                                                  \
        ++g_checks;                                                       \
        if (!(cond)) {                                                    \
            printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);        \
            ++g_fails;                                                    \
        }                                                                 \
    } while (0)

/* ------------------------------------------------------------------ */
/* Stand-ins for the cross-slice imports.  TEST FILE ONLY.             */
/* ------------------------------------------------------------------ */

static int   g_dtorCalls;
static int   g_deleteCalls;
static void *g_lastDeleted;
static int   g_pollResult;
static int   g_pollCalls;

void BrTextBoxDtor(BrTextBox *pBox)
{
    (void)pBox;
    ++g_dtorCalls;
}

void BrOperatorDelete(void *p)
{
    ++g_deleteCalls;
    g_lastDeleted = p;
    free(p);
}

void *BrOperatorNew(uint32_t cb)
{
    /* The real 0x1007DFE0 does NOT zero.  malloc matches that. */
    return malloc(cb ? cb : 1);
}

int32_t BrDikGetDeviceState(uint8_t *pState)
{
    ++g_pollCalls;
    (void)pState;
    return g_pollResult;
}

/* 0x1007C8A0.  br_crt.c owns the real one; this file does not link it.  The
 * body is the same rule -- truncate toward zero, and yield 0 rather than
 * INT_MIN when the value does not fit (CONVENTIONS, "Facts not to re-derive").
 * The list methods only ever feed it small in-range coordinates. */
int32_t BrFtolTrunc(float f)
{
    if (!(f > -2147483648.0f && f < 2147483648.0f)) {
        return 0;
    }
    return (int32_t)f;
}

/* 0x1039B720.  slice2_25.c owns the definition; this file does not link it. */
char g_aBr39B720[0x104];

/* ------------------------------------------------------------------ */

static void SetText(BrTextBox *pBox, const char *pSrc, size_t n)
{
    memset(pBox->sz, 0, sizeof pBox->sz);
    memcpy(pBox->sz, pSrc, n);
}

static uint16_t AdvA(unsigned char c)
{
    return g_BrGlyphFontA[c - BR_GLYPH_FIRST].advance;
}

/* ------------------------------------------------------------------ */
/* 0x1005B050 -- constructor                                           */
/* ------------------------------------------------------------------ */

static void test_init(void)
{
    BrTextBox box;
    size_t i;
    int allZero = 1;

    memset(&box, 0xCD, sizeof box);
    box.left  = 0x11111111;
    box.right = 0x22222222;
    box.f434  = 0x33333333;

    CHECK(BrTextBoxInit(&box) == &box);

    for (i = 0; i < sizeof box.sz; ++i) {
        if (box.sz[i] != 0) { allZero = 0; }
    }
    CHECK(allZero);
    CHECK(box.f08 == 1);
    CHECK(box.f04 == 0);
    CHECK(box.width == 0 && box.height == 0);
    CHECK(box.x == 0.0f && box.y == 0.0f);
    CHECK(box.f418 == 0 && box.f41C == 0 && box.f420 == 0);

    /* The geometry fields are NOT part of the constructor. */
    CHECK(box.left  == 0x11111111);
    CHECK(box.right == 0x22222222);
    CHECK(box.f434  == 0x33333333);
}

/* ------------------------------------------------------------------ */
/* 0x1005B0D0 -- measure with font A                                   */
/* ------------------------------------------------------------------ */

static void test_measure_a(void)
{
    BrTextBox box;

    BrTextBoxInit(&box);

    /* Additivity: the width of a concatenation is the sum of the parts. */
    SetText(&box, "AB", 2);
    BrTextBoxMeasureA(&box);
    CHECK(box.width == (int16_t)(AdvA('A') + AdvA('B')));
    CHECK(box.height == 16);

    /* Space has no glyph in font A, so it is worth exactly the literal 6. */
    box.height = 0;
    SetText(&box, "A B", 3);
    BrTextBoxMeasureA(&box);
    CHECK(box.width ==
          (int16_t)(AdvA('A') + BR_GLYPH_SPACE_ADVANCE + AdvA('B')));

    /* Empty string measures to zero width. */
    box.height = 0;
    SetText(&box, "", 0);
    BrTextBoxMeasureA(&box);
    CHECK(box.width == 0);
    CHECK(box.height == 0);
}

static void test_measure_a_truncates(void)
{
    BrTextBox box;
    int16_t   wAB;
    static const char ctrl[] = { 'A', 'B', '\x01', 'C', 'D', 0 };
    static const char high[] = { 'A', 'B', '\x80', 'C', 'D', 0 };
    static const char del[]  = { 'A', 'B', '\x7f', 'C', 'D', 0 };

    BrTextBoxInit(&box);
    SetText(&box, "AB", 2);
    BrTextBoxMeasureA(&box);
    wAB = box.width;

    /* A byte below 0x20 stops the walk; the partial result is kept. */
    box.height = 0;
    SetText(&box, ctrl, sizeof ctrl - 1);
    BrTextBoxMeasureA(&box);
    CHECK(box.width == wAB);

    /* So does a byte with the top bit set. */
    box.height = 0;
    SetText(&box, high, sizeof high - 1);
    BrTextBoxMeasureA(&box);
    CHECK(box.width == wAB);

    /* 0x7F is INSIDE the accepted range but outside the glyph range, so it
     * contributes nothing and the walk CONTINUES past it. */
    box.height = 0;
    SetText(&box, del, sizeof del - 1);
    BrTextBoxMeasureA(&box);
    CHECK(box.width == (int16_t)(wAB + AdvA('C') + AdvA('D')));
}

static void test_measure_height_is_seeded(void)
{
    BrTextBox box;

    BrTextBoxInit(&box);

    /* height is not reset between calls -- it only ever grows. */
    box.height = 100;
    SetText(&box, "A", 1);
    BrTextBoxMeasureA(&box);
    CHECK(box.height == 100);

    box.height = 3;
    BrTextBoxMeasureA(&box);
    CHECK(box.height == 16);

    /* The max is signed: a seed of -1 loses to 16. */
    box.height = -1;
    BrTextBoxMeasureA(&box);
    CHECK(box.height == 16);
}

/* ------------------------------------------------------------------ */
/* 0x1005B160 -- measure with font B                                   */
/* ------------------------------------------------------------------ */

static void test_measure_b(void)
{
    BrTextBox box;
    uint16_t  adv0 = g_BrGlyphFontB['0' - BR_GLYPH_FIRST].advance;

    BrTextBoxInit(&box);

    /* Font B is the digit font; it charges advance - 4 per glyph. */
    SetText(&box, "12", 2);
    BrTextBoxMeasureB(&box);
    CHECK(box.width == (int16_t)((adv0 - 4) * 2));
    CHECK(box.height == (int16_t)g_BrGlyphFontB['0' - BR_GLYPH_FIRST].height);

    /* Space fails the font-A gate, so it takes the literal-6 path here too. */
    box.height = 0;
    SetText(&box, " ", 1);
    BrTextBoxMeasureB(&box);
    CHECK(box.width == BR_GLYPH_SPACE_ADVANCE);

    /* The documented split: the sentinel is checked in font A, the metrics
     * are taken from font B.  'A' has a font-A glyph and no font-B glyph, so
     * it passes the gate and then adds 0xFFFF - 4, which wraps. */
    CHECK(g_BrGlyphFontA['A' - BR_GLYPH_FIRST].advance != BR_GLYPH_NONE);
    CHECK(g_BrGlyphFontB['A' - BR_GLYPH_FIRST].advance == BR_GLYPH_NONE);
    box.height = 0;
    SetText(&box, "A", 1);
    BrTextBoxMeasureB(&box);
    CHECK(box.width == (int16_t)(uint16_t)(BR_GLYPH_NONE - 4u));
    /* ...and 0xFFFF as a height is -1 signed, so it loses to the seed. */
    CHECK(box.height == 0);
}

/* ------------------------------------------------------------------ */
/* 0x1005B200 -- centring                                              */
/* ------------------------------------------------------------------ */

static void test_centre_x(void)
{
    BrTextBox box;
    float     v;
    float     lead, trail;

    BrTextBoxInit(&box);
    box.left  = 100;
    box.right = 300;
    box.width = 50;

    v = BrTextBoxCentreX(&box);

    /* The value is both returned and stored. */
    CHECK(v == box.x);

    /* The centring identity: equal margins either side. */
    lead  = v - (float)box.left;
    trail = (float)box.right - (v + (float)box.width);
    CHECK(fabsf(lead - trail) < 1e-4f);

    /* Wider than the span -> the result overhangs to the left, no clamp. */
    box.width = 400;
    v = BrTextBoxCentreX(&box);
    CHECK(v < (float)box.left);

    /* width is read signed, so 0x8000 is -32768, not +32768. */
    box.left  = 0;
    box.right = 0;
    box.width = (int16_t)0x8000;
    v = BrTextBoxCentreX(&box);
    CHECK(v > 0.0f);
}

/* ------------------------------------------------------------------ */
/* 0x1005B540 -- character map                                         */
/* ------------------------------------------------------------------ */

static void test_charmap(void)
{
    int i;

    /* Printable ASCII maps to itself. */
    for (i = 0x20; i <= 0x7E; ++i) {
        CHECK(BrCharMapLookup(i) == (uint8_t)i);
    }

    /* The three VK_OEM codes the map exists for. */
    CHECK(BrCharMapLookup(0xBA) == ':');
    CHECK(BrCharMapLookup(0xBD) == '-');
    CHECK(BrCharMapLookup(0xBE) == '.');

    /* Anything else is a miss, and a miss is 0 -- which is what 0x1005B570
     * treats as "no character to append". */
    CHECK(BrCharMapLookup(0x00) == 0);
    CHECK(BrCharMapLookup(0x1F) == 0);
    CHECK(BrCharMapLookup(0x7F) == 0);
    CHECK(BrCharMapLookup(0x1234) == 0);
    CHECK(BrCharMapLookup(-1) == 0);
}

/* ------------------------------------------------------------------ */
/* 0x1005B7F0 / 0x1005B8D0 / 0x1005B8F0 / 0x1005C200                   */
/* ------------------------------------------------------------------ */

static void test_list(void)
{
    BrTextList *pList = (BrTextList *)malloc(sizeof *pList);
    int i;
    int allInit = 1;
    void *pFirst;
    unsigned char src[16];
    unsigned char big[64];

    if (pList == NULL) { CHECK(0); return; }
    memset(pList, 0xCD, sizeof *pList);

    CHECK(BrTextListInit(pList) == pList);

    for (i = 0; i < BR_TEXTLIST_ITEMS; ++i) {
        if (pList->aItems[i].f08 != 1 || pList->aItems[i].sz[0] != 0) {
            allInit = 0;
        }
        if (pList->aBlobs[i].p != NULL || pList->aBlobs[i].size != 0) {
            allInit = 0;
        }
    }
    CHECK(allInit);
    CHECK(pList->count == 0);
    CHECK(pList->f1A932 == -1 && pList->f1A934 == -1);
    CHECK(pList->f1A936 == -1 && pList->f1A938 == -1);

    /* index == -1 with count == 0 clamps up to slot 0. */
    for (i = 0; i < (int)sizeof src; ++i) { src[i] = (unsigned char)(i + 1); }
    CHECK(BrTextListSetBlob(pList, src, sizeof src, -1) == 1);
    CHECK(pList->aBlobs[0].p != NULL);
    CHECK(pList->aBlobs[0].size == sizeof src);
    CHECK(memcmp(pList->aBlobs[0].p, src, sizeof src) == 0);

    /* index == -1 with count == 5 means slot 4. */
    pList->count = 5;
    CHECK(BrTextListSetBlob(pList, src, sizeof src, -1) == 1);
    CHECK(pList->aBlobs[4].p != NULL);
    CHECK(pList->aBlobs[3].p == NULL);

    /* Re-storing into a live slot reuses the SAME allocation.  (That is the
     * bug: a larger size would overrun.  Only the no-growth case is
     * exercised here.) */
    pFirst = pList->aBlobs[0].p;
    memset(big, 0x5A, sizeof big);
    CHECK(BrTextListSetBlob(pList, big, sizeof src, 0) == 1);
    CHECK(pList->aBlobs[0].p == pFirst);
    CHECK(((unsigned char *)pList->aBlobs[0].p)[0] == 0x5A);

    /* Destructor: one per item, and the deleting form only frees on bit 0. */
    g_dtorCalls = 0;
    g_deleteCalls = 0;
    BrTextListDtor(pList);
    CHECK(g_dtorCalls == BR_TEXTLIST_ITEMS);

    g_dtorCalls = 0;
    CHECK(BrTextListDeleteDtor(pList, 0) == pList);
    CHECK(g_dtorCalls == BR_TEXTLIST_ITEMS);
    CHECK(g_deleteCalls == 0);

    free(pList->aBlobs[0].p);
    free(pList->aBlobs[4].p);
    free(pList);
}

static void test_scalar_deleting_dtor(void)
{
    BrTextBox *pBox = (BrTextBox *)malloc(sizeof *pBox);

    if (pBox == NULL) { CHECK(0); return; }
    BrTextBoxInit(pBox);

    g_dtorCalls = 0;
    g_deleteCalls = 0;
    CHECK(BrTextBoxDeleteDtor(pBox, 0) == pBox);
    CHECK(g_dtorCalls == 1);
    CHECK(g_deleteCalls == 0);

    /* Bit 0 set -> free.  Every other bit is ignored. */
    CHECK(BrTextBoxDeleteDtor(pBox, 3) == pBox);
    CHECK(g_dtorCalls == 2);
    CHECK(g_deleteCalls == 1);
    CHECK(g_lastDeleted == (void *)pBox);
}

/* ------------------------------------------------------------------ */
/* 0x1005FF60 / 0x1005FFB0 / 0x1005FFD0 / 0x1005FFF0                   */
/* ------------------------------------------------------------------ */

static void test_key_edges(void)
{
    memset(g_BrDikState, 0, sizeof g_BrDikState);
    memset(g_BrDikPrev,  0, sizeof g_BrDikPrev);
    memset(g_BrDikEdge,  0, sizeof g_BrDikEdge);

    /* Nothing held: no edges, and the scan reports -1. */
    BrMenuSub1005FF60();
    CHECK(BrFn1005FFD0() == -1);

    /* Press: the edge fires exactly once. */
    g_BrDikState[0x3B] = 0x80;              /* DIK_F1 */
    BrMenuSub1005FF60();
    CHECK(g_BrDikEdge[0x3B] == 1);
    CHECK(BrFn1005FFD0() == 0x3B);

    /* Hold: no further edge. */
    BrMenuSub1005FF60();
    CHECK(g_BrDikEdge[0x3B] == 0);
    CHECK(BrFn1005FFD0() == -1);

    /* Release then press again: the edge fires again -- a round trip. */
    g_BrDikState[0x3B] = 0x00;
    BrMenuSub1005FF60();
    CHECK(g_BrDikEdge[0x3B] == 0);
    g_BrDikState[0x3B] = 0x80;
    BrMenuSub1005FF60();
    CHECK(g_BrDikEdge[0x3B] == 1);

    /* Only bit 7 counts: 0x7F is "not down". */
    memset(g_BrDikPrev, 0, sizeof g_BrDikPrev);
    g_BrDikState[0x10] = 0x7F;
    BrMenuSub1005FF60();
    CHECK(g_BrDikEdge[0x10] == 0);
    CHECK(g_BrDikPrev[0x10] == 0);

    /* The scan returns the LOWEST index that fired. */
    memset(g_BrDikState, 0, sizeof g_BrDikState);
    memset(g_BrDikPrev,  0, sizeof g_BrDikPrev);
    g_BrDikState[0xC8] = 0x80;
    g_BrDikState[0x02] = 0x80;
    BrMenuSub1005FF60();
    CHECK(BrFn1005FFD0() == 0x02);

    /* The whole 256-entry range is live, not just the low 64. */
    memset(g_BrDikState, 0, sizeof g_BrDikState);
    memset(g_BrDikPrev,  0, sizeof g_BrDikPrev);
    memset(g_BrDikEdge,  0, sizeof g_BrDikEdge);
    g_BrDikState[0xFF] = 0x80;
    BrMenuSub1005FF60();
    CHECK(g_BrDikEdge[0xFF] == 1);
    CHECK(BrFn1005FFD0() == 0xFF);
}

static void test_poll_gate(void)
{
    memset(g_BrDikState, 0, sizeof g_BrDikState);
    memset(g_BrDikPrev,  0, sizeof g_BrDikPrev);
    memset(g_BrDikEdge,  0, sizeof g_BrDikEdge);
    g_BrDikState[1] = 0x80;

    /* A negative result suppresses the edge pass entirely. */
    g_pollResult = -1;
    g_pollCalls = 0;
    BrDikPollAndEdge();
    CHECK(g_pollCalls == 1);
    CHECK(g_BrDikEdge[1] == 0);

    /* Zero is a pass -- the test is `jl`, not `jle`. */
    g_pollResult = 0;
    BrDikPollAndEdge();
    CHECK(g_BrDikEdge[1] == 1);
}

static void test_button_edges(void)
{
    memset(g_BrBtnRaw,  0, sizeof g_BrBtnRaw);
    memset(g_BrBtnPrev, 0, sizeof g_BrBtnPrev);
    memset(g_BrBtnEdge, 0, sizeof g_BrBtnEdge);

    /* Unlike the keyboard path this ANDs with the RAW dword, so a raw value
     * of 5 yields an edge of 1 & 5 == 1. */
    g_BrBtnRaw[2] = 5;
    BrMenuSub1005FFF0();
    CHECK(g_BrBtnEdge[2] == 1);
    CHECK(g_BrBtnPrev[2] == 5);

    /* Held: prev is non-zero, so the edge clears. */
    BrMenuSub1005FFF0();
    CHECK(g_BrBtnEdge[2] == 0);

    /* Released and pressed again -> edge returns. */
    g_BrBtnRaw[2] = 0;
    BrMenuSub1005FFF0();
    CHECK(g_BrBtnEdge[2] == 0);
    g_BrBtnRaw[2] = 1;
    BrMenuSub1005FFF0();
    CHECK(g_BrBtnEdge[2] == 1);

    /* An even raw value ANDs to 0 even on a genuine press -- reproduced. */
    memset(g_BrBtnPrev, 0, sizeof g_BrBtnPrev);
    g_BrBtnRaw[0] = 2;
    BrMenuSub1005FFF0();
    CHECK(g_BrBtnEdge[0] == 0);
    CHECK(g_BrBtnPrev[0] == 2);
}

/* ------------------------------------------------------------------ */
/* 0x10060210 / 0x100602B0 / 0x10060780                                */
/* ------------------------------------------------------------------ */

static void test_10060210(void)
{
    BrPointI pt;
    int i;
    int allZero = 1;

    g_pBrAA2E80 = &pt;
    memset(g_BrAA3398, 0x7F, sizeof g_BrAA3398);

    g_Br0A81C0 = 640;
    g_Br0A81C4 = 480;
    CHECK(BrFn10060210(NULL) == 1);
    CHECK(pt.x == 320 && pt.y == 240);
    CHECK(g_BrAA33B8 == 640 && g_BrAA33B4 == 480);

    for (i = 0; i < 7; ++i) {
        if (g_BrAA3398[i] != 0) { allZero = 0; }
    }
    CHECK(allZero);

    /* cdq/sub/sar halves toward zero, so -5 gives -2, not -3. */
    g_Br0A81C0 = -5;
    g_Br0A81C4 = -1;
    BrFn10060210((void *)&pt);
    CHECK(pt.x == -2);
    CHECK(pt.y == 0);
}

static int g_ifaceLog[8];
static int g_ifaceN;

static void IfaceF08(void *pThis) { (void)pThis; g_ifaceLog[g_ifaceN++] = 0x08; }
static void IfaceF1C(void *pThis) { (void)pThis; g_ifaceLog[g_ifaceN++] = 0x1C; }
static void IfaceF20(void *pThis) { (void)pThis; g_ifaceLog[g_ifaceN++] = 0x20; }

static void test_release_iface(void)
{
    BrDevIfaceVtbl vtbl;
    BrDevIface     iface;
    BrDevSlot      slot;

    memset(&vtbl, 0, sizeof vtbl);
    vtbl.pfn08 = IfaceF08;
    vtbl.pfn1C = IfaceF1C;
    vtbl.pfn20 = IfaceF20;
    iface.pVtbl = &vtbl;

    memset(&slot, 0, sizeof slot);
    slot.pIface = &iface;

    g_ifaceN = 0;
    BrDevSlotReleaseIface(&slot);
    CHECK(g_ifaceN == 2);
    CHECK(g_ifaceLog[0] == 0x20);   /* +0x20 runs BEFORE +0x08 */
    CHECK(g_ifaceLog[1] == 0x08);
    CHECK(slot.pIface == NULL);

    /* Idempotent: a second call on a cleared slot does nothing. */
    g_ifaceN = 0;
    BrDevSlotReleaseIface(&slot);
    CHECK(g_ifaceN == 0);
}

static void test_memfill(void)
{
    unsigned char buf[32];
    int i;
    int ok = 1;

    memset(buf, 0xAA, sizeof buf);

    /* (dst, COUNT, value) -- the value is the LAST argument.  If the order
     * were memset's, this call would write 7 bytes of 0x00. */
    BrMemFill(buf, 7, 0x5A);
    for (i = 0; i < 7; ++i) {
        if (buf[i] != 0x5A) { ok = 0; }
    }
    CHECK(ok);
    CHECK(buf[7] == 0xAA);      /* no overrun past the count */

    /* Only the low byte of `value` is used. */
    BrMemFill(buf + 8, 4, 0x11223344);
    CHECK(buf[8] == 0x44 && buf[11] == 0x44);
    CHECK(buf[12] == 0xAA);

    /* A zero count writes nothing. */
    BrMemFill(buf + 16, 0, 0xFF);
    CHECK(buf[16] == 0xAA);
}

/* ------------------------------------------------------------------ */

int main(void)
{
    test_init();
    test_measure_a();
    test_measure_a_truncates();
    test_measure_height_is_seeded();
    test_measure_b();
    test_centre_x();
    test_charmap();
    test_list();
    test_scalar_deleting_dtor();
    test_key_edges();
    test_poll_gate();
    test_button_edges();
    test_10060210();
    test_release_iface();
    test_memfill();

    printf("slice3_39: %d checks, %d failures\n", g_checks, g_fails);
    return g_fails != 0;
}
