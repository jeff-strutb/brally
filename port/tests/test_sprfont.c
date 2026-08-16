/* test_sprfont.c -- the menu's sprite font.
 *
 * What is asserted, and why each is a property of the code rather than a
 * restatement of it:
 *
 *  - The rectangle tables' GRID, not their contents. 0x1005F800's four loops
 *    are "cell (i%cols, i/cols) of a cw x ch grid", so the invariants are
 *    that every cell is exactly cw x ch, that column 0 starts at x == 0, and
 *    that the last cell of each table ends where the disassembly's
 *    terminating address says it does. A table read at the wrong stride, or
 *    a loop whose count is off by one, breaks one of those.
 *
 *  - THE TABLES MEET THE METRICS. The largest `sprite` in slice3_39.c's font
 *    A metric table must be the last index table A has. That is the check
 *    that pins 68 from the other end: the two objects were derived from
 *    different parts of the binary (a data table versus a loop bound) and
 *    they have to agree or one of them is wrong.
 *
 *  - The kind -> sheet map lands on the four type_*.bmp entries of the sprite
 *    table BY NAME, so a shifted sprite table would be caught here even
 *    though this module never sees the names.
 *
 *  - The fall-through GOTCHA, asserted as behaviour: kind 3 selects sprite 0.
 *
 *  - The walk's advances against the metric table, and the space's 6, which
 *    the original spells as a subtraction of -6.0f.
 *
 *  - ORIENTATION, with real pixels. A vertical flip is invisible on 'H', 'I',
 *    'O' and every other symmetric capital, and a full flip has shipped in
 *    this tree before because "MAIN MENU" looked fine upside down. 'L' has
 *    all its horizontal ink at the BOTTOM and 'F' at the TOP, so the pair
 *    distinguishes the two orientations no matter what else is wrong. Needs
 *    the extracted art; SKIPs without it.
 */
#include "br_sprfont.h"
#include "br_uispr.h"
#include "br_bmp.h"
#include "br_uinav.h"   /* only for BrUiNav, so g_pBrUiNav can be defined */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The rest of slice3_39.o's world. Linked only for g_BrGlyphFontA; none of
 * these is exercised, exactly as test_uispr.c does it. */
void BrTextBoxDtor(BrTextBox *pBox);
void BrTextBoxDtor(BrTextBox *pBox) { (void)pBox; }
int32_t BrDikGetDeviceState(uint8_t *pState);
int32_t BrDikGetDeviceState(uint8_t *pState) { (void)pState; return 0; }
char g_aBr39B720[0x104];

/* br_uinav.o is NOT linked: this suite reaches none of BrSprFontKindHook's
 * callers, and the one global it needs is the nav pointer, which the module
 * only dereferences after a NULL test. Defining it here keeps the suite's
 * link closure to br_sprfont + slice3_39 + br_uispr + br_crt. */
BrUiNav *g_pBrUiNav;

static int g_checks, g_fails;
#define CHECK(cond, msg) do { g_checks++; if (!(cond)) { \
    g_fails++; printf("  [FAIL] %s (%s:%d)\n", (msg), __FILE__, __LINE__); } } while (0)

/* ---------------------------------------------------------------------- */

static int GridOk(const int32_t (*pTab)[4], int n, int cols, int cw, int ch)
{
    int i;

    for (i = 0; i < n; i++) {
        if (pTab[i][2] - pTab[i][0] != cw) return 0;
        if (pTab[i][3] - pTab[i][1] != ch) return 0;
        if (pTab[i][0] != (i % cols) * cw) return 0;
        if (pTab[i][1] != (i / cols) * ch) return 0;
    }
    return 1;
}

