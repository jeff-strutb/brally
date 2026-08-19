/* test_uispr.c -- the UI sprite table and the control chrome dispatch.
 *
 * What is asserted, and why each one is a property of the code rather than a
 * transcription of it:
 *
 *  - NOT "the table has 145 entries with these numbers in them". That is the
 *    table, restated. What IS asserted is the two pieces of ARITHMETIC that
 *    pin the extent -- the style pool ends exactly where the table begins,
 *    and 145 entries end exactly where the loader's 0x122-dword clear ends --
 *    plus the invariant every entry satisfies (iImage == index, origin at
 *    (0,0)), which a mis-transcribed or mis-shifted row would break.
 *
 *  - The button PAIRS. 82/83, 84/85 and 120/121 are what BrExt_10054B50
 *    places, and the claim being tested is that each pair is the same size
 *    and that size is the +0x7F / +0x21 rectangle the builder computes. If
 *    the table were read at the wrong stride or the wrong base, the two
 *    halves of a pair would not agree and 127x33 would not fall out.
 *
 *  - The dispatch's ARMS, each reached on purpose: no flags28 bit 0, the
 *    0x100000 label arm, the 0x200000 silent arm, the sentinel index, and the
 *    ordinary sprite arm. The label arm is the one the old harness got wrong,
 *    so it gets its own case.
 *
 *  - The selection swap, driven the way the frame drives it: write aStepId[1]
 *    into w1E20C and the resolved sprite must change AND fDown must come up.
 *
 *  - The clip's boundary conditions, in the original's own terms: it clips
 *    right and bottom only, its compares are unsigned, and a zero-width rect
 *    survives (only a NEGATIVE one is rejected).
 *
 *  - The aliased-storage check CONVENTIONS.md asks for: slice3_39.c's
 *    g_BrSprRect46 / g_BrSprRect48 are entries 46 and 48 of this table, and
 *    the two host objects must still agree.
 */
#include "br_uispr.h"
#include "slice3_39.h"      /* g_BrSprRect46 / g_BrSprRect48 -- the alias */
#include "br_uictl.h"       /* BrUiCtlCtor, so a control starts as it does
                             * in the game rather than as a memset */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* slice3_39.o is linked in only for its two rectangle objects (see
 * TestAlias). These three externs are the rest of that object's world; every
 * one is a stand-in, none is exercised, and test_slice3_39.c supplies its own
 * copies of the same three for the same reason. */
void BrTextBoxDtor(BrTextBox *pBox);
void BrTextBoxDtor(BrTextBox *pBox) { (void)pBox; }
int32_t BrDikGetDeviceState(uint8_t *pState);
int32_t BrDikGetDeviceState(uint8_t *pState) { (void)pState; return 0; }
char g_aBr39B720[0x104];

static int g_checks, g_fails;
#define CHECK(cond, msg) do { g_checks++; if (!(cond)) { \
    g_fails++; printf("  [FAIL] %s (%s:%d)\n", (msg), __FILE__, __LINE__); } } while (0)

/* ---------------------------------------------------------------------- */

static void TestExtent(void)
{
    int i;
    int fShape = 1;

    /* The lower bound: slice3_39.h's 21-entry style pool ends where this
     * table starts. Both numbers come from separate derivations. */
    CHECK(BR_UI_STYLE_BASE + (unsigned)BR_UI_STYLE_COUNT * 16u
          == BR_UI_SPR_BASE,
          "the style pool ends exactly at the sprite table's base");

    /* The upper bound: the loader clears 0x122 dwords of a stride-8 table,
     * i.e. 0x122/2 == 145 entries. */
    CHECK(0x122u / 2u == (unsigned)BR_UI_SPR_COUNT,
          "the loader's rep stosd count gives 145 images");

    /* Address indexing must land on the entry the disassembly names. */
    CHECK(BR_UI_SPR(BR_UI_SPR_BASE) == &g_aBrUiSprite[0],
          "BR_UI_SPR of the base is entry 0");
    CHECK(BR_UI_SPR(0x100AB9B8) == &g_aBrUiSprite[46],
          "0x100AB9B8 is entry 46");

    /* The invariant that a shifted or mis-strided read would break. */
    for (i = 0; i < BR_UI_SPR_COUNT; ++i) {
        if (g_aBrUiSprite[i].iImage != i ||
            g_aBrUiSprite[i].rect[0] != 0 || g_aBrUiSprite[i].rect[1] != 0 ||
            g_aBrUiSprite[i].rect[2] <= 0 || g_aBrUiSprite[i].rect[3] <= 0 ||
            (g_aBrUiSprite[i].fBlit & ~1) != 0) {
            fShape = 0;
        }
    }
    CHECK(fShape, "every entry is (index, (0,0)-(w,h), 0 or 1)");

    CHECK(BrUiSpriteAt(-1) == NULL && BrUiSpriteAt(BR_UI_SPR_COUNT) == NULL &&
          BrUiSpriteAt(0) == &g_aBrUiSprite[0],
          "BrUiSpriteAt rejects the sentinel and the far end");
}

