/* test_br_font_glide.c -- the facts that are true of ONE build, and the
 * cross-build diff that established them.
 *
 * test_br_font.c runs its whole battery against both DLLs and asserts only
 * things the two agree about.  This file holds the rest, and it needs BOTH
 * images loaded at once, which is why it is a separate suite.
 *
 * The load-bearing assertion is the pixel diff.  For every renderable class in
 * both sizes, every texel of the Glide blob equals the corresponding D3D texel
 * with its NIBBLES SWAPPED, read at the SAME row.  That single check pins
 * three separate things at once:
 *
 *   - the FORMAT: D3D is IA8 (intensity high, alpha low) and Glide is AI44
 *     (alpha high, intensity low).  It agrees with the format code 4 that
 *     0x1006C790 passes to the texture constructor, which is Glide's
 *     GR_TEXFMT_ALPHA_INTENSITY_44, and with the byte histograms (0x0F
 *     dominant in D3D, 0xF0 in Glide).
 *   - the LAYOUT: `base + stride*class` at pitch 64/32 lands on the same
 *     pixels as D3D's `base + offset[class]` at pitch 704/392.  A wrong
 *     stride, pitch or base would not match anywhere.
 *   - the ROW ORDER: the match is at the same row index, not the mirrored
 *     one, so the two independently-laid-out blobs agree that row 0 is the
 *     BOTTOM of the glyph.  The bottom-up storage is therefore not a D3D
 *     artefact, and it is not a misread base address either -- two different
 *     images would have to be wrong the same way.
 *
 * The ramps cannot contribute to any of that and the suite says so: every ramp
 * byte is nibble-replicated, so swapping is a no-op and the four blocks come
 * out byte-identical.  Asserting that they match would look like evidence for
 * the format and would be worth nothing.
 */
#include "br_font.h"
#include <stdio.h>
#include <string.h>

static int g_fail;
static void check(int c, const char *w)
{ printf("  [%s] %s\n", c ? "PASS" : "FAIL", w); if (!c) g_fail = 1; }

static uint32_t g_dl[4096];
static uint8_t  g_target[320 * 240 * 4];

typedef struct Rect { int32_t ulx, uly, lrx, lry; } Rect;

static size_t collect(const uint32_t *pDL, size_t cWords, Rect *pOut,
                      size_t cMax)
{
    size_t i, n = 0;
    for (i = 0; i + 1 < cWords; i += 2) {
        if ((pDL[i] >> 24) != 0xE3u)
            continue;
        if (n < cMax) {
            pOut[n].lrx = (int32_t)((pDL[i]     >> 12) & 0xFFFu);
            pOut[n].lry = (int32_t)( pDL[i]            & 0xFFFu);
            pOut[n].ulx = (int32_t)((pDL[i + 1] >> 12) & 0xFFFu);
            pOut[n].uly = (int32_t)( pDL[i + 1]        & 0xFFFu);
        }
        ++n;
    }
    return n;
}

static size_t count_cmd(const uint32_t *pDL, size_t cWords, uint32_t cmd)
{
    size_t i, n = 0;
    for (i = 0; i + 1 < cWords; i += 2)
        if ((pDL[i] >> 24) == cmd)
            ++n;
    return n;
}

static size_t emit(const BrFont *pFont, const char *psz, int32_t scale,
                   int32_t x, int32_t y, int32_t detail)
{
    BrTextEmit st;

    BrTextEmitInit(&st, pFont, g_dl, sizeof(g_dl) / sizeof(g_dl[0]));
    st.x      = x;
    st.y      = y;
    st.scale  = scale;
    st.detail = detail;
    BrTextEmitString(&st, psz);
    return st.cWordsWanted;
}