static void TestRectTables(void)
{
    BrSprFontRectInit_1005F800();

    CHECK(GridOk(g_aBrSprRectA, BR_SPRFONT_RECT_A, 8, 16, 16),
          "table A is an 8-wide grid of 16x16 cells");
    CHECK(GridOk(g_aBrSprRectB, BR_SPRFONT_RECT_B, 5, 39, 44),
          "table B is a 5-wide grid of 39x44 cells");
    CHECK(GridOk(g_aBrSprRectC, BR_SPRFONT_RECT_C, 5, 128, 128),
          "table C is a 5-wide grid of 128x128 cells");
    CHECK(GridOk(g_aBrSprRectD, BR_SPRFONT_RECT_D, 3, 128, 128),
          "table D is a 3-wide grid of 128x128 cells");

    /* The four terminating addresses, restated as counts. Each is a literal
     * compared against the running pointer in 0x10058540. */
    CHECK(0x10AC4208u + (unsigned)BR_SPRFONT_RECT_A * 16u == 0x10AC4648u,
          "table A ends at the address its loop compares against");
    CHECK(0x10AC46C0u + (unsigned)BR_SPRFONT_RECT_B * 16u == 0x10AC4800u,
          "table B ends at 0x10AC4800");
    CHECK(0x10AC4AD8u + (unsigned)BR_SPRFONT_RECT_C * 16u == 0x10AC4BC8u,
          "table C ends exactly where table D begins");
    CHECK(0x10AC4BC8u + (unsigned)BR_SPRFONT_RECT_D * 16u == 0x10AC4C58u,
          "table D ends at 0x10AC4C58");

    /* Idempotent: the original runs it once, but nothing stops a second run
     * and a second run must not shift anything. */
    {
        int32_t save[4];
        memcpy(save, g_aBrSprRectA[67], sizeof save);
        BrSprFontRectInit_1005F800();
        CHECK(memcmp(save, g_aBrSprRectA[67], sizeof save) == 0,
              "a second init leaves the tables where they were");
    }
}

/* The two objects that have to agree, derived from different places. */
static void TestTablesMeetMetrics(void)
{
    int i, maxA = -1;
    int fSheetFits = 1;

    for (i = 0; i < BR_GLYPH_COUNT; i++) {
        if (g_BrGlyphFontA[i].sprite != BR_GLYPH_NONE &&
            (int)g_BrGlyphFontA[i].sprite > maxA) {
            maxA = (int)g_BrGlyphFontA[i].sprite;
        }
    }
    CHECK(maxA == BR_SPRFONT_RECT_A - 1,
          "the metric table's largest glyph index is table A's last entry");

    /* And every cell of A has to be inside the 128x144 sheet the sprite table
     * says those four images are. */
    for (i = 0; i < BR_SPRFONT_RECT_A; i++) {
        if (g_aBrSprRectA[i][2] > g_aBrUiSprite[3].rect[2] ||
            g_aBrSprRectA[i][3] > g_aBrUiSprite[3].rect[3]) {
            fSheetFits = 0;
        }
    }
    CHECK(fSheetFits, "every glyph cell fits inside type_wit.bmp's 128x144");

    /* B's ten real digits fit bignums.bmp; entries 10..19 do NOT, and that is
     * the original's, not a transcription slip. */
    CHECK(g_aBrSprRectB[9][2] <= g_aBrUiSprite[5].rect[2] &&
          g_aBrSprRectB[9][3] <= g_aBrUiSprite[5].rect[3],
          "the ten digit cells fit bignums.bmp");
    CHECK(g_aBrSprRectB[10][3] > g_aBrUiSprite[5].rect[3],
          "cells 10..19 of table B address rows bignums.bmp does not have");
}

static void TestSheetMap(void)
{
    CHECK(BrSprFontSheet_1005B730(0) == 2,  "kind 0 -> sprite 2");
    CHECK(BrSprFontSheet_1005B730(1) == 3,  "kind 1 -> sprite 3");
    CHECK(BrSprFontSheet_1005B730(2) == 4,  "kind 2 -> sprite 4");
    CHECK(BrSprFontSheet_1005B730(4) == 0x34, "kind 4 -> sprite 0x34");
    /* The GOTCHA: there is no default arm, so anything else is sprite 0. */
    CHECK(BrSprFontSheet_1005B730(3) == 0, "kind 3 falls through to sprite 0");
    CHECK(BrSprFontSheet_1005B730(200) == 0, "so does any other kind");

    /* The four answers must be the four font sheets, by name. */
    CHECK(strcmp(g_aBrUiSpriteName[2], "type_gry.bmp") == 0 &&
          strcmp(g_aBrUiSpriteName[3], "type_wit.bmp") == 0 &&
          strcmp(g_aBrUiSpriteName[4], "type_mid.bmp") == 0 &&
          strcmp(g_aBrUiSpriteName[0x34], "type_yel.bmp") == 0,
          "the four sheets the map selects are the four type_ bitmaps");
    CHECK(strcmp(g_aBrUiSpriteName[BR_SPRFONT_SHEET_B], "bignums.bmp") == 0,
          "font B's fixed sheet is bignums.bmp");
}

/* ---------------------------------------------------------------------- */

/* A recording blit, so the walk can be checked without any art. */
#define REC_MAX 64
typedef struct Rec {
    int      n;
    int32_t  x[REC_MAX], y[REC_MAX], iSpr[REC_MAX], fBlit[REC_MAX];
    int32_t  rc[REC_MAX][4];
} Rec;

