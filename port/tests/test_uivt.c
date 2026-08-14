/* test_uivt.c -- 0x10048470, 0x10047EB0 and 0x10047FB0 over the
 * BrUiPage_ / BrUiCtl_ model.
 *
 * What is worth asserting here, and what is not:
 *
 *  - NOT "the page constructor zeroes 200 pointers". Volume proves nothing.
 *    What is asserted is the BOUNDARY: apCtl[199] is cleared and the float
 *    that follows it is too, because 0x18 + 200*4 == 0x338 is the fact that
 *    pins the array length, and an off-by-one there is the whole risk.
 *
 *  - The page fill really is a ZERO fill, unlike four of the control
 *    constructor's, and the control's +0x2A40 really is 0xFFFF. Both are
 *    checked against a poisoned object so "left alone" cannot pass.
 *
 *  - The two vtable methods are checked for the properties that a
 *    plausible-but-wrong port would get wrong:
 *      * f34 takes the rectangle's x edges from the STYLE block, not from the
 *        placement coordinates;
 *      * f34 takes y from the control's +0x40, truncated toward zero;
 *      * f34 re-reads the box AFTER the dispatch, so a measuring hook decides
 *        +0x48 / +0x4A / +0x5C;
 *      * kind 3 dispatches +0x08 and every other kind dispatches +0x04;
 *      * the +0x28 call is driven by the ARGUMENT's bit 0, not by the item
 *        flags field it was OR-ed into -- these differ, and the test makes
 *        them differ;
 *      * f38 ORs into +0x1C/+0x24/+0x28 and assigns everything else, so
 *        calling it twice accumulates in three places and only three;
 *      * the code word lands in BOTH +0x2A40 and +0x1E20C.
 *
 *  - An ordering invariant: f38 then f34 is the order every builder uses, and
 *    f34 consumes what f38 wrote. Running them in the other order is checked
 *    to produce a DIFFERENT y, which is what makes the dependency real rather
 *    than asserted.
 */
#include "br_uivt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_checks, g_fails;
#define CHECK(cond, msg) do { g_checks++; if (!(cond)) { \
    g_fails++; printf("  [FAIL] %s (%s:%d)\n", (msg), __FILE__, __LINE__); } } while (0)

/* --- a recording text-box vtable ---------------------------------------- */

static int s_c04, s_c08, s_c28;

/* Stand-ins for BrTextBoxMeasureA / MeasureB. They write width and height,
 * which is the only thing 0x10047EB0 cares about, and they write DIFFERENT
 * values so the dispatch can be told apart from the outside. */
static void MeasureA(BrTextBox *pBox)
{
    s_c04++;
    pBox->width  = (int16_t)(3 * (int)strlen(pBox->sz));
    pBox->height = 16;
}
static void MeasureB(BrTextBox *pBox)
{
    s_c08++;
    pBox->width  = (int16_t)(40 * (int)strlen(pBox->sz));
    pBox->height = 45;
}
static float CentreX(BrTextBox *pBox)
{
    s_c28++;
    pBox->x = (float)pBox->left;
    return pBox->x;
}

static BrTextBoxVtbl s_boxVtbl;

/* A control that has been through BrUiCtlCtor and had its box vtable wired,
 * which is the state every builder hands to f34. */
static BrUiCtl_ *NewCtl(void)
{
    BrUiCtl_ *c = (BrUiCtl_ *)malloc(sizeof(BrUiCtl_));
    if (!c) return NULL;
    memset(c, 0xA5, sizeof(*c));
    BrUiCtlCtor(c);
    c->f2B5C.pVtbl = &s_boxVtbl;
    return c;
}

/* 0x100AB448, read out of orig/BRD3D.dll: { 148, 110, 358, 260 }. Only [0]
 * and [2] are read, but all four are present so an over-read would show. */
static const int32_t s_style[4] = { 148, 110, 358, 260 };

/* ======================================================================== */