/* The three button pairs BrExt_10054B50 places, and the rectangle its own
 * arithmetic produces for them. */
static void TestButtonPairs(void)
{
    static const int aPair[3][2] = { { 0x52, 0x53 }, { 0x54, 0x55 },
                                     { 0x78, 0x79 } };
    int p;

    for (p = 0; p < 3; ++p) {
        const BrUiSprite *pUp   = BrUiSpriteAt(aPair[p][0]);
        const BrUiSprite *pDown = BrUiSpriteAt(aPair[p][1]);

        CHECK(pUp != NULL && pDown != NULL, "both halves of the pair exist");
        if (pUp == NULL || pDown == NULL) {
            continue;
        }
        CHECK(pUp->rect[2] == pDown->rect[2] &&
              pUp->rect[3] == pDown->rect[3],
              "a button and its down variant are the same size");
        /* 0x7F and 0x21 are the literals the builder adds to rcLeft/rcTop. */
        CHECK(pUp->rect[2] - pUp->rect[0] == 0x7F &&
              pUp->rect[3] - pUp->rect[1] == 0x21,
              "the button art is exactly the +0x7F / +0x21 rectangle");
    }
}

/* ---------------------------------------------------------------------- */

static BrUiCtl_ *NewCtl(void)
{
    BrUiCtl_ *c = (BrUiCtl_ *)calloc(1, (size_t)BR_UI_CTL_ALLOC_SIZE);

    if (c != NULL) {
        BrUiCtlCtor(c);
    }
    return c;
}

static void TestDispatchArms(void)
{
    BrUiCtl_  *c = NewCtl();
    BrUiChrome ch;

    if (c == NULL) {
        return;
    }

    /* The constructor leaves aStepId[] at 0xFFFF and w1E20C untouched; give
     * the control the shape a placed rectangle has. */
    c->flags28 = 5;                  /* every builder's a5 */
    c->flags1C = 0x402001;           /* the rectangle rows' flags */
    c->x = 100.9f;
    c->y = 200.9f;
    c->aStepId[0] = 0x52;
    c->aStepId[1] = 0x53;
    c->w1E20C     = 0x52;            /* what 0x10048180 writes when idle */

    CHECK(BrUiCtlChrome(c, 640, 480, &ch) != 0 &&
          ch.kind == BR_UI_CHROME_SPRITE,
          "an ordinary rectangle row resolves to a sprite");
    CHECK(ch.iSprite == 0x52 && ch.iImage == 0x52,
          "the sprite is the one w1E20C names");
    CHECK(ch.w == 0x7F && ch.h == 0x21, "and it is 127x33");
    /* __ftol truncates toward zero, so 200.9 is 200 and not 201. */
    CHECK(ch.x == 100 && ch.y == 200, "the position is the control's, truncated");
    CHECK(ch.fDown == 0, "the idle sprite is not the down sprite");

    /* The frame's selection swap. */
    c->w1E20C = 0x53;
    CHECK(BrUiCtlChrome(c, 640, 480, &ch) != 0 && ch.iSprite == 0x53 &&
          ch.fDown != 0,
          "aStepId[1] resolves to the down art and is reported as such");

    /* The label arm -- flags1C bit 0x100000. This is the arm that must NOT
     * produce a background: the old harness drew one here. */
    c->flags1C = 0x102001;
    c->w1E20C  = 3;                  /* the builders really do set this */
    CHECK(BrUiCtlChrome(c, 640, 480, &ch) != 0 &&
          ch.kind == BR_UI_CHROME_TEXT,
          "a label draws its text and no sprite");

    /* The silent arm. */
    c->flags1C = 0x202001;
    CHECK(BrUiCtlChrome(c, 640, 480, &ch) == 0 &&
          ch.kind == BR_UI_CHROME_NONE,
          "0x200000 draws nothing at all");

    /* flags28 bit 0 clear gates the whole dispatch. */
    c->flags1C = 0x402001;
    c->flags28 = 4;
    c->w1E20C  = 0x52;
    CHECK(BrUiCtlChrome(c, 640, 480, &ch) == 0,
          "no flags28 bit 0, no drawing");
    c->flags28 = 5;

    /* The sentinel: a builder that passed a7 = -1 and nothing overrode it. */
    c->w1E20C = 0xFFFFu;
    CHECK(BrUiCtlChrome(c, 640, 480, &ch) == 0,
          "0xFFFF is signed -1 and draws nothing");
    CHECK(BrUiCtlSpriteUp(c) == 0x52 && BrUiCtlSpriteDown(c) == 0x53,
          "the pair accessors sign-extend");

    free(c);
}

/* ---------------------------------------------------------------------- */