static void RecBlit(void *pCtx, int32_t x, int32_t y, int32_t iSprite,
                    const int32_t *pRect, int32_t fBlit)
{
    Rec *p = (Rec *)pCtx;
    if (p->n >= REC_MAX) return;
    p->x[p->n] = x; p->y[p->n] = y;
    p->iSpr[p->n] = iSprite; p->fBlit[p->n] = fBlit;
    memcpy(p->rc[p->n], pRect, sizeof p->rc[0]);
    p->n++;
}

static void BoxInit(BrTextBox *pBox, const char *psz, uint8_t kind,
                    float x, float y)
{
    memset(pBox, 0, sizeof *pBox);
    strcpy(pBox->sz, psz);
    pBox->f08 = kind;
    pBox->x = x;
    pBox->y = y;
    /* f04 bit 0 clear, so the walk takes +0x410 and never dispatches through
     * the NULL vtable. The centring arm is the harness's business. */
}

static void TestWalk(void)
{
    BrTextBox box;
    Rec       rec;
    int       i;
    float     end;

    /* One glyph per drawable character, at the box's own y, with x advancing
     * by the metric table's `advance`. */
    memset(&rec, 0, sizeof rec);
    BoxInit(&box, "AB", 1, 100.0f, 40.0f);
    end = BrSprFontDraw_1005B2B0(&box, RecBlit, &rec);

    CHECK(rec.n == 2, "two characters produce two glyphs");
    if (rec.n == 2) {
        CHECK(rec.y[0] == 40 && rec.y[1] == 40,
              "every glyph gets the box's y unchanged -- no baseline");
        CHECK(rec.x[0] == 100, "the first glyph starts at the box's x");
        CHECK(rec.x[1] == 100 + (int32_t)g_BrGlyphFontA['A' - 0x20].advance,
              "the second is one 'A' advance along");
        CHECK(rec.iSpr[0] == 3 && rec.iSpr[1] == 3,
              "kind 1 blits out of sprite 3");
        CHECK(rec.fBlit[0] == g_aBrUiSprite[3].fBlit,
              "the blit flag is the sheet's own table entry, not the file's");
        CHECK(memcmp(rec.rc[0], g_aBrSprRectA[g_BrGlyphFontA['A' - 0x20].sprite],
                     sizeof rec.rc[0]) == 0,
              "the source rect is the metric's glyph cell");
    }
    CHECK((int32_t)end == 100 + (int32_t)g_BrGlyphFontA['A' - 0x20].advance
                              + (int32_t)g_BrGlyphFontA['B' - 0x20].advance,
          "the returned pen is the sum of the two advances");

    /* A space has no glyph and advances by 6 -- the original subtracts the
     * -6.0f at 0x10077674. */
    memset(&rec, 0, sizeof rec);
    BoxInit(&box, " A", 1, 0.0f, 0.0f);
    (void)BrSprFontDraw_1005B2B0(&box, RecBlit, &rec);
    CHECK(rec.n == 1, "the space draws nothing");
    if (rec.n == 1) {
        CHECK(rec.x[0] == BR_GLYPH_SPACE_ADVANCE,
              "the space still moves the pen by 6");
    }

    /* Any byte below 0x20 or at/above 0x80 stops the walk, and what came
     * before it is still drawn. */
    memset(&rec, 0, sizeof rec);
    BoxInit(&box, "A\tB", 1, 0.0f, 0.0f);
    (void)BrSprFontDraw_1005B2B0(&box, RecBlit, &rec);
    CHECK(rec.n == 1, "a control character stops the walk where it stands");

    /* The kind byte, and only the kind byte, chooses the sheet. */
    for (i = 0; i < 4; i++) {
        static const uint8_t aKind[4]  = { 0, 1, 2, 4 };
        static const int32_t aSheet[4] = { 2, 3, 4, 0x34 };
        memset(&rec, 0, sizeof rec);
        BoxInit(&box, "A", aKind[i], 0.0f, 0.0f);
        (void)BrSprFontDraw_1005B2B0(&box, RecBlit, &rec);
        CHECK(rec.n == 1 && rec.iSpr[0] == aSheet[i],
              "the same string on a different kind changes only the sheet");
    }

    /* Lower case shares the upper case's glyph -- the sheet has no minuscules
     * and the metric table says so. Same cell, different advance is what a
     * mis-shifted table would produce, so both are checked. */
    CHECK(g_BrGlyphFontA['a' - 0x20].sprite == g_BrGlyphFontA['A' - 0x20].sprite
       && g_BrGlyphFontA['a' - 0x20].advance == g_BrGlyphFontA['A' - 0x20].advance,
          "'a' and 'A' are the same glyph at the same advance");
}

/* ---------------------------------------------------------------------- */

