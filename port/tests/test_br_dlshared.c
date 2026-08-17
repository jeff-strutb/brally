/* test_br_dlshared.c -- the display-list routines both builds share.
 *
 * EVERY EXPECTED VALUE HERE IS DERIVED FROM THE DISASSEMBLY, not from the
 * comments that used to sit over the two copies of each routine. That
 * distinction is load-bearing: two of the three routines below were ported
 * twice in this tree, and in both cases one copy was wrong and had a comment
 * agreeing with it.
 *
 * The assertions marked MUTATION are the ones that fail if the specific
 * defect that was found is put back. Each names it.
 */
#include "br_dlshared.h"

#include <math.h>
#include <stdio.h>

static int g_fail;
static void check(int c, const char *w)
{ printf("  [%s] %s\n", c ? "PASS" : "FAIL", w); if (!c) g_fail = 1; }

/* ------------------------------------------------------------------ */
/* 0x10022120 / 0x10022DC0 -- the outcodes                             */
/* ------------------------------------------------------------------ */

static void test_clipcodes(void)
{
    const float nan = (float)(0.0 / 0.0);
    int32_t oc;

    printf("clip codes (0x10022120 glide / 0x10022DC0 d3d)\n");

    /* Dead centre of a unit frustum: nothing set. */
    check(BrDlsClipCodes(0.0f, 0.0f, 0.0f, 1.0f) == 0,
          "a vertex inside every plane has no clip bits");

    /* w < 0 sets bit 0 -- and, because every other test is written against
     * the same w, it drags most of the others with it. Assert bit 0 alone. */
    oc = BrDlsClipCodes(0.0f, 0.0f, 0.0f, -1.0f);
    check((oc & 0x01) != 0, "w < 0 sets the w bit");

    /* WHICH COMPONENT DRIVES WHICH BIT. This is the ordering that the two
     * copies had to agree on and that the bit NAMES do not tell you: the
     * middle five tests run z, z, x, x, y, y. */
    check(BrDlsClipCodes(0.0f, 0.0f, -2.0f, 1.0f) == 0x02,
          "z + w < 0 sets 0x02 and nothing else");
    check(BrDlsClipCodes(0.0f, 0.0f, 2.0f, 1.0f) == 0x04,
          "w - z < 0 sets 0x04 and nothing else");
    check(BrDlsClipCodes(-2.0f, 0.0f, 0.0f, 1.0f) == 0x08,
          "x + w < 0 sets 0x08 -- x, not y, despite 0x08 being 'left'");
    check(BrDlsClipCodes(2.0f, 0.0f, 0.0f, 1.0f) == 0x10,
          "w - x < 0 sets 0x10");
    check(BrDlsClipCodes(0.0f, -2.0f, 0.0f, 1.0f) == 0x20,
          "y + w < 0 sets 0x20");
    check(BrDlsClipCodes(0.0f, 2.0f, 0.0f, 1.0f) == 0x40,
          "w - y < 0 sets 0x40");

    /* The boundary itself is INSIDE: the original's `test ah,1` reads C0,
     * which is set for strictly-less-than only. An exactly-zero sum must not
     * clip, or every vertex touching a plane would be rejected. */
    check(BrDlsClipCodes(-1.0f, 0.0f, 0.0f, 1.0f) == 0,
          "x + w == 0 exactly is INSIDE, not clipped");
    check(BrDlsClipCodes(0.0f, 0.0f, 0.0f, 0.0f) == 0,
          "w == 0 exactly is INSIDE too -- `jl`, not `jle`");

    /* MUTATION: writing any of the seven as `v < 0.0f` instead of
     * `!(v >= 0.0f)` makes NaN take the INSIDE side. The original's compare
     * is unordered-aware (C0 is set for unordered), so a NaN is rejected on
     * every plane it appears in. This is the assertion the two copies of this
     * function disagreed about for the whole life of the duplication. */
    check(BrDlsClipCodes(nan, 0.0f, 0.0f, 1.0f) == (0x08 | 0x10),
          "MUTATION: a NaN x sets BOTH x planes -- unordered takes the clip "
          "side");
    check(BrDlsClipCodes(0.0f, nan, 0.0f, 1.0f) == (0x20 | 0x40),
          "MUTATION: a NaN y sets both y planes");
    check(BrDlsClipCodes(0.0f, 0.0f, nan, 1.0f) == (0x02 | 0x04),
          "MUTATION: a NaN z sets both z planes");
    check(BrDlsClipCodes(0.0f, 0.0f, 0.0f, nan) == 0x7F,
          "MUTATION: a NaN w sets ALL SEVEN -- every test involves w");
}

/* ------------------------------------------------------------------ */
/* 0x1001EC30 / 0x1001CF30 -- G_SETTILESIZE                            */
/* ------------------------------------------------------------------ */