static void TestClip(void)
{
    int32_t rc[4] = { 0, 0, 100, 50 };
    int32_t w = -1, h = -1;

    CHECK(BrUiSprClip(0, 0, rc, 640, 480, &w, &h) && w == 100 && h == 50,
          "a sprite well inside the surface is not clipped");

    CHECK(BrUiSprClip(600, 0, rc, 640, 480, &w, &h) && w == 40 && h == 50,
          "the right edge is clipped to the surface");
    CHECK(BrUiSprClip(0, 460, rc, 640, 480, &w, &h) && w == 100 && h == 20,
          "the bottom edge is clipped to the surface");

    CHECK(BrUiSprClip(700, 0, rc, 640, 480, &w, &h) == 0,
          "a sprite past the right edge draws nothing");

    /* One-sided, and unsigned. Two consequences, both the original's and
     * neither an improvement:
     *
     *   a sprite STRADDLING the left edge is not clipped at all -- x+w is
     *   still a small positive, so the compare passes and the blit runs off
     *   the left of the surface;
     *
     *   a sprite entirely off the left has a NEGATIVE x+w, which as an
     *   unsigned compares above cx, so it takes the clip arm, and there
     *   cx < (unsigned)x is true and it draws nothing. */
    CHECK(BrUiSprClip(-10, 0, rc, 640, 480, &w, &h) && w == 100,
          "a sprite straddling the left edge is NOT clamped");
    CHECK(BrUiSprClip(-200, 0, rc, 640, 480, &w, &h) == 0,
          "a sprite entirely off the left draws nothing");

    /* Only a NEGATIVE extent is rejected. Six style-shaped entries in the
     * corpus are degenerate, so the difference is not academic. */
    rc[2] = 0;
    CHECK(BrUiSprClip(0, 0, rc, 640, 480, &w, &h) && w == 0,
          "a zero-width rectangle survives the test");
    rc[2] = -1;
    CHECK(BrUiSprClip(0, 0, rc, 640, 480, &w, &h) == 0,
          "a negative-width rectangle does not");

    /* No surface given: the clip is skipped, not applied with cx == 0. */
    rc[2] = 100;
    CHECK(BrUiSprClip(600, 460, rc, 0, 0, &w, &h) && w == 100 && h == 50,
          "cx/cy of 0 mean 'no surface known' and skip the clip");
}

/* ---------------------------------------------------------------------- */

/* CONVENTIONS.md's aliased-storage rule: one original address, two host
 * objects. slice3_39.c owns writable copies of entries 46 and 48's
 * rectangles; nothing writes either side today, so they must still agree.
 * When one of them changes, this is what says so. */
static void TestAlias(void)
{
    CHECK((unsigned)BR_UI_SPR_BASE + 46u * BR_UI_SPR_STRIDE + 4u == 0x100AB9BCu,
          "g_BrSprRect46's address is entry 46's rect");
    CHECK((unsigned)BR_UI_SPR_BASE + 48u * BR_UI_SPR_STRIDE + 4u == 0x100AB9ECu,
          "g_BrSprRect48's address is entry 48's rect");
    CHECK(memcmp(g_BrSprRect46, g_aBrUiSprite[46].rect, sizeof g_BrSprRect46)
          == 0,
          "slice3_39's copy of entry 46 has not drifted");
    CHECK(memcmp(g_BrSprRect48, g_aBrUiSprite[48].rect, sizeof g_BrSprRect48)
          == 0,
          "slice3_39's copy of entry 48 has not drifted");
}

/* The four names the rest of the tree depends on. Each is corroborated by
 * something the pairing did not see, so asserting them is not asserting the
 * pairing back to itself:
 *   2/3/4/52 are the sheets 0x1005B730 maps its kind byte onto, and all four
 *   must be the same size for one glyph rect table to index any of them.
 *   5 is the literal 0x1005B7A0 pushes. */
static void TestFontSheets(void)
{
    static const int aSheet[4] = { 2, 3, 4, 0x34 };
    int i, fSame = 1;

    for (i = 0; i < 4; ++i) {
        const BrUiSprite *p = BrUiSpriteAt(aSheet[i]);
        const BrUiSprite *p0 = BrUiSpriteAt(aSheet[0]);

        if (p == NULL || p0 == NULL ||
            p->rect[2] != p0->rect[2] || p->rect[3] != p0->rect[3] ||
            (p->fBlit & 1) == 0) {
            fSame = 0;
        }
    }
    CHECK(fSame,
          "the four kind-byte font sheets are one size and all colour-keyed");
    CHECK(g_aBrUiSpriteName[2] != NULL &&
          strstr(g_aBrUiSpriteName[2], "type_") != NULL,
          "sheet 2 is a type_ bitmap");
    CHECK(g_aBrUiSpriteName[0x34] != NULL &&
          strstr(g_aBrUiSpriteName[0x34], "type_") != NULL,
          "sheet 0x34 is a type_ bitmap");
    CHECK(g_aBrUiSpriteName[5] != NULL &&
          strcmp(g_aBrUiSpriteName[5], "bignums.bmp") == 0,
          "the literal 5 the font-B glyph draw pushes is the digit strip");
}

int main(void)
{
    TestExtent();
    TestButtonPairs();
    TestDispatchArms();
    TestClip();
    TestAlias();
    TestFontSheets();

    printf("uispr: %d checks, %d failures\n", g_checks, g_fails);
    return g_fails ? 1 : 0;
}
