/* test_br_font.c -- the font recovered from BRD3D.dll, and 0x10018590.
 *
 * The assertions here are properties of the DATA and of the emitter's control
 * flow, not counts.  Two of them are worth calling out because they are the
 * ones that would fail if the strips had been misidentified:
 *
 *  - "IA8, not something else": every renderable glyph contains BOTH texels
 *    with a zero high nibble and a full low nibble (outline: black, opaque)
 *    AND texels that are 0xFF (body: white, opaque).  Under any other 8-bit
 *    reading -- I8, A8, a palette index -- an outlined font would not split
 *    that way, and under a wrong base address the strip would be noise and
 *    would not split that way either.
 *
 *  - "the two offset runs are concatenated": the table rises monotonically
 *    across 0..27 and again across 28..54 and DROPS between them.  That is
 *    the structural fact the class map depends on, and it is what makes class
 *    27 unusable.
 */
#include "br_font.h"
#include <stdio.h>
#include <string.h>

static int g_fail;
static void check(int c, const char *w)
{ printf("  [%s] %s\n", c ? "PASS" : "FAIL", w); if (!c) g_fail = 1; }

static uint8_t g_target[320 * 240 * 4];
static uint32_t g_dl[4096];

/* Collect every 0xE3 texture rectangle an emit produced. */
typedef struct Rect { int32_t ulx, uly, lrx, lry; } Rect;

static size_t collect(const uint32_t *pDL, size_t cWords, Rect *pOut, size_t cMax)
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

/* Emit one string into g_dl and report how far the pen moved, measured as the
 * right edge of the last rectangle minus the left edge of the first. */
static size_t emit(const BrFont *pFont, const char *psz, int32_t scale,
                   int32_t x, int32_t y, int32_t fHiRes)
{
    BrTextEmit st;

    BrTextEmitInit(&st, pFont, g_dl, sizeof(g_dl) / sizeof(g_dl[0]));
    st.x      = x;
    st.y      = y;
    st.scale  = scale;
    st.fHiRes = fHiRes;
    BrTextEmitString(&st, psz);
    return st.cWordsWanted;
}