/* ORIENTATION. Rasterise one glyph out of the real sheet and weigh its ink
 * top against bottom.
 *
 * 'L' is bottom-heavy and 'F' is top-heavy, in every typeface there is. If
 * br_bmp.c's bottom-up handling or the cell arithmetic flipped either the
 * sheet or the cell, the two would swap -- and no symmetric capital would
 * show it. */
typedef struct Ink { const BrBmp *pSheet; long top, bottom; } Ink;

static void InkBlit(void *pCtx, int32_t x, int32_t y, int32_t iSprite,
                    const int32_t *pRect, int32_t fBlit)
{
    Ink *p = (Ink *)pCtx;
    int32_t w = pRect[2] - pRect[0], h = pRect[3] - pRect[1], i, j;
    (void)x; (void)y; (void)iSprite; (void)fBlit;

    for (j = 0; j < h; j++) {
        for (i = 0; i < w; i++) {
            const uint8_t *pS = p->pSheet->pRgba +
                ((size_t)(pRect[1] + j) * p->pSheet->w + pRect[0] + i) * 4u;
            if (pS[3] == 0) continue;           /* the colour key */
            if (j < h / 2) p->top++; else p->bottom++;
        }
    }
}

static long InkFor(const BrBmp *pSheet, char c, long *pTop, long *pBottom,
                   char *pszArt, size_t cbArt)
{
    BrTextBox box;
    Ink       ink;
    const int32_t *pRect;
    int32_t   w, h, i, j;
    size_t    k = 0;

    ink.pSheet = pSheet; ink.top = 0; ink.bottom = 0;
    BoxInit(&box, (char[]){ c, 0 }, 1, 0.0f, 0.0f);
    (void)BrSprFontDraw_1005B2B0(&box, InkBlit, &ink);
    *pTop = ink.top; *pBottom = ink.bottom;

    /* ASCII art, so a human reading the log can see the glyph the way the
     * screenshot shows it. */
    pRect = g_aBrSprRectA[g_BrGlyphFontA[(unsigned char)c - 0x20].sprite];
    w = pRect[2] - pRect[0]; h = pRect[3] - pRect[1];
    for (j = 0; j < h && k + (size_t)w + 2 < cbArt; j++) {
        for (i = 0; i < w; i++) {
            const uint8_t *pS = pSheet->pRgba +
                ((size_t)(pRect[1] + j) * pSheet->w + pRect[0] + i) * 4u;
            pszArt[k++] = pS[3] ? '#' : '.';
        }
        pszArt[k++] = '\n';
    }
    pszArt[k] = '\0';
    return ink.top + ink.bottom;
}

static int TestOrientation(void)
{
    BrBmp sheet;
    long  lt, lb, ft, fb;
    static char szL[1024], szF[1024];

    memset(&sheet, 0, sizeof sheet);
    if (BrBmpLoad(&sheet, "testdata/images/type_wit.bmp") != 0) {
        return 0;
    }
    BrBmpApplyKey(&sheet, BR_UI_COLOUR_KEY);

    CHECK(sheet.w == (uint32_t)g_aBrUiSprite[3].rect[2] &&
          sheet.h == (uint32_t)g_aBrUiSprite[3].rect[3],
          "the file's own size is the size the sprite table claims");

    (void)InkFor(&sheet, 'L', &lt, &lb, szL, sizeof szL);
    (void)InkFor(&sheet, 'F', &ft, &fb, szF, sizeof szF);

    printf("  'L'  top=%ld bottom=%ld\n%s", lt, lb, szL);
    printf("  'F'  top=%ld bottom=%ld\n%s", ft, fb, szF);

    CHECK(lt > 0 && lb > 0 && ft > 0 && fb > 0,
          "both glyphs have ink in both halves");
    CHECK(lb > lt, "'L' is bottom-heavy -- its foot is at the BOTTOM");
    CHECK(ft > fb, "'F' is top-heavy -- its bars are at the TOP");
    /* The pair, restated as the thing a flip would break: L and F disagree
     * about which half is heavier. A vertical flip swaps both verdicts. */
    CHECK((lb > lt) != (fb > ft),
          "L and F weigh opposite ways, which is what a flip would invert");

    BrBmpFree(&sheet);
    return 1;
}

int main(void)
{
    int fArt;

    TestRectTables();
    TestTablesMeetMetrics();
    TestSheetMap();
    TestWalk();
    fArt = TestOrientation();

    if (!fArt) {
        printf("  (orientation check skipped: testdata/images/type_wit.bmp "
               "is not extracted)\n");
    }
    printf("sprfont: %d checks, %d failures\n", g_checks, g_fails);
    return g_fails ? 1 : 0;
}