static void TestPageCtor(void)
{
    BrUiPage_ *pg = (BrUiPage_ *)malloc(sizeof(BrUiPage_));
    BrUiPage_ *r;
    if (!pg) { printf("alloc failed\n"); return; }

    memset(pg, 0xA5, sizeof(*pg));
    r = BrUiPageCtor_10048470(pg);

    CHECK(r == pg, "page ctor returns this");
    CHECK(pg->pVtbl == g_pBrUiPageVtbl, "stores the 0x1008F6F8 vtable");
    CHECK(pg->pVtbl != NULL, "page vtable pointer is never left NULL");

    CHECK(pg->pfn04 == NULL && pg->pfn08 == NULL && pg->pfn0C == NULL,
          "+0x04/+0x08/+0x0C cleared");
    CHECK(pg->f10 == 0, "+0x10 cleared");
    CHECK(pg->cCtl == 0, "+0x14 cleared");
    CHECK(pg->pOwner == NULL, "+0x340 cleared");
    CHECK(pg->cSel == 0, "+0x344 cleared");
    CHECK(pg->f346 == 0, "+0x346 cleared");

    /* The fill's extent, which is what fixes BR73_PAGE_CTL_MAX at 200. */
    CHECK(pg->apCtl[0] == NULL, "fill covers the first slot");
    CHECK(pg->apCtl[BR73_PAGE_CTL_MAX - 1] == NULL,
          "fill covers the LAST slot (0x18 + 200*4 == 0x338)");
    CHECK(pg->fX == 0.0f && pg->fY == 0.0f,
          "the two floats past the array are cleared too");

    /* This fill writes 0, not -1 -- `xor eax,eax` precedes the rep stosd
     * with nothing touching eax in between. The control constructor next door
     * is the one with -1 fills; confusing the two is the standing hazard. */
    CHECK(pg->apCtl[100] == NULL, "the page fill is a ZERO fill");

    CHECK(BrUiPageCtor_10048470(NULL) == NULL,
          "NULL this returns NULL, does not fault");
    free(pg);
}

static void TestPlace(void)
{
    BrUiCtl_ *c = NewCtl();
    if (!c) { printf("alloc failed\n"); return; }

    /* The constructor's -1 fill reaches +0x2A40; f38 is what replaces it. */
    CHECK(c->f2A40 == (uint16_t)0xFFFFu, "+0x2A40 starts at -1, not 0");
    CHECK(c->f1C == 1, "+0x1C starts at 1 -- f38 ORs into it");

    BrUiCtlPlace_10047FB0(c, (BrPhase_ *)0, 195.0f, 130.0f,
                          0x102001, 2, 5, 7, 0x45);

    CHECK(c->f3C == 195.0f && c->f40 == 130.0f, "+0x3C/+0x40 take x and y");
    CHECK(c->f1C == (1 | 0x102001), "+0x1C is OR-ed, not assigned");
    CHECK(c->f24 == 2 && c->f28 == 5, "+0x24/+0x28 OR in a4 and a5");
    CHECK(c->f2968 == 7, "+0x2968 is assigned");
    CHECK(c->f2A40 == 0x45 && c->f1E20C == 0x45,
          "one code word, TWO destinations");

    /* Second call: three fields accumulate, the rest are overwritten. */
    BrUiCtlPlace_10047FB0(c, (BrPhase_ *)0, 10.0f, 20.0f,
                          0x200000, 8, 0, 9, 0x39);
    CHECK(c->f1C == (1 | 0x102001 | 0x200000), "+0x1C accumulates");
    CHECK(c->f24 == (2 | 8), "+0x24 accumulates");
    CHECK(c->f28 == 5, "+0x28 accumulates (OR with 0 is a no-op)");
    CHECK(c->f2968 == 9, "+0x2968 does NOT accumulate");
    CHECK(c->f2A40 == 0x39, "+0x2A40 does NOT accumulate");
    CHECK(c->f3C == 10.0f && c->f40 == 20.0f, "the floats are assigned");

    BrUiCtlPlace_10047FB0(NULL, NULL, 0, 0, 0, 0, 0, 0, 0);   /* must not fault */
    free(c);
}