int main(void)
{
    BrFont f;
    Rect   aR[64], aR2[64];
    size_t n, n2, cw;
    int    i, s, cls, cRenderable = 0;

    if (BrFontLoad(&f, "orig/BRD3D.dll") != 0) {
        printf("SKIP br_font: orig/BRD3D.dll not readable\n");
        return 0;
    }
    check(1, "BrFontLoad(orig/BRD3D.dll)");

    /* ---- the class map ------------------------------------------------ */
    check(BrFontClassOf(&f, '1') == 0 && BrFontClassOf(&f, '9') == 8 &&
          BrFontClassOf(&f, '0') == 9,
          "digits: '1' opens the run and '0' closes it, not the other way");
    check(BrFontClassOf(&f, 'A') == BR_FONT_CLASS_ALPHA &&
          BrFontClassOf(&f, 'Z') == BR_FONT_CLASS_ALPHA + 25,
          "letters occupy classes 28..53");
    for (i = 'a'; i <= 'z'; ++i)
        if (BrFontClassOf(&f, i) != BrFontClassOf(&f, i - 32))
            break;
    check(i > 'z', "lower case folds onto upper case");
    check(BrFontClassOf(&f, ' ') < 0 && BrFontClassOf(&f, '\n') < 0,
          "space and control characters have no class");
    /* The original compares the byte SIGNED, so the high half is outside. */
    check(BrFontClassOf(&f, 0xC9) < 0 && BrFontClassOf(&f, 0xFF) < 0,
          "bytes >= 0x80 are outside the class range (signed compare)");
    for (i = 0; i < BR_FONT_CLASS_N; ++i)
        if (f.aClass[i] == BR_FONT_CLASS_GAP)
            break;
    check(i == BR_FONT_CLASS_N, "no character maps to the gap class 27");

    /* ---- the offset tables -------------------------------------------- */
    for (s = 0; s < 2; ++s) {
        int mono = 1;
        for (i = 0; i < BR_FONT_CLASS_GAP; ++i)
            if (f.aOff[s][i + 1] <= f.aOff[s][i]) mono = 0;
        for (i = BR_FONT_CLASS_ALPHA; i < BR_FONT_CLASSES - 1; ++i)
            if (f.aOff[s][i + 1] <= f.aOff[s][i]) mono = 0;
        check(mono, s == 0 ? "large offsets rise within each run"
                           : "small offsets rise within each run");
        check(f.aOff[s][BR_FONT_CLASS_ALPHA] < f.aOff[s][BR_FONT_CLASS_GAP],
              s == 0 ? "large table DROPS at the run boundary"
                     : "small table DROPS at the run boundary");
        check(f.aOff[s][0] == 0 && f.aOff[s][BR_FONT_CLASS_ALPHA] == 0,
              s == 0 ? "large runs both start at column 0"
                     : "small runs both start at column 0");
    }

    /* ---- every glyph fits inside its strip ----------------------------- */
    {
        int fits = 1, sawOutline = 0, sawBody = 0, allHaveBoth = 1;

        for (s = 0; s < 2; ++s) {
            for (cls = 0; cls < BR_FONT_CLASSES - 1; ++cls) {
                BrGlyph gl;
                int     x, y, outline = 0, body = 0;

                if (cls == BR_FONT_CLASS_GAP)
                    continue;
                if (BrFontGlyph(&f, cls, s, &gl) != 0) { fits = 0; continue; }
                if (gl.w <= 0 || gl.h <= 0) { fits = 0; continue; }
                ++cRenderable;
                for (y = 0; y < gl.h; ++y)
                    for (x = 0; x < gl.w; ++x) {
                        uint8_t t = gl.pIA8[y * gl.pitch + x];
                        if ((t >> 4) == 0 && (t & 0xF) == 0xF) outline = 1;
                        if (t == 0xFF)                          body    = 1;
                    }
                sawOutline |= outline;
                sawBody    |= body;
                if (!outline || !body) allHaveBoth = 0;
            }
        }
        check(fits, "every renderable class resolves to a glyph inside its strip");
        check(cRenderable == 2 * (BR_FONT_CLASSES - 2),
              "53 renderable classes in each of the two sizes");
        check(sawOutline && sawBody, "IA8: opaque-black and opaque-white texels present");
        check(allHaveBoth, "every glyph has both an outline and a body -- "
                           "the strips are outlined IA8, not noise");
    }

    /* The gap class and out-of-range classes must be refused, because the
     * offset table straddles two runs there and a width taken across the
     * boundary would be negative. */
    {
        BrGlyph gl;
        check(BrFontGlyph(&f, BR_FONT_CLASS_GAP, BR_FONT_LARGE, &gl) != 0,
              "class 27 (the gap) is refused");
        check(BrFontGlyph(&f, BR_FONT_CLASSES - 1, BR_FONT_LARGE, &gl) != 0,
              "class 54 (the terminator) is refused");
        check(BrFontGlyph(&f, -1, BR_FONT_LARGE, &gl) != 0,
              "a negative class is refused");
    }

    /* ---- the shading ramp --------------------------------------------- */
    {
        int nibbled = 1, falls;
        for (s = 0; s < 2; ++s)
            for (i = 0; i < BR_FONT_RAMP_BYTES; ++i) {
                uint8_t t = f.aRamp[s][0][i];
                if ((t >> 4) != (t & 0xF)) nibbled = 0;
            }
        check(nibbled, "ramp bytes are nibble-replicated (IA8 with I == A)");
        falls = f.aRamp[BR_FONT_LARGE][0][0] == 0xFF &&
                f.aRamp[BR_FONT_LARGE][0][(BR_FONT_RAMP_H - 1) *
                                          BR_FONT_RAMP_W] == 0x00;
        check(falls, "ramp variant 0 runs full at the top to zero at the bottom");
    }

    /* ---- 0x10018590: control flow -------------------------------------- */
    cw = emit(&f, "", BR_FONT_LARGE_CELL, 10, 60, 0);
    check(collect(g_dl, cw, aR, 64) == 0, "an empty string draws no rectangles");
    check(count_cmd(g_dl, cw, 0xE7u) == 3,
          "an empty string still emits the preamble and the epilogue");

    cw = emit(&f, "AB", BR_FONT_LARGE_CELL, 10, 60, 0);
    n  = collect(g_dl, cw, aR, 64);
    check(n == 2, "two glyphs, two rectangles");
    check(count_cmd(g_dl, cw, 0xDCu) == 2, "one texture bind per glyph");
    check(aR[0].ulx == 10, "the first glyph starts at the requested x");
    check(aR[1].ulx > aR[0].ulx, "the pen advances");

    /* The pen advance is additive over concatenation: this is the property
     * 0x100193C0 relies on, and it holds only because every glyph's advance
     * comes from the same cumulative offset table. */
    {
        int32_t advA, advC;
        cw   = emit(&f, "AB", BR_FONT_LARGE_CELL, 0, 60, 0);
        collect(g_dl, cw, aR, 64);
        advA = aR[1].ulx;                      /* pen after 'A' */
        cw   = emit(&f, "CD", BR_FONT_LARGE_CELL, 0, 60, 0);
        collect(g_dl, cw, aR2, 64);
        advC = aR2[1].ulx;                     /* pen after 'C' */
        cw   = emit(&f, "ACD", BR_FONT_LARGE_CELL, 0, 60, 0);
        n2   = collect(g_dl, cw, aR, 64);
        check(n2 == 3 && aR[2].ulx == advA + advC,
              "pen advances are additive over concatenation");
    }

    /* Spaces advance by (14*scale)/40 + 1 -- and the ORIGINAL's own width
     * routine omits the +1, which is why a spaced string draws wider than it
     * measures.  Checked as a difference so the constant is not restated. */
    {
        int32_t x1, x2;
        cw = emit(&f, "A A", BR_FONT_LARGE_CELL, 0, 60, 0);
        n  = collect(g_dl, cw, aR, 64);
        check(n == 2, "a space draws nothing");
        x1 = aR[1].ulx;
        cw = emit(&f, "AA", BR_FONT_LARGE_CELL, 0, 60, 0);
        collect(g_dl, cw, aR2, 64);
        x2 = aR2[1].ulx;
        check(x1 - x2 == (14 * BR_FONT_LARGE_CELL) / 40 + 1,
              "a space advances (14*scale)/40 + 1");
    }

    /* Escapes. */
    cw = emit(&f, "%iA", BR_FONT_LARGE_CELL, 10, 60, 0);
    check(collect(g_dl, cw, aR, 64) == 1, "\"%i\" is consumed and draws nothing");
    cw = emit(&f, "%nA", BR_FONT_LARGE_CELL, 10, 60, 0);
    check(collect(g_dl, cw, aR, 64) == 1, "\"%n\" is consumed and draws nothing");
    cw = emit(&f, "%%", BR_FONT_LARGE_CELL, 10, 60, 0);
    check(collect(g_dl, cw, aR, 64) == 1, "\"%%\" draws one glyph");
    cw = emit(&f, "%", BR_FONT_LARGE_CELL, 10, 60, 0);
    check(collect(g_dl, cw, aR, 64) == 1,
          "a trailing \"%\" is drawn as a glyph");
    /* A colour code whose SECOND letter is off the end degenerates: the '%'
     * is drawn, the cursor steps by one, and the letter is then drawn as an
     * ordinary glyph on the next pass -- so "%r" draws TWO glyphs.  The same
     * fall-through is recorded for 0x100193C0 in slice6_76.h. */
    cw = emit(&f, "%r", BR_FONT_LARGE_CELL, 10, 60, 0);
    check(collect(g_dl, cw, aR, 64) == 2,
          "\"%r\" at the end of a string draws both characters");

    /* A two-letter colour code emits both halves of the gradient and draws
     * nothing.  The preamble already sets one of each, hence the +1. */
    cw = emit(&f, "%rw", BR_FONT_LARGE_CELL, 10, 60, 0);
    check(collect(g_dl, cw, aR, 64) == 0, "\"%rw\" draws nothing");
    check(count_cmd(g_dl, cw, 0xFAu) == 2 && count_cmd(g_dl, cw, 0xFBu) == 2,
          "\"%rw\" sets both ends of the gradient");
    cw = emit(&f, "%zzA", BR_FONT_LARGE_CELL, 10, 60, 0);
    check(collect(g_dl, cw, aR, 64) == 1,
          "an unrecognised colour code still eats both letters");
    check(count_cmd(g_dl, cw, 0xFAu) == 1 && count_cmd(g_dl, cw, 0xFBu) == 1,
          "an unrecognised colour code changes no colour");

    cw = emit(&f, "%xFF8000A", BR_FONT_LARGE_CELL, 10, 60, 0);
    check(collect(g_dl, cw, aR, 64) == 1, "\"%xRRGGBB\" consumes eight bytes");

    /* ---- geometry ------------------------------------------------------ */
    /* The large font is chosen at scale >= 0x19 and the small below it, and
     * the test is made against the ALREADY-DOUBLED scale in hi-res. */
    {
        int32_t hBig, hSmall;
        cw = emit(&f, "A", BR_FONT_LARGE_MIN, 10, 200, 0);
        collect(g_dl, cw, aR, 64);
        hBig = aR[0].lry - aR[0].uly;
        cw = emit(&f, "A", BR_FONT_LARGE_MIN - 1, 10, 200, 0);
        collect(g_dl, cw, aR2, 64);
        hSmall = aR2[0].lry - aR2[0].uly;
        check(hBig == BR_FONT_LARGE_MIN && hSmall == BR_FONT_LARGE_MIN - 1,
              "the drawn cell height is the scale, either side of the threshold");
        /* One scale unit narrower but a different font: the same glyph is
         * WIDER at scale 24 than at 25, because the small font's cell is 20
         * rather than 40 and the width divides by it. */
        check(aR2[0].lrx - aR2[0].ulx > aR[0].lrx - aR[0].ulx,
              "crossing the threshold downwards makes the glyph wider");
    }

    /* Hi-res doubles x, y and the scale before anything else -- including
     * before the large/small threshold test.  So drawing at scale s in hi-res
     * is the same geometry as drawing at 2s without it; taken at the origin,
     * where the y offset (30*scale)/40 doubles too, the two rectangles are
     * identical.  That is a stronger statement than "twice as wide", and it
     * is the one that fails if the doubling is applied in the wrong order. */
    {
        cw = emit(&f, "A", 20, 0, 0, 1);
        n  = collect(g_dl, cw, aR, 64);
        cw = emit(&f, "A", 40, 0, 0, 0);
        n2 = collect(g_dl, cw, aR2, 64);
        check(n == 1 && n2 == 1 &&
              aR[0].ulx == aR2[0].ulx && aR[0].uly == aR2[0].uly &&
              aR[0].lrx == aR2[0].lrx && aR[0].lry == aR2[0].lry,
              "hi-res at scale 20 is identical to plain at scale 40");
    }
    {
        cw = emit(&f, "A", 20, 10, 100, 0);
        collect(g_dl, cw, aR, 64);
        cw = emit(&f, "A", 20, 10, 100, 1);
        collect(g_dl, cw, aR2, 64);
        check(aR2[0].ulx == 2 * aR[0].ulx, "hi-res doubles the left edge");
        check(aR2[0].lry - aR2[0].uly == 2 * (aR[0].lry - aR[0].uly),
              "hi-res doubles the drawn cell height");
    }

    /* A negative pen position takes the clamp arm, which clamps at zero and
     * only at zero. */
    {
        cw = emit(&f, "A", BR_FONT_LARGE_CELL, -30, 60, 0);
        n  = collect(g_dl, cw, aR, 64);
        check(n == 1 && aR[0].ulx == 0,
              "a negative left edge clamps to 0 (the out-of-bounds arm)");
    }

    /* ---- the rasteriser ------------------------------------------------ */
    memset(g_target, 0, sizeof(g_target));
    n = BrFontDrawString(&f, "MAIN MENU", BR_FONT_LARGE_CELL, 20, 60,
                         g_target, 320, 240);
    check(n == 8, "MAIN MENU rasterises eight glyphs (the space draws none)");
    {
        size_t k, ink = 0, opaque = 0;
        for (k = 0; k < 320u * 240u; ++k) {
            if (g_target[k * 4 + 3] != 0)   ++ink;
            if (g_target[k * 4 + 3] == 255) ++opaque;
        }
        check(ink > 0, "the caption puts pixels on the target");
        /* An outlined font is mostly fully opaque where it draws at all: the
         * antialiased fringe is a minority of the ink.  This is the property
         * that fails if the alpha nibble were being read as the intensity. */
        check(opaque * 2 > ink, "most drawn pixels are fully opaque");
        /* And it must not cover the screen: a caption is a small fraction. */
        check(ink < 320u * 240u / 4u, "the caption is not a screen-wide smear");
    }

    /* Drawing wholly off the target writes nothing. */
    memset(g_target, 0, sizeof(g_target));
    n = BrFontDrawString(&f, "MAIN MENU", BR_FONT_LARGE_CELL, 2000, 2000,
                         g_target, 320, 240);
    {
        size_t k, ink = 0;
        for (k = 0; k < 320u * 240u; ++k)
            if (g_target[k * 4 + 3] != 0) ++ink;
        check(ink == 0, "a caption entirely off the target writes nothing");
    }

    /* Two identical draws are idempotent over opaque pixels: the second pass
     * cannot change a pixel the first made fully opaque. */
    {
        static uint8_t once[320 * 240 * 4];
        memset(g_target, 0, sizeof(g_target));
        BrFontDrawString(&f, "CHAMPIONSHIP", 30, 20, 100,
                         g_target, 320, 240);
        memcpy(once, g_target, sizeof(once));
        BrFontDrawString(&f, "CHAMPIONSHIP", 30, 20, 100,
                         g_target, 320, 240);
        {
            size_t k, diff = 0;
            for (k = 0; k < 320u * 240u; ++k)
                if (g_target[k * 4 + 3] == 255 && once[k * 4 + 3] == 255 &&
                    memcmp(&g_target[k * 4], &once[k * 4], 3) != 0)
                    ++diff;
            check(diff == 0, "redrawing does not change opaque pixels");
        }
    }

    BrFontFree(&f);
    check(f.aStrip[0][0].pIA8 == NULL, "BrFontFree releases the strips");

    printf(g_fail ? "\n1 or more failures\n" : "\n0 failures\n");
    return g_fail;
}
