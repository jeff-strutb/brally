/* test_n64tex.c -- verify the CI4/LUT4 decoder against retail cargfx data. */
#include "br_n64tex.h"
#include "br_testdata.h"
#include <stdio.h>

static int g_fail;
static void check(int c, const char *w)
{ printf("  [%s] %s\n", c ? "PASS" : "FAIL", w); if (!c) g_fail = 1; }

int main(void)
{
    BR_REQUIRE_TESTDATA("testdata/skytexdesert.ci4", "n64tex");
    BrTex t;
    unsigned i, opaque = 0, distinct = 0;
    unsigned seen[256] = {0};

    if (BrTexLoadCI4(&t, "testdata/skytexdesert.ci4",
                     "testdata/skytexdesert.lut4", 64, 64) != 0) {
        printf("  [FAIL] BrTexLoadCI4\n");
        return 1;
    }
    check(1, "BrTexLoadCI4");
    check(t.width == 64 && t.height == 64, "dimensions");

    for (i = 0; i < t.width * t.height; i++) {
        if (t.pixels[i * 4 + 3] == 255) opaque++;
        seen[t.pixels[i * 4]] = 1;                 /* distinct red values */
    }
    for (i = 0; i < 256; i++) distinct += seen[i];

    printf("  opaque %u/%u, %u distinct red levels\n",
           opaque, t.width * t.height, distinct);
    /* the retail palette is fully opaque; a wrong endianness would scramble
     * the alpha bit and drop this well below 100% */
    check(opaque == t.width * t.height, "palette fully opaque (big-endian RGBA5551)");
    /* a sky gradient must use most of the 16 palette entries; if the nibble
     * order were reversed we would still get 16, but if the palette were
     * misread we would collapse to very few */
    check(distinct >= 8, "gradient uses a range of palette entries");

    /* ------------------------------------------------------------------ *
     * LITERAL TEXELS, WORKED OUT BY HAND FROM THE TWO FILES.
     *
     * The two counts above are a census: neither of them reads a single
     * channel value, so the whole of the colour arithmetic -- which
     * nibble is red, the 0x1F mask, the 5->8 bit replication and the
     * alpha bit -- is unasserted by them.  Every one of those can be
     * broken while `opaque == 4096` and `distinct >= 8` stay true.
     *
     * The workings, from `xxd testdata/skytexdesert.lut4` and
     * `xxd testdata/skytexdesert.ci4`:
     *
     *   lut4[0..1] = 30 C5, big-endian RGBA5551 0x30C5
     *       R = 0x30C5 >> 11        = 6   -> (6<<3)|(6>>2)    =  49
     *       G = (0x30C5 >> 6) & 1F  = 3   -> (3<<3)|(3>>2)    =  24
     *       B = (0x30C5 >> 1) & 1F  = 2   -> (2<<3)|(2>>2)    =  16
     *       A = 0x30C5 & 1          = 1   -> 255
     *   ci4[0] = 0x00, so texels 0 and 1 are both palette entry 0.
     *
     *   lut4[18..19] = B5 F3 -> 0xB5F3, palette entry 9
     *       R = 22 -> 181,  G = 23 -> 189,  B = 25 -> 206,  A = 255
     *   the first texel using entry 9 is 2146.
     *
     *   ci4[289] = 0x10, the first byte whose two nibbles differ:
     *       texel 578 is the HIGH nibble (entry 1, 0x4147 -> 66,41,24)
     *       texel 579 is the LOW  nibble (entry 0            -> 49,24,16)
     *
     * Entry 0 is the one that pins the alpha bit: 0x30C5 has bit 0 set and
     * bit 1 CLEAR, so reading `v & 2` instead of `v & 1` makes it
     * transparent.  Entry 9 is the one that pins the channels apart: its
     * three components are all different and all three carry replicated
     * low bits.
     * ------------------------------------------------------------------ */
    {
        static const struct { unsigned i; uint8_t r, g, b, a; } kTexel[] = {
            {    0,  49,  24,  16, 255 },   /* entry 0  (0x30C5) */
            {  578,  66,  41,  24, 255 },   /* entry 1  (0x4147), high nibble */
            {  579,  49,  24,  16, 255 },   /* entry 0,           low nibble  */
            { 2146, 181, 189, 206, 255 }    /* entry 9  (0xB5F3) */
        };
        unsigned k, cWrong = 0;
        for (k = 0; k < sizeof kTexel / sizeof kTexel[0]; k++) {
            const uint8_t *px = t.pixels + (size_t)kTexel[k].i * 4;
            printf("  texel %4u = %3u %3u %3u %3u (expect %3u %3u %3u %3u)\n",
                   kTexel[k].i, px[0], px[1], px[2], px[3],
                   kTexel[k].r, kTexel[k].g, kTexel[k].b, kTexel[k].a);
            if (px[0] != kTexel[k].r || px[1] != kTexel[k].g
             || px[2] != kTexel[k].b || px[3] != kTexel[k].a)
                cWrong++;
        }
        check(cWrong == 0, "four hand-decoded texels match byte for byte");
    }

    BrTexFree(&t);
    printf(g_fail ? "\nFAILED\n" : "\nALL PASSED\n");
    return g_fail;
}