static void TestSetText(void)
{
    BrUiCtl_ *c = NewCtl();
    if (!c) { printf("alloc failed\n"); return; }

    s_c04 = s_c08 = s_c28 = 0;

    /* Place first: this is the order every builder uses, and f34 consumes
     * +0x3C / +0x40. The y is deliberately fractional and negative-free so
     * the truncation is visible. */
    BrUiCtlPlace_10047FB0(c, (BrPhase_ *)0, 195.0f, 130.75f,
                          0x102001, 2, 5, 1, (int32_t)-1);
    /* a2 == 2: bit 0 CLEAR, so no +0x28 call. a3 == 1: not 3, so +0x04. */
    BrUiCtlSetText_10047EB0(c, "MAIN MENU", 2, 1, s_style);

    CHECK(strcmp(c->f2B5C.sz, "MAIN MENU") == 0, "text copied with its NUL");
    CHECK(c->f2B5C.f08 == 1, "the kind byte is stored");
    CHECK(c->f2B5C.f04 == 2u, "flags OR-ed into the item");
    CHECK(s_c04 == 1 && s_c08 == 0, "kind != 3 dispatches vtable +0x04");
    CHECK(s_c28 == 0, "bit 0 clear -> no +0x28 call");

    /* The rectangle: x edges from the STYLE, y from +0x40 truncated. A port
     * that used the placement x would give 195 here. */
    CHECK(c->f50 == 148, "+0x50 comes from pStyle[0], NOT from x");
    CHECK(c->f58 == 358, "+0x58 comes from pStyle[2], NOT from x + 0x7F");
    CHECK(c->f54 == 130, "+0x54 is __ftol(+0x40) -- truncated toward zero");
    CHECK(c->f2B5C.left == 148 && c->f2B5C.right == 358,
          "the item gets the same two edges");
    CHECK(c->f2B5C.x == 195.0f && c->f2B5C.y == 130.75f,
          "the item gets the control's +0x3C / +0x40 verbatim");

    /* +0x5C and +0x48/+0x4A are decided by the measuring hook, which ran
     * AFTER the zeroing. MeasureA sets height 16 and width 3*len. */
    CHECK(c->f5C == 130 + 16, "+0x5C is +0x54 plus the MEASURED height");
    CHECK(c->f4A == 16, "+0x4A is the measured height");
    CHECK(c->f48 == (uint16_t)(3 * 9), "+0x48 is the measured width");

    free(c);
}

static void TestSetTextDispatch(void)
{
    BrUiCtl_ *c = NewCtl();
    if (!c) { printf("alloc failed\n"); return; }

    s_c04 = s_c08 = s_c28 = 0;
    BrUiCtlPlace_10047FB0(c, (BrPhase_ *)0, 0.0f, 0.0f, 0, 2, 5, 0, 0);

    /* Seed the item flags with bit 0 ALREADY set, then pass a2 with bit 0
     * CLEAR. The original tests the argument, so +0x28 must NOT run even
     * though the field ends up with bit 0 set. */
    c->f2B5C.f04 = 1u;
    BrUiCtlSetText_10047EB0(c, "12345", 4, 3, s_style);

    CHECK(s_c08 == 1 && s_c04 == 0, "kind == 3 dispatches vtable +0x08");
    CHECK(c->f2B5C.f04 == (1u | 4u), "flags are OR-ed, not assigned");
    CHECK(s_c28 == 0,
          "the +0x28 call follows the ARGUMENT's bit 0, not the field's");
    CHECK(c->f4A == 45 && c->f48 == (uint16_t)(40 * 5),
          "the font-B measurement is the one that landed");

    /* Now the other way: argument bit 0 set. */
    s_c04 = s_c08 = s_c28 = 0;
    BrUiCtlSetText_10047EB0(c, "12345", 1, 3, s_style);
    CHECK(s_c28 == 1, "argument bit 0 set -> +0x28 runs once");

    free(c);
}

