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

/* Every code 0..0xFF whose answer is NOT zero, decoded out of the two shipped
 * images (both agree, byte for byte) rather than taken from any comment. The
 * loop below asserts this table AND asserts zero for every code not in it, so
 * it is exhaustive over the function's whole reachable input domain -- adding
 * a spurious record or deleting a real one both fail. */
static const struct { int32_t code; uint8_t ch; } s_aCharMapNonZero[] = {
    { 0x00, 0x54 },   /* "Time" -- see the UNREACHABLE note below           */
    { 0x09, 0x09 },   /* TAB                                               */
    { 0x80, 0x1C },
    { 0xA0, 0x34 },
    { 0xAA, 0xC6 },
    { 0xBA, 0x3A },   /* ':'  -- one of the three real VK_OEM records      */
    { 0xBD, 0x2D },   /* '-'                                               */
    { 0xBE, 0x2E },   /* '.'                                               */
    { 0xD0, 0xE1 },
    { 0xE2, 0xFF },
    { 0xF0, 0xFF }
    /* 0x20..0x7E identity is handled by the loop, not listed. */
};

static void test_charmap(void)
{
    int i, k;

    /* Printable ASCII maps to itself. */
    for (i = 0x20; i <= 0x7E; ++i) {
        CHECK(BrCharMapLookup(i) == (uint8_t)i);
    }

    /* EXHAUSTIVE over 0..0xFF. The original's loop bound is an ADDRESS, so
     * it walks 784 records -- 686 of them past the module's real table -- and
     * fifteen of those accidental (code, ch) pairs have a code a WM_CHAR
     * wParam can carry. This port transcribed 98 of the 784 and every one of
     * the fifteen answered 0 instead. */
    for (i = 0; i <= 0xFF; ++i) {
        uint8_t want = (i >= 0x20 && i <= 0x7E) ? (uint8_t)i : 0u;
        for (k = 0; k < (int)(sizeof s_aCharMapNonZero
                              / sizeof s_aCharMapNonZero[0]); ++k) {
            if (s_aCharMapNonZero[k].code == i) {
                want = s_aCharMapNonZero[k].ch;
                break;
            }
        }
        ++g_checks;
        if (BrCharMapLookup(i) != want) {
            printf("FAIL %s:%d  BrCharMapLookup(0x%02X) == 0x%02X, want 0x%02X\n",
                   __FILE__, __LINE__, i, BrCharMapLookup(i), want);
            ++g_fails;
        }
    }

    /* THE ONE THAT MATTERS. Pressing Tab appends a character in the original.
     * Record 544 (BRGlide 557) is the pair of dwords (9, 9) that sits just
     * before the first intensity ramp at 0x100ADF6C; the record framing turns
     * it into code 9 -> 0x09. This port returned 0 and appended nothing. */
    CHECK(BrCharMapLookup(0x09) == 0x09);

    /* UNREACHABLE, AND SAID SO. Code 0 matches record 156 (BRGlide 163),
     * whose `ch` dword is 0x656D6954 -- the first four bytes of the string
     * "Time" -- so the function returns 0x54, 'T'. The port used to return 0
     * here and a test asserted that WRONG answer as if it were verified.
     *
     * It cannot happen in the shipped game, and the instruction that stops it
     * is the reason, not an assumption:
     *
     *     1005B6A6  a1e433aa10  mov eax, [0x10AA33E4]
     *     1005B6AB  3bc5        cmp eax, ebp        ; ebp == 0
     *     1005B6AD  746d        je  0x1005B71C      ; never reaches the lookup
     *
     * The value is still asserted, because a caller-side filter is not a
     * property of BrCharMapLookup. */
    CHECK(BrCharMapLookup(0x00) == 0x54);

    /* Code 8 is diverted the same way, by `cmp eax, 8 / jne 0x1005B6E0` at
     * 0x1005B6AF -- it is the backspace path. No record has code 8 in either
     * image, so the answer would have been 0 regardless. */
    CHECK(BrCharMapLookup(0x08) == 0);

    /* Genuine misses -- no record in either image carries these codes. */
    CHECK(BrCharMapLookup(0x1F) == 0);
    CHECK(BrCharMapLookup(0x7F) == 0);
    CHECK(BrCharMapLookup(0x1234) == 0);

    /* -1 IS ZERO FOR A DIFFERENT REASON, and the distinction is why this one
     * is kept: 0xFFFFFFFF is not a miss. It MATCHES record 561 (BRGlide 574),
     * one of the -1 fields of the 0x20-byte descriptor block at 0x100AE080,
     * whose paired dword is 0. The port reaches the same answer by missing,
     * which is the right answer by luck. Unreachable anyway -- the wParam is
     * 0..0xFF. */
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

/* ------------------------------------------------------------------ */
/* 0x1005B910 / 0x1005BC10 -- the two public vtable methods             */
/* ------------------------------------------------------------------ */

static int g_measA, g_measB;
static void MeasA(BrTextBox *p) { (void)p; ++g_measA; }
static void MeasB(BrTextBox *p) { (void)p; ++g_measB; }
static BrTextBoxVtbl g_itemVtbl;

static int g_f0CCalls;
static void ListF0C(void) { ++g_f0CCalls; }

static int g_f2CCalls;
static int32_t g_f2CArg;
static void ListF2C(BrTextList *p, int32_t a1)
{ (void)p; g_f2CArg = a1; ++g_f2CCalls; }
static BrTextListVtbl g_listVtbl;

/* A fresh, fully-constructed list.  The item vtable has to be in place BEFORE
 * construction, because BrTextListInit is what copies it into the hundred
 * items -- which is exactly the ordering the host harness has to respect too. */
static BrTextList *NewList(void)
{
    BrTextList *pList = (BrTextList *)malloc(sizeof *pList);
    if (pList == NULL) { return NULL; }

    /* 0xCD, not 0: the original's allocator does not zero, and both methods
     * have paths that read fields no constructor writes. */
    memset(pList, 0xCD, sizeof *pList);

    g_itemVtbl.pfn04 = MeasA;
    g_itemVtbl.pfn08 = MeasB;
    g_pBrTextBoxVtbl = &g_itemVtbl;

    g_listVtbl.f10 = BrTextListAddRow;
    g_listVtbl.f14 = BrTextListConfig;
    g_listVtbl.f2C = ListF2C;
    g_pBrTextListVtbl = &g_listVtbl;

    BrTextListInit(pList);
    return pList;
}

static void FreeList(BrTextList *pList)
{
    int i;
    for (i = 0; i < BR_TEXTLIST_ITEMS; ++i) { free(pList->aBlobs[i].p); }
    free(pList);
}

/* The rectangle both crashing builders pass, entry 16 of the pool. */
static const BrTextStyle g_rc = { 188, 130, 300, 206 };

static void test_config_copies_and_literals(void)
{
    BrTextList *pList = NewList();
    if (pList == NULL) { CHECK(0); return; }

    CHECK(pList->f1A932 == -1 && pList->f1A934 == -1 && pList->f1A938 == -1);

    CHECK(BrTextListConfig(pList, 0x40001, &g_rc, 4, 0, -1) == 1);

    /* The rectangle lands twice: verbatim as four dwords, and as two floats. */
    CHECK(pList->f1A93C == g_rc.left  && pList->f1A940 == g_rc.top);
    CHECK(pList->f1A944 == g_rc.right && pList->f1A948 == g_rc.bottom);
    CHECK(pList->f1C.f == (float)g_rc.left);
    CHECK(pList->f20.f == (float)g_rc.top);

    CHECK(pList->f18 == 0x40001u);

    /* The three -1 sentinels become '0', '.' and ':' -- and f1A936, which
     * LOOKS like a fourth sentinel, is an argument instead. */
    CHECK(pList->f1A932 == 0x30);
    CHECK(pList->f1A934 == 0x2E);
    CHECK(pList->f1A938 == 0x3A);
    CHECK(pList->f1A936 == -1);

    CHECK(pList->f1A930 == 4);
    CHECK(pList->f1A92E == 0);

    /* Three int32 arguments into three int16 fields: the high half is
     * DISCARDED, not saturated. */
    CHECK(BrTextListConfig(pList, 0, &g_rc, 0x10005, 0x20007, 0x30009) == 1);
    CHECK(pList->f1A930 == 5);
    CHECK(pList->f1A92E == 7);
    CHECK(pList->f1A936 == 9);

    FreeList(pList);
}

static void test_config_scrollbar_arms(void)
{
    BrTextList *pList = NewList();
    int32_t dx, dy;

    if (pList == NULL) { CHECK(0); return; }
    dx = g_BrSprRect48[2] - g_BrSprRect48[0];
    dy = g_BrSprRect48[3] - g_BrSprRect48[1];

    /* --- neither arm.  The vertical and horizontal blocks stay untouched,
     * and the run is still not a no-op: the tail always executes. */
    pList->f1A99C[7].u = 0;
    pList->f1A99C[8].u = 0;
    pList->f1A94C = 0x5A5A;
    pList->f1A96C = 0xA5A5;
    pList->f1A99C[4].f = 40.0f;
    pList->f1A99C[5].f = 70.0f;
    CHECK(BrTextListConfig(pList, 0, &g_rc, 4, 0, -1) == 1);
    CHECK(pList->f1A94C == 0x5A5A);
    CHECK(pList->f1A96C == 0xA5A5);
    CHECK(pList->f1A98C == 40 && pList->f1A990 == 70);

    /* --- the VERTICAL arm, which is the one both menu builders take. */
    BrTextListInit(pList);
    pList->f1A99C[8].i = 1;
    CHECK(BrTextListConfig(pList, 0x40001, &g_rc, 4, 0, -1) == 1);

    /* The horizontal block is not written on this arm. */
    CHECK(pList->f1A96C != g_rc.left || g_rc.left == 0);

    /* The identity that pins the sign of the constant at 0x1008F6B0: the
     * handle starts AT the low end of its travel, not two pixels above it. */
    CHECK(pList->f1A99C[5].f == pList->f1A99C[11].f);
    /* ... and the travel length really is high minus low. */
    CHECK(pList->f1A99C[13].f ==
          pList->f1A99C[12].f - pList->f1A99C[11].f);
    /* The bar sits just outside the rectangle's right edge, both boxes. */
    CHECK(pList->f1A94C == g_rc.right + 3);
    CHECK(pList->f1A95C == g_rc.right + 3);
    CHECK(pList->f1A968 == g_rc.bottom);
    CHECK(pList->f1A99C[4].f == (float)(g_rc.right + 3));

    /* --- the HORIZONTAL arm.  Same two identities on its own pair of
     * limits, which is what says the two arms are the same construction. */
    BrTextListInit(pList);
    pList->f1A99C[7].i = 1;
    CHECK(BrTextListConfig(pList, 0x40001, &g_rc, 4, 0, -1) == 1);
    CHECK(pList->f1A99C[4].f == pList->f1A99C[9].f);
    CHECK(pList->f1A99C[13].f ==
          pList->f1A99C[10].f - pList->f1A99C[4].f);
    CHECK(pList->f1A96C == g_rc.left);
    CHECK(pList->f1A984 == g_rc.right);
    CHECK(pList->f1A970 == g_rc.bottom + 3);

    /* --- [7] wins when both are set. */
    BrTextListInit(pList);
    pList->f1A99C[7].i = 1;
    pList->f1A99C[8].i = 1;
    pList->f1A94C = 0x5A5A;
    CHECK(BrTextListConfig(pList, 0, &g_rc, 4, 0, -1) == 1);
    CHECK(pList->f1A94C == 0x5A5A);

    /* --- the tail's three fields, on whichever arm ran. */
    CHECK(pList->f1A98C == BrFtolTrunc(pList->f1A99C[4].f));
    CHECK(pList->f1A990 == BrFtolTrunc(pList->f1A99C[5].f));
    CHECK(pList->f1A994 == dx + pList->f1A98C);
    CHECK(pList->f1A998 == dy + pList->f1A990);

    FreeList(pList);
}

static void test_config_clamps_arrow_size(void)
{
    BrTextList *pList = NewList();
    int32_t save[4];

    if (pList == NULL) { CHECK(0); return; }
    memcpy(save, g_BrSprRect48, sizeof save);

    /* An inside-out arrow rectangle clamps to zero rather than being read as
     * an absolute size -- `test/jge/xor`, not a negation. */
    g_BrSprRect48[0] = 40; g_BrSprRect48[2] = 10;
    g_BrSprRect48[1] = 40; g_BrSprRect48[3] = 10;

    pList->f1A99C[7].i = 1;
    CHECK(BrTextListConfig(pList, 0, &g_rc, 4, 0, -1) == 1);
    /* dx == 0, so the travel's low edge is the rectangle's own left edge. */
    CHECK(pList->f1A974 == g_rc.left);
    CHECK(pList->f1A994 == pList->f1A98C);
    CHECK(pList->f1A998 == pList->f1A990);

    memcpy(g_BrSprRect48, save, sizeof save);
    FreeList(pList);
}

static void test_addrow(void)
{
    BrTextList *pList = NewList();
    int i;
    int pitchOk = 1;

    if (pList == NULL) { CHECK(0); return; }
    BrTextListConfig(pList, 0x40001, &g_rc, 4, 0, -1);

    /* NULL text is the one early-out that changes nothing at all. */
    g_measA = g_measB = 0;
    CHECK(BrTextListAddRow(pList, NULL, 0, 1, &g_rc, 1) == 0);
    CHECK(pList->count == 0);
    CHECK(g_measA == 0 && g_measB == 0);

    /* a5 != 0 -- the whole string. */
    CHECK(BrTextListAddRow(pList, "Season One", 0, 1, &g_rc, 1) == 1);
    CHECK(pList->count == 1);
    CHECK(strcmp(pList->aItems[0].sz, "Season One") == 0);

    /* a5 == 0 -- ten characters and no more.  The append that follows is of
     * the CRT's empty probe buffer, so it adds nothing. */
    CHECK(BrTextListAddRow(pList, "ABCDEFGHIJKLMNOP", 0, 1, &g_rc, 0) == 1);
    CHECK(strcmp(pList->aItems[1].sz, "ABCDEFGHIJ") == 0);

    /* A short source is still NUL-terminated on that path. */
    CHECK(BrTextListAddRow(pList, "abc", 0, 1, &g_rc, 0) == 1);
    CHECK(strcmp(pList->aItems[2].sz, "abc") == 0);

    /* Geometry: the style's two horizontal edges, the derived width limit,
     * and y as a function of the row index. */
    CHECK(pList->aItems[0].left  == g_rc.left);
    CHECK(pList->aItems[0].right == g_rc.right);
    CHECK(pList->aItems[0].f41C  ==
          (int16_t)(g_rc.right - g_rc.left - 0x10));
    CHECK(pList->aItems[0].x == (float)g_rc.left);
    CHECK(pList->aItems[0].y == (float)pList->aItems[0].f428);
    CHECK(pList->aItems[0].f430 == pList->aItems[0].f428 + 0x12);
    CHECK(pList->aItems[0].f428 == BrFtolTrunc(pList->f20.f));

    /* The row PITCH is the invariant, not any one row's absolute y. */
    for (i = 1; i < (int)pList->count; ++i) {
        if (pList->aItems[i].f428 - pList->aItems[i - 1].f428 != 19) {
            pitchOk = 0;
        }
    }
    CHECK(pitchOk);

    /* Measure dispatch: a3 == 3 picks font B, anything else font A. */
    g_measA = g_measB = 0;
    CHECK(BrTextListAddRow(pList, "9", 0, 3, &g_rc, 1) == 1);
    CHECK(g_measB == 1 && g_measA == 0);
    CHECK(pList->aItems[3].f08 == 3);
    CHECK(BrTextListAddRow(pList, "x", 0, 1, &g_rc, 1) == 1);
    CHECK(g_measA == 1);

    /* f04 is OR-ed into, not assigned. */
    pList->aItems[5].f04 = 0x100;
    CHECK(BrTextListAddRow(pList, "y", 0x011, 1, &g_rc, 1) == 1);
    CHECK(pList->aItems[5].f04 == 0x111u);

    FreeList(pList);
}

static void test_addrow_full_list_calls_2c(void)
{
    BrTextList *pList = NewList();

    if (pList == NULL) { CHECK(0); return; }
    BrTextListConfig(pList, 0x40001, &g_rc, 4, 0, -1);

    pList->count = BR_TEXTLIST_ITEMS;
    g_f2CCalls = 0; g_f2CArg = -1;
    CHECK(BrTextListAddRow(pList, "over", 0, 1, &g_rc, 1) == 1);
    CHECK(g_f2CCalls == 1);
    CHECK(g_f2CArg == 0);
    /* The list does NOT grow: slot 99 is rewritten and the count comes back
     * to 100.  A hundred and first row is a hundredth row. */
    CHECK(pList->count == BR_TEXTLIST_ITEMS);
    CHECK(strcmp(pList->aItems[BR_TEXTLIST_ITEMS - 1].sz, "over") == 0);

    FreeList(pList);
}

static void test_addrow_scroll_block(void)
{
    BrTextList *pList = NewList();
    float lo, hi;

    if (pList == NULL) { CHECK(0); return; }

    /* The block is reached only with bit 23 of the flag word.  Every ported
     * caller passes 0x40001, so this is the arm the builders never take.
     *
     * The row it compares is aItems[f1A930 + f1A92E] -- the first row BELOW
     * the visible window, NOT the row just appended.  Configuring with a
     * visible-row count of 0 makes the two coincide, which is the only reason
     * the assertions below can name a row at all. */
    pList->f1A99C[8].i = 1;
    BrTextListConfig(pList, 0x800000, &g_rc, 0, 0, -1);
    pList->f0C = ListF0C;
    lo = pList->f1A99C[11].f;
    hi = pList->f1A99C[12].f;

    /* Row 0 is "Alpha", the buffer is empty: no match, so the offset moves,
     * the callback fires, and the handle stays inside its limits. */
    g_aBr39B720[0] = '\0';
    g_f0CCalls = 0;
    CHECK(BrTextListAddRow(pList, "Alpha", 0, 1, &g_rc, 1) == 1);
    CHECK(g_f0CCalls == 1);
    /* Bumped to 1, then clamped back down to count - 1 == 0. */
    CHECK(pList->f1A92E == 0);
    CHECK(pList->f1A99C[5].f >= lo && pList->f1A99C[5].f <= hi);
    CHECK(pList->f1A998 == pList->f1A990 + 0x10);

    /* Now point the selection at row 1 and give the buffer that row's text in
     * the OTHER case.  The compare is case-insensitive -- the whole reason it
     * is _stricmp and not strcmp -- so this matches, and a match abandons the
     * scroll update and returns 0.  The row is still appended: the early-out
     * is at the END of the function, not the start. */
    pList->f1A930 = 1;
    strcpy(g_aBr39B720, "bravo");
    g_f0CCalls = 0;
    CHECK(BrTextListAddRow(pList, "BRAVO", 0, 1, &g_rc, 1) == 0);
    CHECK(pList->count == 2);
    CHECK(strcmp(pList->aItems[1].sz, "BRAVO") == 0);
    CHECK(g_f0CCalls == 0);

    /* One byte different and it is not a match any more, so the same call
     * shape returns 1.  This is the pair that says the 0 above came from the
     * compare and not from something incidental. */
    strcpy(g_aBr39B720, "bravo!");
    pList->f1A930 = 1;
    pList->f1A92E = 0;
    g_f0CCalls = 0;
    CHECK(BrTextListAddRow(pList, "Charlie", 0, 1, &g_rc, 1) == 1);
    CHECK(g_f0CCalls == 1);

    /* The clamp is a clamp: drive the handle far past the top and it lands
     * exactly on the limit rather than overshooting. */
    pList->f1A930 = 1;
    pList->f1A92E = 0;
    pList->f1A99C[5].f  = hi;
    pList->f1A99C[13].f = 100000.0f;
    CHECK(BrTextListAddRow(pList, "Delta", 0, 1, &g_rc, 1) == 1);
    CHECK(pList->f1A99C[5].f == hi);

    /* ... and the same on the low side, which is also the side an unordered
     * compare takes: `test ah,1` is C0 alone, so a NaN would clamp here. */
    pList->f1A930 = 1;
    pList->f1A92E = 0;
    pList->f1A99C[5].f  = lo;
    pList->f1A99C[13].f = -100000.0f;
    CHECK(BrTextListAddRow(pList, "Echo", 0, 1, &g_rc, 1) == 1);
    CHECK(pList->f1A99C[5].f == lo);

    g_aBr39B720[0] = '\0';
    FreeList(pList);
}

static void test_style_pool(void)
{
    /* The pool's own arithmetic, not its contents: BR_UI_STYLE is address
     * indexed, so what is worth asserting is that the address the disassembly
     * shows and the entry the table holds are the same object -- and that the
     * table ends exactly where the sprite table at 0x100AB568 begins, which is
     * what pins the extent. */
    CHECK(BR_UI_STYLE(BR_UI_STYLE_BASE) == &g_aBrUiStyle[0]);
    CHECK(BR_UI_STYLE(0x100AB538) == &g_aBrUiStyle[18]);
    CHECK(BR_UI_STYLE_BASE + BR_UI_STYLE_COUNT * 16u == 0x100AB568u);

    /* Entry 18 is the rectangle 0x1004F700 passes and entry 12 the one
     * 0x1005A6E0 passes; both are the same 188..300 column. (Both were two
     * lower before 0x10047A60 pinned the pool's base at 0x100AB418; the
     * ADDRESS-indexed form is the one that did not have to change.) */
    CHECK(BR_UI_STYLE(0x100AB538)->left  == 188);
    CHECK(BR_UI_STYLE(0x100AB538)->right == 300);
    CHECK(BR_UI_STYLE(0x100AB4D8)->left  == 188);
    CHECK(BR_UI_STYLE(0x100AB4D8)->right == 300);

    /* 0x100AB438 is the screen. It used to be entry 0 and the base; it is now
     * entry 2, because 0x10047A60 reads the two below it as rectangles. */
    CHECK(BR_UI_STYLE(0x100AB438)->right  == 639);
    CHECK(BR_UI_STYLE(0x100AB438)->bottom == 479);

    /* The two entries the new reader pinned, in the order it tests them:
     * 0x100AB448, then 0x100AB418, then 0x100AB428. */
    CHECK(BR_UI_STYLE(0x100AB418)->left == 0
       && BR_UI_STYLE(0x100AB418)->top == 0
       && BR_UI_STYLE(0x100AB418)->right == 200
       && BR_UI_STYLE(0x100AB418)->bottom == 200);
    CHECK(BR_UI_STYLE(0x100AB428)->left == 0
       && BR_UI_STYLE(0x100AB428)->top == 380
       && BR_UI_STYLE(0x100AB428)->right == 200
       && BR_UI_STYLE(0x100AB428)->bottom == 480);
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
    test_config_copies_and_literals();
    test_config_scrollbar_arms();
    test_config_clamps_arrow_size();
    test_addrow();
    test_addrow_full_list_calls_2c();
    test_addrow_scroll_block();
    test_style_pool();
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
