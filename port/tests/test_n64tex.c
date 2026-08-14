/* test_n64tex.c -- verify the CI4/LUT4 decoder against retail cargfx data. */
#include "br_n64tex.h"
#include <stdio.h>

static int g_fail;
static void check(int c, const char *w)
{ printf("  [%s] %s\n", c ? "PASS" : "FAIL", w); if (!c) g_fail = 1; }

int main(void)
{
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

    BrTexFree(&t);
    printf(g_fail ? "\nFAILED\n" : "\nALL PASSED\n");
    return g_fail;
}