/* Where a rendered glyph is at its WIDEST, expressed as a fraction of its own
 * inked height: 0 at the top row, 1000 at the bottom.  This is the quantity
 * that a vertical flip turns into 1000 - x, which is what makes it a usable
 * orientation test on glyphs that are near-symmetric left to right.  Returns
 * -1 if nothing was drawn.
 *
 * A count of pixels would not do: an 'L' and an upside-down 'L' have the same
 * pixels.  The POSITION of the widest row is the asymmetry. */
static int32_t widest_row_permille(int32_t cx, int32_t cy)
{
    int32_t x, y, lo = -1, hi = -1, best = -1, bestW = 0;

    for (y = 0; y < cy; ++y) {
        int32_t a = cx, b = -1, w;
        for (x = 0; x < cx; ++x)
            if (g_target[((size_t)y * (size_t)cx + (size_t)x) * 4 + 3] != 0) {
                if (x < a) a = x;
                if (x > b) b = x;
            }
        w = (b < a) ? 0 : b - a + 1;
        if (w > 0) {
            if (lo < 0) lo = y;
            hi = y;
        }
        if (w > bestW) { bestW = w; best = y; }
    }
    if (lo < 0 || hi <= lo)
        return -1;
    return ((best - lo) * 1000) / (hi - lo);
}

