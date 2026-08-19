/* test_br_surf.c -- the game's 16-bit surface and its 24bpp conversion.
 *
 * Everything here is a property of the ORIGINAL's arithmetic, checked against
 * a synthetic BITMAP rather than against a file, so the suite runs with no
 * extracted assets.  The one assertion that needs real art -- that but-sav.bmp
 * is 127x33 -- belongs to test_br_bmp.c and stays there.
 *
 * The properties, and what a wrong transcription would do to each:
 *
 *   - The two ways the original states the colour key must agree.  0x10001290
 *     stores 0x07E0 as a literal; 0x100583C0 recomputes it from COLORREF
 *     0x0000FF00 through 0x100014A0.  A mis-transcribed packer disagrees with
 *     the literal, and that disagreement is visible without any art.
 *   - The 565 packer's masks are asymmetric (0xF8 red, 0xFC green, >>3 blue).
 *     Making them uniform gives green five bits, which loses the key.
 *   - The walk starts at the LAST stored row and moves backwards.  Flipping
 *     it turns every sprite upside down; a synthetic gradient shows it
 *     without needing a glyph.
 *   - The gate refuses everything but 24bpp, which is the original's whole
 *     format support and the reason the port is allowed to refuse 8bpp.
 *   - The widening has to be invertible ON THE KEY.  This is the assertion
 *     that would have caught a shift-instead-of-replicate expansion, which
 *     silently turns every font sheet opaque.
 */
#include "br_surf.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_checks, g_fails;
#define CHECK(c, m) do { g_checks++; if (!(c)) { g_fails++; \
    printf("  [FAIL] %s\n", (m)); } } while (0)

/* Build a BITMAP over a caller-owned bottom-up 24bpp buffer, the way
 * GetObjectA would describe a DIB section: DWORD-aligned rows, pBits at the
 * first stored row. */
static void MakeBitmap(BrGdiBitmap *pbm, const uint8_t *pBits,
                       int32_t cx, int32_t cy, uint16_t bpp)
{
    memset(pbm, 0, sizeof *pbm);
    pbm->cx           = cx;
    pbm->cy           = cy;
    pbm->cbWidthBytes = (int32_t)((((size_t)cx * bpp + 31u) / 32u) * 4u);
    pbm->cPlanes      = 1;
    pbm->cBitsPixel   = bpp;
    pbm->pBits        = pBits;
}

static void TestKey(void)
{
    BrSurf s;
    memset(&s, 0, sizeof s);

    /* RGB(0,255,0) is COLORREF 0x0000FF00 -- red in the LOW byte. */
    BrSurfSetColourKey(&s, 0x0000FF00u);
    CHECK(s.key == BR_SURF_KEY_565,
          "0x100014A0(RGB(0,255,0)) == the 0x07E0 that 0x10001308 stores");

    /* The packer must not be colour-blind: the three channels have to land in
     * different fields, or the agreement above would be a coincidence. */
    BrSurfSetColourKey(&s, 0x000000FFu);          /* RGB(255,0,0) */
    CHECK(s.key == 0xF800u, "COLORREF red packs to the top five bits");
    BrSurfSetColourKey(&s, 0x00FF0000u);          /* RGB(0,0,255) */
    CHECK(s.key == 0x001Fu, "COLORREF blue packs to the bottom five bits");
    BrSurfSetColourKey(&s, 0x00FFFFFFu);
    CHECK(s.key == 0xFFFFu, "white saturates every field");
    BrSurfSetColourKey(&s, 0x00000000u);
    CHECK(s.key == 0x0000u, "black clears every field");
}

static void TestPack(void)
{
    /* One row, three pixels: pure blue, pure green, pure red -- stored B,G,R.
     * cx == 3 so the row is 9 bytes and the DWORD stride is 12; the three
     * padding bytes are deliberately non-zero, so a converter that ignored
     * the stride would read them as a fourth pixel. */
    static uint8_t row[12] = { 0xFF,0x00,0x00,  0x00,0xFF,0x00,
                               0x00,0x00,0xFF,  0xAA,0xBB,0xCC };
    BrGdiBitmap bm;
    BrSurf     *s;

    MakeBitmap(&bm, row, 3, 1, 24);
    s = BrSurfFromBitmap(&bm);
    CHECK(s != NULL, "a 24bpp BITMAP converts");
    if (!s) return;

    CHECK(s->cx == 3 && s->cy == 1, "the surface takes the bitmap's size");
    CHECK(s->pPix[0] == 0x001Fu, "stored B first -> the low five bits");
    CHECK(s->pPix[1] == BR_SURF_KEY_565,
          "pure green packs to 0x07E0 -- six bits, and it IS the key");
    CHECK(s->pPix[2] == 0xF800u, "stored R third -> the top five bits");

    /* The asymmetric masks, stated as the thing a uniform mask would break:
     * 0xFC keeps six bits of green, so 252..255 all collapse onto 0x07E0.
     * (No shipped sheet contains such a pixel -- counted, 0 of 517,440 across
     * the 18 keyed sheets -- so this is a property of the arithmetic and not
     * a claim about the art.) */
    {
        static uint8_t near_[4] = { 0x07, 0xFC, 0x07, 0x00 };
        BrGdiBitmap b2; BrSurf *s2;
        MakeBitmap(&b2, near_, 1, 1, 24);
        s2 = BrSurfFromBitmap(&b2);
        CHECK(s2 && s2->pPix[0] == BR_SURF_KEY_565,
              "(7,252,7) quantises ONTO the key -- the fringe drops out too");
        BrSurfFree(s2);
    }
    BrSurfFree(s);
}