static void test_tilesize(void)
{
    BrDlsTileSize t;

    printf("tile size (0x1001EC30 glide / 0x1001CF30 d3d)\n");

    /* uls = 4, ult = 8, lrs = 0x40, lrt = 0x80, all in 10.2. */
    BrDlsTileSizeDecode(0x00004008u, 0x00040080u, &t);
    check(t.uls == 4 && t.ult == 8, "w0 carries uls in the high field, ult in the low");
    check(t.lrs == 0x40 && t.lrt == 0x80, "w1 carries lrs and lrt the same way");
    check(t.tileW == ((0x40 - 4 + 4) >> 2), "tileW == (lrs - uls + 4) >> 2");
    check(t.tileH == ((0x80 - 8 + 4) >> 2), "tileH == (lrt - ult + 4) >> 2");

    /* THE SIGN FOLD. 0xFF8 is -8, not 4088: `cmp 0x800 / jl / sub 0x1000`.
     * And the branch is `jl`, so 0x800 itself IS folded, to -2048. */
    BrDlsTileSizeDecode(0xFF8FF8u, 0x0u, &t);
    check(t.uls == -8 && t.ult == -8, "a 12-bit 0xFF8 is -8, not 4088");
    BrDlsTileSizeDecode(0x800800u, 0x7FF7FFu, &t);
    check(t.uls == -2048 && t.ult == -2048, "0x800 folds to -2048 (`jl`, not `jle`)");
    check(t.lrs == 2047 && t.lrt == 2047, "0x7FF stays 2047 -- the top of the range");

    /* THE SHIFT IS ARITHMETIC. lrs -2048 against uls 2047 gives -4091, and
     * `sar 2` rounds toward -inf: -1023, not -1022. An unsigned shift would
     * produce something around a billion. */
    BrDlsTileSizeDecode(0x7FF7FFu, 0x800800u, &t);
    check(t.tileW == -1023 && t.tileH == -1023,
          "a back-to-front tile gives a NEGATIVE span, rounded toward -inf");
}

/* ------------------------------------------------------------------ */
/* 0x10021570 + 0x10021510 (0xE4) and 0x100219D0 + 0x10021B80 (0xE3)   */
/* ------------------------------------------------------------------ */

static void test_tilerect(void)
{
    BrDlsTileRect a, b;

    printf("tile rects (0xE4 and 0xE3, both builds)\n");

    /* Corner assignment: w1 -> upper-left (arguments 0 and 1), w0 -> lower-
     * right (arguments 2 and 3), tile from w1 bits 26:24. Read off the push
     * order at 0x1002159E..0x100215AD, last push first.
     *
     *   w0 = (lrx << 12) | lry,  w1 = (tile << 24) | (ulx << 12) | uly       */
    BrDlsTileRectDecode(0x00019032u, 0x0300A014u, 0, &a);
    check(a.lrx == 0x019 && a.lry == 0x032, "0xE4: w0 is the LOWER-RIGHT pair");
    check(a.ulx == 0x00A && a.uly == 0x014, "0xE4: w1 is the UPPER-LEFT pair");
    check(a.tile == 3, "0xE4: the tile is w1 bits 26:24");

    /* THE SCALING, AND IT IS THE ONE THIS TREE HAD BACKWARDS.
     *
     * 0xE4 shifts NOTHING: 0x1002157E is `and eax,0xFFF` and there is no
     * shift anywhere in the function. br_dl.c used to shift these RIGHT by
     * two, which is a quarter of the intended coordinate.
     *
     * 0xE3 shifts LEFT by two: `shl edx,2` at 0x100219EE and 0x10021A04, and
     * `shr ecx,0xA / and 0x3FFC` at 0x100219EB, which is the same multiply
     * folded into the shift. br_dl.c used to leave these alone. */
    BrDlsTileRectDecode(0x00019032u, 0x0300A014u, 0, &a);
    check(a.ulx == 0x00A, "MUTATION: 0xE4 passes its corners through UNSCALED");
    BrDlsTileRectDecode(0x00019032u, 0x0300A014u, 1, &b);
    check(b.ulx == 0x00A * 4, "MUTATION: 0xE3 multiplies its corners by FOUR");
    check(b.uly == 0x014 * 4 && b.lrx == 0x019 * 4 && b.lry == 0x032 * 4,
          "...all four of them, not just one");
    check(b.tile == a.tile, "and the tile is not scaled");

    /* The relation that makes the two forms one function: 0xE3's whole-pixel
     * fields and 0xE4's quarter-pixel fields describe the same rectangle when
     * 0xE3's are a quarter the size. Feed it exactly that and the two must
     * agree, every field. A tree that scales the wrong one fails here even if
     * both individual checks above were written to match it. */
    BrDlsTileRectDecode(0x000640C8u, 0x03028050u, 0, &a);   /* 10.2 fields */
    BrDlsTileRectDecode(0x00019032u, 0x0300A014u, 1, &b);   /* a quarter    */
    check(a.ulx == b.ulx && a.uly == b.uly &&
          a.lrx == b.lrx && a.lry == b.lry,
          "0xE4 at 4x and 0xE3 at 1x land on the same quarter-pixel corners");

    /* The two command sizes, from the returns: 0xE4 leaves esi at cmd+0x10
     * and then adds 8; 0xE3 returns esi+8. */
    check(BR_DLS_TILERECT_E4_BYTES == 0x18, "0xE4 consumes three commands");
    check(BR_DLS_TILERECT_E3_BYTES == 0x08, "0xE3 consumes one");
    check(BR_DLS_SKIP_BYTES == 8, "the default handler steps one command");
}

int main(void)
{
    test_clipcodes();
    test_tilesize();
    test_tilerect();
    printf(g_fail ? "\n1 failures\n" : "\nALL PASSED\n");
    return g_fail;
}
