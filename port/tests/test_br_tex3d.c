/* test_br_tex3d.c -- the load-time texture pass (br_tex3d.h).
 *
 * WHAT IS ASSERTED, AND WHY IT IS NOT A PIXEL COUNT
 * ----------------------------------------------------------------------
 * CONVENTIONS.md is explicit that a test which encodes an expectation
 * rather than a property of the code is worse than none.  So the assertions
 * here are identities and round-trips of the transcription itself:
 *
 *   - 0x100271F0 is a ROTATION, so it is a bijection on the 16-bit space
 *     and rotating the other way undoes it.  Checked over all 65536.
 *   - the odd-row swizzle is `i ^ 4`, which is an INVOLUTION, so decoding a
 *     two-row image whose source rows are equal must yield two rows that
 *     are each other's word swap -- and doing it again returns the first.
 *   - the seam's arithmetic has an exact consistency requirement: the 0xDC
 *     handler returns `p + 8*w1`, so `8*w1` must land precisely on the
 *     command that FOLLOWS the run.  If it does not, the interpreter either
 *     re-executes setup commands or eats geometry.
 *   - the dedup is idempotent: the same run twice is one record.
 *   - and the negative: a list with no texture commands must plant nothing.
 *     ("A path that has never executed is not tested" cuts both ways --
 *     a pass that plants a bind where the data has none is worse.)
 */
#include "br_tex3d.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_fail;

static void check(int cond, const char *pszWhat)
{
    printf("  [%s] %s\n", cond ? "PASS" : "FAIL", pszWhat);
    if (!cond)
        g_fail = 1;
}

/* Host order, as BrDlPatch leaves a list: little-endian words, opcode in
 * byte 3. */
static void put8(uint8_t *p, uint32_t w0, uint32_t w1)
{
    p[0] = (uint8_t)w0;        p[1] = (uint8_t)(w0 >> 8);
    p[2] = (uint8_t)(w0 >> 16); p[3] = (uint8_t)(w0 >> 24);
    p[4] = (uint8_t)w1;        p[5] = (uint8_t)(w1 >> 8);
    p[6] = (uint8_t)(w1 >> 16); p[7] = (uint8_t)(w1 >> 24);
}