static void TestOrder(void)
{
    /* Four rows of one pixel, blue ramping 0,1,2,3 in STORAGE order.  A
     * bottom-up DIB stores the bottom row first, so storage row 0 is the
     * image's bottom row and the surface's LAST row must be the one holding
     * storage row 0. */
    static uint8_t rows[4][4] = { {0,0,0,0}, {1,0,0,0}, {2,0,0,0}, {3,0,0,0} };
    BrGdiBitmap bm;
    BrSurf     *s;

    MakeBitmap(&bm, &rows[0][0], 1, 4, 24);
    s = BrSurfFromBitmap(&bm);
    CHECK(s != NULL, "the four-row bitmap converts");
    if (!s) return;

    CHECK(s->pPix[0] == (uint16_t)(3u >> 3) &&
          s->pPix[3] == (uint16_t)(0u >> 3),
          "surface row 0 is the LAST stored row -- the bottom-up flip");
    /* Stated a second way, as a relation rather than two values, because the
     * ramp quantises to zero at 5 bits and the values above are both 0. */
    {
        static uint8_t big[4][4] = { {0x00,0,0,0}, {0x40,0,0,0},
                                     {0x80,0,0,0}, {0xC0,0,0,0} };
        BrGdiBitmap b2; BrSurf *s2;
        MakeBitmap(&b2, &big[0][0], 1, 4, 24);
        s2 = BrSurfFromBitmap(&b2);
        CHECK(s2 && s2->pPix[0] > s2->pPix[3],
              "the ramp comes out inverted, which is the flip");
        BrSurfFree(s2);
    }
    BrSurfFree(s);
}

static void TestGate(void)
{
    static uint8_t bits[64];
    BrGdiBitmap bm;
    uint16_t    bpp;
    int         fRefusedAll = 1;

    for (bpp = 1; bpp <= 32; bpp++) {
        BrSurf *s;
        if (bpp == 24) continue;
        MakeBitmap(&bm, bits, 1, 1, bpp);
        s = BrSurfFromBitmap(&bm);
        if (s) { fRefusedAll = 0; BrSurfFree(s); }
    }
    CHECK(fRefusedAll,
          "0x1000124B refuses every depth but 24 -- 8bpp included");

    MakeBitmap(&bm, bits, 1, 1, 24);
    {
        BrSurf *s = BrSurfFromBitmap(&bm);
        CHECK(s != NULL, "...and accepts 24");
        BrSurfFree(s);
    }
    CHECK(BrSurfFromBitmap(NULL) == NULL, "a NULL bitmap is refused");
}

static void TestWiden(void)
{
    CHECK(BrSurf565ToRgb(BR_SURF_KEY_565) == 0x00FF00u,
          "the key widens back to exactly 0x00FF00 -- replication, not shift");
    CHECK(BrSurf565ToRgb(0xFFFFu) == 0xFFFFFFu, "white widens to white");
    CHECK(BrSurf565ToRgb(0x0000u) == 0x000000u, "black widens to black");
    CHECK(BrSurf565ToRgb(0xF800u) == 0xFF0000u, "saturated red round-trips");
    CHECK(BrSurf565ToRgb(0x001Fu) == 0x0000FFu, "saturated blue round-trips");
}

static void TestSurfLifetime(void)
{
    BrSurf *s = BrSurfNew(4, 3);
    CHECK(s != NULL, "BrSurfNew allocates");
    if (s) {
        CHECK(s->cx == 4 && s->cy == 3, "dimensions as passed");
        CHECK(s->key == 0, "0x1000115C clears the key at construction");
        CHECK(s->pPix != NULL, "pixels allocated -- cx*cy*2, pitch cx");
        BrSurfFree(s);
    }
    BrSurfFree(NULL);          /* 0x10001195's `test esi,esi / je` */
    CHECK(1, "BrSurfFree(NULL) is a no-op, as 0x10001195 makes it");
}

int main(void)
{
    TestKey();
    TestPack();
    TestOrder();
    TestGate();
    TestWiden();
    TestSurfLifetime();

    printf("br_surf: %d checks, %d failures\n", g_checks, g_fails);
    return g_fails ? 1 : 0;
}
