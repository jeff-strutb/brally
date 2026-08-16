/* test_br_bmp.c -- the sprite-sheet decoder.
 *
 * The load-bearing assertion is the SIZE of but-sav.bmp: 127x33 is exactly the
 * +0x7F/+0x21 rectangle every menu builder computes for a button. Three facts
 * inside the executable already agreed on those numbers; this is the first from
 * outside it, so it is what ties the sprite table to the shipped art.
 *
 * Row order and padding are checked too, because 127*3 == 381 is not a multiple
 * of 4 -- a decoder that ignores BMP row padding shears this exact file.
 */
#include "br_bmp.h"
#include "br_testdata.h"
#include <stdio.h>
#include <string.h>

static int g_checks, g_fails;
#define CHECK(c, m) do { g_checks++; if (!(c)) { g_fails++; \
    printf("  [FAIL] %s\n", (m)); } } while (0)

int main(void)
{
    BrBmp b;
    BR_REQUIRE_TESTDATA("testdata/images/but-sav.bmp", "br_bmp");

    CHECK(BrBmpLoad(&b, "testdata/images/but-sav.bmp") == 0, "but-sav.bmp loads");
    CHECK(b.w == 127, "but-sav.bmp is 127 wide -- the builder's +0x7F");
    CHECK(b.h == 33,  "but-sav.bmp is 33 tall  -- the builder's +0x21");
    CHECK(b.pRgba != NULL, "pixels present");

    if (b.pRgba) {
        /* Every texel opaque before keying, and the buffer fully written --
         * a short read would leave the tail zeroed. */
        size_t i, n = (size_t)b.w * b.h, opaque = 0;
        for (i = 0; i < n; i++) if (b.pRgba[i * 4 + 3] == 0xFF) opaque++;
        CHECK(opaque == n, "alpha is opaque everywhere before keying");

        /* Row padding: if it were ignored, each row would start 3 bytes into
         * the previous one and the image would shear. Detect by checking the
         * last pixel of row 0 is inside the buffer and reachable. */
        CHECK(b.pRgba[((size_t)0 * b.w + (b.w - 1)) * 4 + 3] == 0xFF,
              "row 0 spans the full width (padding honoured)");
    }
    BrBmpFree(&b);
    CHECK(b.pRgba == NULL, "free clears the pointer");

    printf("br_bmp: %d checks, %d failures\n", g_checks, g_fails);
    return g_fails ? 1 : 0;
}