int main(void)
{
    BrFont  g, d;
    Rect    aR[64], aR2[64];
    size_t  n, cw;
    int     cls, s, i;

    if (BrFontLoad(&g, "orig/BRGlide.dll") != 0 ||
        BrFontLoad(&d, "orig/BRD3D.dll") != 0) {
        printf("SKIP br_font_glide: both original DLLs are required\n");
        return 0;
    }

    /* ---- NOTHING SAMPLES THE TAIL -------------------------------------
     *
     * br_font.h asserts that no glyph's sampled rectangle runs past its
     * block, on the strength of the emitter's G_SETTILESIZE stopping at
     * `off[k+1] - off[k] + 1` columns.  That is a claim about the shipped
     * offset tables, so measure it instead of believing it: for every
     * renderable class in both sizes and BOTH builds, the last byte the
     * emitter can address -- row h-1, column w-1 -- must lie inside the
     * block the glyph came from.
     *
     * Both builds are checked because the two lay the blob out completely
     * differently (one window per class at stride 0xA00/0x280 against one
     * wide strip at pitch 704/392) and only share the offset tables.  A
     * single build passing would say nothing about the other. */
    {
        int okG = 1, okD = 1, okBlob = 1, cChecked = 0;

        for (s = 0; s < 2; ++s) {
            for (cls = 0; cls < BR_FONT_CLASSES; ++cls) {
                int b;
                for (b = 0; b < 2; ++b) {
                    const BrFont *pF = b ? &d : &g;
                    BrGlyph gl;
                    const uint8_t *pLast, *pBlobEnd = NULL, *pCellEnd = NULL;
                    int t;

                    if (BrFontGlyph(pF, cls, s, &gl) != 0)
                        continue;              /* the gap class, and 54 */
                    /* the last byte the emitted G_SETTILESIZE can address */
                    pLast = gl.pTexels + (size_t)(gl.h - 1) * (size_t)gl.pitch
                          + (size_t)(gl.w - 1);
                    for (t = 0; t < 2; ++t) {
                        const BrFontStrip *pS = &pF->aStrip[s][t];
                        size_t cb;
                        if (pS->pTexels == NULL)
                            continue;
                        cb = (pS->stride != 0)
                             ? (size_t)pS->stride * (size_t)BR_FONT_G_CELLS
                             : (size_t)pS->pitch * (size_t)pS->height;
                        if (gl.pTexels >= pS->pTexels &&
                            gl.pTexels <  pS->pTexels + cb) {
                            pBlobEnd = pS->pTexels + cb;
                            /* Glide gives each class a fixed window; D3D
                             * gives the whole run one strip. */
                            pCellEnd = (pS->stride != 0)
                                     ? pS->pTexels + (size_t)pS->stride * (size_t)(cls + 1)
                                     : pBlobEnd;
                        }
                    }
                    if (pBlobEnd == NULL) { okBlob = 0; continue; }
                    if (pLast >= pBlobEnd) okBlob = 0;
                    if (pLast >= pCellEnd) { if (b) okD = 0; else okG = 0; }
                    ++cChecked;
                }
            }
        }
        check(cChecked == 4 * (BR_FONT_CLASSES - 2),
              "every renderable class in both sizes and both builds measured");
        check(okG, "Glide: a class's sampled rectangle stays inside its own "
                   "0xA00/0x280 window -- the column tail is never read");
        check(okD, "D3D: the same, against a wholly different layout");
        check(okBlob, "and no glyph in either build samples past the end of "
                      "its blob, class 53 included");
    }

    /* ---- the loader tells the two apart -------------------------------- */
    check(g.build == BR_FONT_BUILD_GLIDE && d.build == BR_FONT_BUILD_D3D,
          "BrFontLoad identifies each build from the image, not the path");
    check(g.ahPage[BR_FONT_LARGE] != 0u && g.ahPage[BR_FONT_SMALL] != 0u &&
          d.ahPage[BR_FONT_LARGE] == 0u,
          "0x1006C790 makes two page textures; 0x10073820 makes none");
    check(d.ahTex[BR_FONT_LARGE][BR_FONT_CLASS_ALPHA] != 0u &&
          g.ahTex[BR_FONT_LARGE][BR_FONT_CLASS_ALPHA] == 0u,
          "0x10073820 fills a per-class handle table; Glide has none");
    check(g.aStrip[BR_FONT_LARGE][0].stride == BR_FONT_G_LARGE_STRIDE &&
          g.aStrip[BR_FONT_SMALL][0].stride == BR_FONT_G_SMALL_STRIDE &&
          d.aStrip[BR_FONT_LARGE][0].stride == 0,
          "Glide indexes by class stride, D3D by column");

    /* ---- the metrics are shared ---------------------------------------- */
    check(memcmp(g.aClass, d.aClass, sizeof g.aClass) == 0,
          "the class map is byte-identical in both builds");
    check(memcmp(g.aOff, d.aOff, sizeof g.aOff) == 0,
          "both offset tables are byte-identical in both builds");

    /* ---- the pixel diff: format, layout and row order all at once ------ */
    {
        int mismatch = 0, cChecked = 0, sawSwapMatters = 0;

        for (s = 0; s < 2; ++s) {
            for (cls = 0; cls < BR_FONT_CLASSES - 1; ++cls) {
                BrGlyph gg, gd;
                int     x, y;

                if (cls == BR_FONT_CLASS_GAP)
                    continue;
                if (BrFontGlyph(&g, cls, s, &gg) != 0 ||
                    BrFontGlyph(&d, cls, s, &gd) != 0) {
                    mismatch = 1;
                    continue;
                }
                if (gg.w != gd.w || gg.h != gd.h) { mismatch = 1; continue; }
                ++cChecked;
                for (y = 0; y < gg.h; ++y)
                    for (x = 0; x < gg.w; ++x) {
                        uint8_t a = gg.pTexels[y * gg.pitch + x];
                        uint8_t b = gd.pTexels[y * gd.pitch + x];
                        if (a != (uint8_t)(((b & 0x0Fu) << 4) | (b >> 4)))
                            mismatch = 1;
                        /* At least one texel must have UNEQUAL nibbles, or
                         * the swap would be vacuous -- the trap the ramps
                         * fall into. */
                        if ((b >> 4) != (b & 0x0Fu))
                            sawSwapMatters = 1;
                    }
            }
        }
        check(cChecked == 2 * (BR_FONT_CLASSES - 2),
              "53 classes compared in each of the two sizes");
        check(sawSwapMatters,
              "the glyphs contain texels whose nibbles differ, so the swap "
              "is a real claim and not a no-op");
        check(!mismatch,
              "every Glide texel is the D3D texel nibble-swapped, at the "
              "SAME row -- format AI44 vs IA8, same bottom-up row order");
    }

    /* The ramps are the counter-example: identical either way, so they say
     * nothing about the format.  Asserted so the distinction is recorded. */
    {
        int replicated = 1;
        for (s = 0; s < 2; ++s)
            for (i = 0; i < BR_FONT_RAMP_BYTES; ++i) {
                uint8_t t = g.aRamp[s][0][i];
                if ((t >> 4) != (t & 0x0Fu)) replicated = 0;
            }
        check(memcmp(g.aRamp, d.aRamp, sizeof g.aRamp) == 0 && replicated,
              "the four ramps are byte-identical AND nibble-replicated, so "
              "they cannot distinguish the two formats");
    }

    /* ---- the emitter's one visible divergence: the 0xDD ----------------- */
    cw = emit(&g, "AB", BR_FONT_LARGE_CELL, 10, 60, 1);
    check(count_cmd(g_dl, cw, 0xDDu) == 2 && count_cmd(g_dl, cw, 0xDCu) == 2,
          "Glide emits a 0xDD before every 0xDC (0x10015FD0)");
    cw = emit(&d, "AB", BR_FONT_LARGE_CELL, 10, 60, 1);
    check(count_cmd(g_dl, cw, 0xDDu) == 0 && count_cmd(g_dl, cw, 0xDCu) == 2,
          "D3D emits the 0xDC alone (0x100189D4)");

    /* The 0xDD payload is the class's window address, not a token. */
    {
        size_t   k;
        int      seen = 0, wrong = 0;
        uint32_t want[2];

        want[0] = g.aBlockVa[BR_FONT_LARGE] +
                  (uint32_t)g.aStrip[BR_FONT_LARGE][0].stride *
                  (uint32_t)BrFontClassOf(&g, 'A');
        want[1] = g.aBlockVa[BR_FONT_LARGE] +
                  (uint32_t)g.aStrip[BR_FONT_LARGE][0].stride *
                  (uint32_t)BrFontClassOf(&g, 'B');
        cw = emit(&g, "AB", BR_FONT_LARGE_CELL, 10, 60, 1);
        for (k = 0; k + 1 < cw; k += 2)
            if ((g_dl[k] >> 24) == 0xDDu) {
                if (seen < 2 && g_dl[k + 1] != want[seen]) wrong = 1;
                ++seen;
            }
        check(seen == 2 && !wrong,
              "the 0xDD payload is base + stride*class (0x10015FDC)");
    }

    /* ---- the detail global is D3D-only --------------------------------- */
    {
        int32_t wD1, wD2, wG1, wG2;

        cw = emit(&d, "W", BR_FONT_LARGE_CELL, 0, 60, 1);
        collect(g_dl, cw, aR, 64);
        wD1 = aR[0].lrx - aR[0].ulx;
        cw = emit(&d, "W", BR_FONT_LARGE_CELL, 0, 60, 2);
        collect(g_dl, cw, aR2, 64);
        wD2 = aR2[0].lrx - aR2[0].ulx;
        check(wD2 > wD1,
              "D3D: detail > 1 forces the small font (0x100185E7)");

        cw = emit(&g, "W", BR_FONT_LARGE_CELL, 0, 60, 1);
        collect(g_dl, cw, aR, 64);
        wG1 = aR[0].lrx - aR[0].ulx;
        cw = emit(&g, "W", BR_FONT_LARGE_CELL, 0, 60, 2);
        collect(g_dl, cw, aR2, 64);
        wG2 = aR2[0].lrx - aR2[0].ulx;
        check(wG1 == wG2 && wG1 == wD1,
              "Glide: detail is not read at all (0x10015B67)");

        check(BrFontMeasure(&d, "WWW", BR_FONT_LARGE_CELL, 0, 2) >
              BrFontMeasure(&d, "WWW", BR_FONT_LARGE_CELL, 0, 1) &&
              BrFontMeasure(&g, "WWW", BR_FONT_LARGE_CELL, 0, 2) ==
              BrFontMeasure(&g, "WWW", BR_FONT_LARGE_CELL, 0, 1),
              "the same split in the width routines (0x100193C0 vs "
              "0x10016980, the 9 bytes that make them 207 and 198)");
    }

    /* ---- the width routines agree everywhere else ----------------------- */
    {
        static const char *apsz[] = {
            "", "A", "MAIN MENU", "CHAMPIONSHIP", "0123456789",
            "SET DRIVER", "BACK", "A A A", "%rwABC", "%%", "%iX", "%zzQ",
            "the quick brown fox", "!\"#$&'()*+,-./:;<=>?@[]^_`{|}~"
        };
        int same = 1;
        for (i = 0; i < (int)(sizeof apsz / sizeof apsz[0]); ++i) {
            int32_t sc;
            for (sc = 8; sc <= 64; sc += 4)
                if (BrFontMeasure(&g, apsz[i], sc, 0, 1) !=
                    BrFontMeasure(&d, apsz[i], sc, 0, 1) ||
                    BrFontMeasure(&g, apsz[i], sc, 1, 1) !=
                    BrFontMeasure(&d, apsz[i], sc, 1, 1))
                    same = 0;
        }
        check(same, "0x10016980 and 0x100193C0 agree on every string and "
                    "scale tried, hi-res and not");
    }

    /* ---- QUIRK 1: the tile is one column wider than the advance --------- */
    {
        int32_t adv, drawn;

        cw = emit(&g, "AB", BR_FONT_LARGE_CELL, 0, 60, 1);
        n  = collect(g_dl, cw, aR, 64);
        /* The pen lands where the width routine says it does ... */
        check(n == 2 && aR[1].ulx == BrFontMeasure(&g, "A",
                                                   BR_FONT_LARGE_CELL, 0, 1),
              "Glide: the pen advance IS the measured width");
        /* ... but the ink reaches further, because the tile is one wider. */
        adv   = BrFontMeasure(&g, "AB", BR_FONT_LARGE_CELL, 0, 1);
        drawn = aR[1].lrx;
        check(drawn > adv,
              "Glide: the last glyph's ink runs past the measured width -- "
              "0x10015FB7's `inc ecx`, which 0x10016980 does not add");

        /* And a space is padded by one in the emitter only. */
        {
            int32_t xSpaced, xTight;
            cw = emit(&g, "A A", BR_FONT_LARGE_CELL, 0, 60, 1);
            collect(g_dl, cw, aR, 64);
            xSpaced = aR[1].ulx;
            cw = emit(&g, "AA", BR_FONT_LARGE_CELL, 0, 60, 1);
            collect(g_dl, cw, aR2, 64);
            xTight = aR2[1].ulx;
            check(xSpaced - xTight ==
                      BrFontMeasure(&g, "A A", BR_FONT_LARGE_CELL, 0, 1) -
                      BrFontMeasure(&g, "AA", BR_FONT_LARGE_CELL, 0, 1) + 1,
                  "Glide: a space draws one pixel wider than it measures "
                  "(0x100161E1 against 0x10016A2A)");
        }
    }

    /* ---- QUIRK 2: the out-of-bounds arm is not a scissor ---------------- */
    {
        /* Right edge past 0x140 with a positive left edge: the clamp arm
         * runs, clamps nothing (both corners are already positive) and lets
         * the rectangle out unchanged. */
        cw = emit(&g, "W", BR_FONT_LARGE_CELL, 330, 60, 1);
        n  = collect(g_dl, cw, aR, 64);
        check(n == 1 && aR[0].ulx == 330 && aR[0].lrx > 0x140,
              "Glide: a rectangle past the right edge still goes out "
              "(0x10016112 clamps at zero and only at zero)");
        cw = emit(&g, "W", BR_FONT_LARGE_CELL, -40, 60, 1);
        n  = collect(g_dl, cw, aR, 64);
        check(n == 1 && aR[0].ulx == 0,
              "Glide: a negative left edge clamps to zero");
        /* Below the bottom: same arm, same non-clamping. */
        cw = emit(&g, "W", BR_FONT_LARGE_CELL, 10, 300, 1);
        n  = collect(g_dl, cw, aR, 64);
        check(n == 1 && aR[0].lry > 0xF0,
              "Glide: a rectangle past the bottom edge still goes out");
    }

    /* ---- QUIRK 3: bottom-up storage, checked on the rendered output ----- */
    {
        /* Six glyphs whose vertical asymmetry runs in KNOWN and OPPOSITE
         * directions.  'L' and 'J' are widest near their foot; 'F', 'T', 'E'
         * and 'P' are widest near their bar.  A vertical flip maps the
         * measure to 1000 - x, so it would break all six at once -- which is
         * the point: capitals are near-symmetric left to right and a flip is
         * easy to miss on any single letter.
         *
         * Both builds are measured, so this also says the two paths agree
         * about orientation and not merely about pixel counts. */
        static const struct { char ch; int fLow; } aG[] = {
            { 'L', 1 }, { 'J', 1 },
            { 'F', 0 }, { 'T', 0 }, { 'E', 0 }, { 'P', 0 }
        };
        int okG = 1, okD = 1, opposite = 1;

        for (i = 0; i < (int)(sizeof aG / sizeof aG[0]); ++i) {
            char    sz[2];
            int32_t pG, pD;

            sz[0] = aG[i].ch;
            sz[1] = '\0';

            memset(g_target, 0, sizeof g_target);
            BrFontDrawString(&g, sz, 40, 20, 60, g_target, 320, 240);
            pG = widest_row_permille(320, 240);

            memset(g_target, 0, sizeof g_target);
            BrFontDrawString(&d, sz, 40, 20, 60, g_target, 320, 240);
            pD = widest_row_permille(320, 240);

            if (pG < 0 || pD < 0 || pG != pD) opposite = 0;
            if (aG[i].fLow) {
                if (!(pG > 550)) okG = 0;
                if (!(pD > 550)) okD = 0;
            } else {
                if (!(pG < 450)) okG = 0;
                if (!(pD < 450)) okD = 0;
            }
        }
        check(okG, "Glide: 'L' and 'J' render widest near the BOTTOM and "
                   "'F','T','E','P' near the TOP -- the T inversion is "
                   "right and the blob is stored bottom-up");
        check(okD, "D3D: the same six land the same way up");
        check(opposite,
              "both builds put the widest row at the same height, so the "
              "orientation is a property of the font and not of one reader");
    }

    /* ---- the two builds rasterise the same picture ---------------------- */
    {
        static uint8_t aG[320 * 240 * 4];
        size_t k, diff = 0, ink = 0;

        memset(g_target, 0, sizeof g_target);
        BrFontDrawString(&g, "SET DRIVER", 30, 20, 100, g_target, 320, 240);
        memcpy(aG, g_target, sizeof aG);
        memset(g_target, 0, sizeof g_target);
        BrFontDrawString(&d, "SET DRIVER", 30, 20, 100, g_target, 320, 240);
        for (k = 0; k < 320u * 240u; ++k) {
            if (aG[k * 4 + 3] != 0) ++ink;
            if (memcmp(&aG[k * 4], &g_target[k * 4], 4) != 0) ++diff;
        }
        check(ink > 0 && diff == 0,
              "a caption rasterises identically through both paths -- the "
              "nibble swap and the two layouts cancel exactly");
    }

    BrFontFree(&g);
    BrFontFree(&d);
    printf(g_fail ? "\n1 or more failures\n" : "\n0 failures\n");
    return g_fail;
}