static uint32_t dlw(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* 0x100295B0's two words, from the fields it decodes. */
static uint32_t settile_w0(int fmt, int siz, int lineBytes, int tmem)
{
    return 0xF5000000u | ((uint32_t)fmt << 21) | ((uint32_t)siz << 19) |
           (((uint32_t)(lineBytes >> 3) & 0x1FFu) << 9) |
           ((uint32_t)tmem & 0x1FFu);
}

static uint32_t settile_w1(int tile, int maskS, int maskT,
                           int mirrorS, int mirrorT)
{
    return ((uint32_t)tile << 24) | ((uint32_t)maskT << 14) |
           ((uint32_t)maskS << 4) | ((uint32_t)mirrorS << 8) |
           ((uint32_t)mirrorT << 18);
}

/* One complete texture-setup run, in the order testdata/ce.rca uses it,
 * followed by a G_VTX to close it.  Returns the command count. */
static int emit_run(uint8_t *p, uint32_t texels, uint32_t tlut,
                    uint32_t tileW0, uint32_t tileW1)
{
    int n = 0;
    put8(p + 8 * n++, 0xFD100000u, texels);         /* SETTIMG(texels)   */
    put8(p + 8 * n++, 0xE6000000u, 0u);             /* LOADSYNC          */
    put8(p + 8 * n++, 0xF3000000u, 0u);             /* LOADBLOCK         */
    if (tlut != 0) {
        put8(p + 8 * n++, 0xFD100000u, tlut);       /* SETTIMG(tlut)     */
        put8(p + 8 * n++, 0xF0000000u, 0x0603C000u);/* LOADTLUT          */
    }
    put8(p + 8 * n++, 0xE7000000u, 0u);             /* PIPESYNC          */
    put8(p + 8 * n++, 0xE8000000u, 0u);             /* TILESYNC          */
    put8(p + 8 * n++, tileW0, tileW1);              /* SETTILE           */
    put8(p + 8 * n++, 0xF2000000u, 0x000FE0FEu);    /* SETTILESIZE       */
    return n;
}

/* ------------------------------------------------------------------ */
static void test_texel_rotation(void)
{
    /* The conversion is (v>>1) | ((v&1)<<15) on the BIG-endian halfword,
     * i.e. a one-bit rotate right.  Exercised through the public decoder on
     * a 1x1 RGBA16 texture, which is the only way in from outside. */
    static uint8_t dl[8 * 16];
    static uint8_t src[2];
    BrTex3d tex;
    uint32_t v;
    int n, cWrong = 0, cAliased = 0;
    unsigned char seen[8192];

    printf("texel rotation (0x100271F0)\n");
    memset(seen, 0, sizeof(seen));
    BrTex3dInit(&tex);

    n = emit_run(dl, 0x80000000u, 0u,
                 settile_w0(0, 2, 8, 0), settile_w1(0, 0, 0, 0, 0));
    put8(dl + 8 * n++, 0x04000000u, 0u);            /* G_VTX: the seam   */
    put8(dl + 8 * n++, 0xB8000000u, 0u);            /* G_ENDDL           */
    BrTex3dScan(&tex, dl, (size_t)n * 8u);

    for (v = 0; v < 0x10000u; v++) {
        uint16_t out = 0;
        uint32_t back;
        src[0] = (uint8_t)(v >> 8);                 /* N64 halfwords are  */
        src[1] = (uint8_t)v;                        /* big-endian         */
        if (BrTex3dDecode(&tex, 0, src, sizeof(src), NULL, 0, &out) !=
            BR_TEX3D_OK) {
            cWrong++;
            break;
        }
        /* rotate left by one undoes it */
        back = ((uint32_t)out << 1) | ((uint32_t)out >> 15);
        if ((back & 0xFFFFu) != v)
            cWrong++;
        if (seen[out >> 3] & (unsigned char)(1u << (out & 7)))
            cAliased++;
        seen[out >> 3] |= (unsigned char)(1u << (out & 7));
    }
    check(cWrong == 0, "rotating back recovers every one of the 65536 inputs");
    check(cAliased == 0, "the conversion is injective (it is a rotation)");
    BrTex3dFree(&tex);
}

/* ------------------------------------------------------------------ */
static void test_row_swizzle(void)
{
    /* An 8x2 RGBA16 image whose two SOURCE rows are byte-identical.  Row 0
     * is read linearly and row 1 through `i ^ 4`, so the two decoded rows
     * must be each other's 32-bit word swap -- and applying that swap twice
     * must return the original, which is the involution the expander's
     * 4-forward/8-back cursor implements. */
    static uint8_t dl[8 * 16];
    static uint8_t src[32];
    static uint16_t out[16];
    BrTex3d tex;
    int n, i, cMismatch = 0, cInvolution = 0;

    printf("odd-row word swap (0x1002543E / 0x10026FBD)\n");
    BrTex3dInit(&tex);
    for (i = 0; i < 16; i++) {                      /* texel i == i       */
        src[i * 2 + 0] = 0;
        src[i * 2 + 1] = (uint8_t)(i & 7);
    }

    n = emit_run(dl, 0x80000000u, 0u,
                 settile_w0(0, 2, 16, 0), settile_w1(0, 3, 1, 0, 0));
    put8(dl + 8 * n++, 0x04000000u, 0u);
    put8(dl + 8 * n++, 0xB8000000u, 0u);
    BrTex3dScan(&tex, dl, (size_t)n * 8u);

    check(tex.cRec == 1 && tex.aRec[0].w == 8 && tex.aRec[0].h == 2,
          "1<<maskS by 1<<maskT is the registered size");
    check(BrTex3dDecode(&tex, 0, src, sizeof(src), NULL, 0, out) ==
          BR_TEX3D_OK, "BrTex3dDecode (RGBA16)");

    for (i = 0; i < 8; i++) {
        /* row1[i] is row0 read with the byte swizzle: two texels per
         * 4-byte word, so the texel permutation is i ^ 2. */
        if (out[8 + i] != out[i ^ 2])
            cMismatch++;
        if (out[8 + (i ^ 2)] != out[i])
            cInvolution++;
    }
    check(cMismatch == 0,
          "the odd row is the even row permuted by the word swap");
    check(cInvolution == 0, "applying the swap twice is the identity");
    BrTex3dFree(&tex);
}

/* ------------------------------------------------------------------ */
static void test_ci4_and_mirror(void)
{
    static uint8_t dl[8 * 16];
    static uint8_t texels[64];
    static uint8_t pal[32];
    static uint16_t out[256];
    BrTex3d tex;
    int n, i, cBad = 0;

    printf("CI4, the palette, and the mirror\n");
    BrTex3dInit(&tex);

    /* 8x4 CI4: one byte holds two texels, HIGH nibble first, so a row is
     * four bytes and `line` is four. */
    for (i = 0; i < 16; i++)
        texels[i] = (uint8_t)((((2 * i) & 0xF) << 4) | ((2 * i + 1) & 0xF));
    for (i = 0; i < 16; i++) {                      /* palette[i] == i    */
        pal[i * 2 + 0] = 0;
        pal[i * 2 + 1] = (uint8_t)i;
    }

    n = emit_run(dl, 0x80000000u, 0x80001000u,
                 settile_w0(2, 0, 4, 0), settile_w1(0, 3, 2, 0, 0));
    put8(dl + 8 * n++, 0x04000000u, 0u);
    put8(dl + 8 * n++, 0xB8000000u, 0u);
    BrTex3dScan(&tex, dl, (size_t)n * 8u);

    check(tex.cRec == 1 && tex.aRec[0].w == 8 && tex.aRec[0].h == 4,
          "CI4 8x4 registered");
    check(tex.aRec[0].palSrc == 0x80001000u && tex.aRec[0].texelSrc == 0x80000000u,
          "LOADBLOCK took the FIRST image address and LOADTLUT the second");
    check(BrTex3dDecode(&tex, 0, texels, sizeof(texels), pal, sizeof(pal),
                        out) == BR_TEX3D_OK, "BrTex3dDecode (CI4)");
    /* Row 0 is linear: texel x is nibble x of the row, palette[x], and the
     * palette entry is put through the same rotation. */
    for (i = 0; i < 8; i++) {
        uint32_t want = (uint32_t)(i & 0xF);
        want = (want >> 1) | ((want & 1u) << 15);
        if (out[i] != (uint16_t)want)
            cBad++;
    }
    check(cBad == 0, "CI4 splits each byte high nibble first and indexes the TLUT");

    /* The same tile with the S mirror set must be twice as wide, and the
     * second half must be the first reversed -- 0x10025680 emits the row it
     * has just written, backwards. */
    BrTex3dFree(&tex);
    BrTex3dInit(&tex);
    n = emit_run(dl, 0x80000000u, 0x80001000u,
                 settile_w0(2, 0, 4, 0), settile_w1(0, 3, 2, 1, 0));
    put8(dl + 8 * n++, 0x04000000u, 0u);
    put8(dl + 8 * n++, 0xB8000000u, 0u);
    BrTex3dScan(&tex, dl, (size_t)n * 8u);
    check(tex.cRec == 1 && tex.aRec[0].w == 16 && tex.aRec[0].h == 4,
          "the mirror bit doubles the registered width");
    check(BrTex3dDecode(&tex, 0, texels, sizeof(texels), pal, sizeof(pal),
                        out) == BR_TEX3D_OK, "BrTex3dDecode (CI4 mirrored)");
    cBad = 0;
    for (i = 0; i < 8; i++)
        if (out[8 + i] != out[7 - i])
            cBad++;
    check(cBad == 0, "the mirrored half is the row reversed");
    BrTex3dFree(&tex);
}

/* ------------------------------------------------------------------ */
static void test_plant_and_dedup(void)
{
    static uint8_t dl[8 * 64];
    BrTex3d tex;
    int n, iRun1, iRun2, nRun;
    uint32_t w0, w1;

    printf("the seam (0x10028B50) and the dedup (0x10027A70)\n");
    BrTex3dInit(&tex);

    memset(dl, 0, sizeof(dl));
    n = 0;
    iRun1 = n;
    n += emit_run(dl + 8 * n, 0x80000000u, 0x80001000u,
                  settile_w0(2, 0, 4, 0), settile_w1(0, 3, 2, 0, 0));
    nRun = n - iRun1;
    put8(dl + 8 * n++, 0x04000000u, 0u);            /* closes run 1       */
    iRun2 = n;
    n += emit_run(dl + 8 * n, 0x80000000u, 0x80001000u,
                  settile_w0(2, 0, 4, 0), settile_w1(0, 3, 2, 0, 0));
    put8(dl + 8 * n++, 0x04000000u, 0u);            /* closes run 2       */
    /* a third run, same tile but a different texel source */
    n += emit_run(dl + 8 * n, 0x80002000u, 0x80001000u,
                  settile_w0(2, 0, 4, 0), settile_w1(0, 3, 2, 0, 0));
    put8(dl + 8 * n++, 0x04000000u, 0u);
    put8(dl + 8 * n++, 0xB8000000u, 0u);

    check(BrTex3dScan(&tex, dl, (size_t)n * 8u) == (size_t)n,
          "the scan walked the whole list and stopped at G_ENDDL");
    check(tex.cRuns == 3 && tex.cPlanted == 3, "three runs, three binds planted");
    check(tex.cRec == 2,
          "the two identical runs deduped and the third did not");

    w0 = dlw(dl + 8 * iRun1);
    w1 = dlw(dl + 8 * iRun1 + 4);
    check((w0 >> 24) == 0xDCu, "the run's first command became 0xDC");
    check((w0 & 0x00FFFFFFu) == 0u, "and carries texture id 0");
    /* THE CONSISTENCY REQUIREMENT: 0xDC returns p + 8*w1, so 8*w1 must land
     * exactly on the command after the run -- here, the G_VTX. */
    check(w1 == (uint32_t)nRun, "w1 is the run length in 8-byte commands");
    check(dlw(dl + 8 * iRun1 + (size_t)w1 * 8u) >> 24 == 0x04u,
          "p + 8*w1 lands exactly on the command that follows the run");

    check((dlw(dl + 8 * iRun2) & 0x00FFFFFFu) == 0u,
          "the duplicate run binds the SAME id");
    BrTex3dFree(&tex);
}

/* ------------------------------------------------------------------ */
static void test_no_texture_no_bind(void)
{
    static uint8_t dl[8 * 8];
    BrTex3d tex;
    int n = 0, i, cDc = 0;

    printf("a list with no texture setup\n");
    BrTex3dInit(&tex);
    put8(dl + 8 * n++, 0x04000000u, 0x40000000u);
    put8(dl + 8 * n++, 0xBF000000u, 0x00000204u);
    put8(dl + 8 * n++, 0xFC127FFFu, 0xFFFFF238u);
    put8(dl + 8 * n++, 0xB8000000u, 0u);
    BrTex3dScan(&tex, dl, (size_t)n * 8u);

    for (i = 0; i < n; i++)
        if ((dlw(dl + 8 * i) >> 24) == 0xDCu)
            cDc++;
    check(cDc == 0 && tex.cPlanted == 0 && tex.cRec == 0,
          "nothing planted where the data has no texture");
    BrTex3dFree(&tex);
}

/* ------------------------------------------------------------------ */
static void test_unsupported_is_refused(void)
{
    /* IA8 (fmt 3, siz 1) is one of the twenty-odd arms of 0x100250D0 that
     * have not been transcribed.  The decoder must SAY so; producing
     * plausible pixels for a format nobody read is exactly the failure
     * br_dl.h records three times over. */
    static uint8_t dl[8 * 16];
    static uint8_t src[64];
    static uint16_t out[64];
    BrTex3d tex;
    int n;

    printf("an untranscribed format\n");
    BrTex3dInit(&tex);
    n = emit_run(dl, 0x80000000u, 0u,
                 settile_w0(3, 1, 8, 0), settile_w1(0, 3, 3, 0, 0));
    put8(dl + 8 * n++, 0x04000000u, 0u);
    put8(dl + 8 * n++, 0xB8000000u, 0u);
    BrTex3dScan(&tex, dl, (size_t)n * 8u);
    check(tex.cRec == 1, "an unsupported format still REGISTERS (it must: "
                         "the 0xDC id is an array index)");
    check(BrTex3dDecode(&tex, 0, src, sizeof(src), NULL, 0, out) ==
          BR_TEX3D_UNSUPPORTED, "and the decoder refuses rather than guesses");
    BrTex3dFree(&tex);
}

int main(void)
{
    test_texel_rotation();
    test_row_swizzle();
    test_ci4_and_mirror();
    test_plant_and_dedup();
    test_no_texture_no_bind();
    test_unsupported_is_refused();
    printf(g_fail ? "\nFAILED\n" : "\nALL PASSED\n");
    return g_fail;
}