static void TestSetTextResets(void)
{
    BrUiCtl_ *c = NewCtl();
    if (!c) { printf("alloc failed\n"); return; }

    BrUiCtlPlace_10047FB0(c, (BrPhase_ *)0, 0.0f, 0.0f, 0, 2, 5, 0, 0);

    /* BrTextBoxMeasureA/B SEED height from its current value and only ever
     * grow it, so 0x10047EB0 zeroing width/height before the dispatch is what
     * keeps a second, shorter string from inheriting the first's metrics.
     * Measure with the tall font, then the short one, and check the short
     * result is what survives. */
    BrUiCtlSetText_10047EB0(c, "12345", 0, 3, s_style);
    CHECK(c->f4A == 45, "tall font first");
    BrUiCtlSetText_10047EB0(c, "ab", 0, 1, s_style);
    CHECK(c->f4A == 16, "width/height are RESET before each measure");
    CHECK(c->f48 == (uint16_t)(3 * 2), "and the width is the new string's");

    /* No text-box vtable: the measure is skipped, the box keeps zeroes, and
     * the control's rectangle still gets the style edges and the truncated y.
     * This is the port's NULL-vtable deviation, pinned so it cannot quietly
     * become something else. */
    c->f2B5C.pVtbl = NULL;
    BrUiCtlPlace_10047FB0(c, (BrPhase_ *)0, 0.0f, 42.9f, 0, 2, 5, 0, 0);
    BrUiCtlSetText_10047EB0(c, "zz", 1, 1, s_style);
    CHECK(c->f54 == 42, "y still truncates with no box vtable");
    CHECK(c->f5C == 42, "and +0x5C is +0x54 plus a height of zero");
    CHECK(c->f2B5C.width == 0 && c->f2B5C.height == 0,
          "an unmeasured box reads as zero, not as the previous measurement");

    BrUiCtlSetText_10047EB0(NULL, "x", 0, 0, s_style);   /* must not fault */
    BrUiCtlSetText_10047EB0(c, NULL, 0, 0, s_style);     /* must not fault */
    free(c);
}

static void TestTextRoom(void)
{
    BrUiCtl_ *c = NewCtl();
    char *pszLong;
    size_t i;
    if (!c) { printf("alloc failed\n"); return; }

    /* DEVIATION under test: the original's `rep movs` is unbounded and would
     * run past the item block into the control's own fields. The port
     * truncates. The assertion is that the buffer stays terminated and the
     * field immediately after it is untouched. */
    pszLong = (char *)malloc(BR73_ITEM_TEXT_ROOM + 64u);
    if (!pszLong) { free(c); printf("alloc failed\n"); return; }
    for (i = 0; i < BR73_ITEM_TEXT_ROOM + 63u; ++i) pszLong[i] = 'x';
    pszLong[BR73_ITEM_TEXT_ROOM + 63u] = '\0';

    c->f2B5C.f420 = 0x5A5A5A5A;
    BrUiCtlPlace_10047FB0(c, (BrPhase_ *)0, 0.0f, 0.0f, 0, 2, 5, 0, 0);
    BrUiCtlSetText_10047EB0(c, pszLong, 0, 1, s_style);

    CHECK(strlen(c->f2B5C.sz) == (size_t)BR73_ITEM_TEXT_ROOM - 1u,
          "an over-long string is truncated to the buffer");
    CHECK(c->f2B5C.f420 == 0, "the copy did not run into the next field");

    free(pszLong);
    free(c);
}

int main(void)
{
    s_boxVtbl.pfn04 = MeasureA;
    s_boxVtbl.pfn08 = MeasureB;
    s_boxVtbl.pfn28 = CentreX;

    TestPageCtor();
    TestPlace();
    TestSetText();
    TestSetTextDispatch();
    TestSetTextResets();
    TestTextRoom();

    printf("uivt: %d checks, %d failures\n", g_checks, g_fails);
    return g_fails ? 1 : 0;
}
