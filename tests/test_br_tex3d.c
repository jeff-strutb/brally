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
    /* IA8 (fmt 3, siz 1) and I4 (fmt 4, siz 0) leave 0x100250D0 as EIGHT-BIT
     * texels.  Both arms are transcribed (BrTex3dExpand), but this entry
     * point hands back a uint16_t buffer, so it must SAY it cannot carry
     * them; truncating or widening them to look plausible is exactly the
     * failure br_dl.h records three times over.  Reach them through
     * BrTex3dExpand -- test_expander_byte_arms below does.
     *
     * THESE TWO ARE NOT HYPOTHETICAL.  A census of every G_SETTILE in the
     * canonical texture-load idiom across all nineteen shipped assets (see
     * br_dl.h) finds RGBA16 1155, CI4 277, I4 32, IA8 16 and CI8 zero, and
     * testdata/tracks/desert.trk alone carries five real textures in these
     * two formats -- IA8 64x32 and 64x64, I4 16x16 and two 32x8.  So this
     * test guards a LIVE gap, not a theoretical one, and the thing it must
     * catch is somebody teaching br_tex3d_bpp the sizes (which are not in
     * doubt: 8 and 4) without transcribing the conversion arms, which would
     * silently route them into the 16-bit path. */
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
          BR_TEX3D_UNSUPPORTED,
          "IA8 -- present in desert.trk -- is refused, not guessed");
    BrTex3dFree(&tex);

    /* and the same for I4, the other format the census found in the tracks */
    BrTex3dInit(&tex);
    n = emit_run(dl, 0x80000000u, 0u,
                 settile_w0(4, 0, 1, 0), settile_w1(0, 4, 4, 0, 0));
    put8(dl + 8 * n++, 0x04000000u, 0u);
    put8(dl + 8 * n++, 0xB8000000u, 0u);
    BrTex3dScan(&tex, dl, (size_t)n * 8u);
    check(tex.cRec == 1, "an I4 tile registers too");
    check(BrTex3dDecode(&tex, 0, src, sizeof(src), NULL, 0, out) ==
          BR_TEX3D_UNSUPPORTED,
          "I4 -- also present in desert.trk -- is refused, not guessed");
    BrTex3dFree(&tex);
}

/* ==================================================================== */
/* 0x100250D0 whole -- the six arms BrTex3dDecode's 16-bit buffer cannot */
/* express, reached through BrTex3dExpand directly.                     */
/* ==================================================================== */

/* THE TWO DIVIDE IDIOMS, WRITTEN AS THE INSTRUCTIONS WRITE THEM.  The
 * expander divides by 255 in two places and NOT the same way, which is the
 * single easiest thing to harmonise away by accident:
 *
 *   0x1002585B  mov eax,0x80808081 / imul ecx / add edx,ecx / sar edx,7
 *               / mov eax,edx / shr eax,31 / add edx,eax      -- SIGNED
 *   0x10026747  mov eax,0x80808081 / mul edx  / shr edx,7     -- UNSIGNED
 *
 * These two helpers are those sequences, and the first assertion below is
 * that each agrees with the C operator the transcription uses.  Everything
 * after that is entitled to say "/ 255" and "/ 255u". */
static int32_t magic_div255_signed(int32_t v)
{
    int32_t hi = (int32_t)(((int64_t)(int32_t)0x80808081 * (int64_t)v) >> 32);
    int32_t q  = (hi + v) >> 7;
    return q + (int32_t)((uint32_t)q >> 31);
}

static uint32_t magic_div255_unsigned(uint32_t v)
{
    return (uint32_t)((0x80808081ull * (uint64_t)v) >> 32) >> 7;
}

/* A one-level tile, filled the way 0x100295B0 fills one. */
static void mktile(BrTex3dTile *pT, int fmt, int siz, int maskS, int maskT,
                   int lineBytes, int mirrorS, int mirrorT)
{
    memset(pT, 0, sizeof(*pT));
    pT->fmt = fmt; pT->siz = siz;
    pT->maskS = maskS; pT->maskT = maskT;
    pT->line = lineBytes;
    pT->mirrorS = mirrorS; pT->mirrorT = mirrorT;
}

/* ------------------------------------------------------------------ */
static void test_divide_idioms(void)
{
    int32_t v;
    int cS = 0, cU = 0;

    printf("the two /255 idioms (0x1002585B signed, 0x10026747 unsigned)\n");
    for (v = -70000; v <= 70000; v += 7) {
        if (magic_div255_signed(v) != v / 255) cS++;
        if (magic_div255_unsigned((uint32_t)v) != (uint32_t)v / 255u) cU++;
    }
    check(cS == 0, "imul 0x80808081 / sar 7 / +sign IS C's signed / 255");
    check(cU == 0, "mul 0x80808081 / shr 7 IS C's unsigned / 255u");
    check(magic_div255_signed(-4335) != (int32_t)magic_div255_unsigned(
              (uint32_t)-4335),
          "and they DISAGREE on a negative numerator -- so which arm uses "
          "which is a real distinction, not a style");
}

/* ------------------------------------------------------------------ */
static void test_expander_byte_arms(void)
{
    /* The three arms whose output is one BYTE per texel.  Each is defined by
     * a nibble identity read off the listing, and each identity is checkable
     * without knowing the answer in advance:
     *
     *   I4   (0x1002624D)  b -> (b&0xF0)|(b>>4), then ((b<<4)|(b&0xF))
     *                      -- both output bytes have EQUAL nibbles, and each
     *                      equals the source nibble it came from.
     *   AI44 (0x10026C04)  b -> (b<<4)|(b>>4): an INVOLUTION, so expanding
     *                      the output again returns the input.
     *   I8   (0x10026DF9)  a plain copy.
     */
    BrTex3dTile t;
    static const uint8_t src[4] = { 0x3A, 0xF0, 0x0F, 0x51 };
    uint8_t out[64], again[64];
    int i, cNib = 0, cVal = 0;

    printf("the byte-output arms (I4, AI44, I8)\n");

    /* --- I4: 4bpp, fmt 4, mode != 1.  maskS 3 => 4 source bytes per row. */
    mktile(&t, 4, 0, 3, 0, 4, 0, 0);
    memset(out, 0xCC, sizeof(out));
    BrTex3dExpand(out, 8, 0, src, NULL, 4, 0, 0, 0, 1, &t, 0, 0,
                  0, 0, 0, 0, 0, 0, 0, 0, 0);
    for (i = 0; i < 4; i++) {
        uint8_t hi = out[i * 2 + 0], lo = out[i * 2 + 1];
        if ((hi >> 4) != (hi & 0xF) || (lo >> 4) != (lo & 0xF)) cNib++;
        if ((hi >> 4) != (src[i] >> 4) || (lo >> 4) != (src[i] & 0xF)) cVal++;
    }
    check(cNib == 0, "I4 replicates the nibble into both halves of the byte");
    check(cVal == 0, "...the high source nibble first, then the low one");
    check(out[8] == 0xCC, "and it stops at the byte budget");

    /* --- AI44: 8bpp, fmt 3, mode != 1.  An involution. */
    mktile(&t, 3, 1, 2, 0, 4, 0, 0);
    BrTex3dExpand(out, 4, 1, src, NULL, 3, 0, 0, 0, 1, &t, 0, 0,
                  0, 0, 0, 0, 0, 0, 0, 0, 0);
    BrTex3dExpand(again, 4, 1, out, NULL, 3, 0, 0, 0, 1, &t, 0, 0,
                  0, 0, 0, 0, 0, 0, 0, 0, 0);
    check(memcmp(again, src, 4) == 0,
          "AI44 is a nibble swap, so doing it twice is the identity");
    check(out[0] == 0xA3 && out[1] == 0x0F && out[2] == 0xF0 && out[3] == 0x15,
          "...and it really did swap them");

    /* --- I8: 8bpp, fmt 4.  A copy. */
    mktile(&t, 4, 1, 2, 0, 4, 0, 0);
    BrTex3dExpand(out, 4, 1, src, NULL, 4, 0, 0, 0, 1, &t, 0, 0,
                  0, 0, 0, 0, 0, 0, 0, 0, 0);
    check(memcmp(out, src, 4) == 0, "I8 copies the byte through");
}

/* ------------------------------------------------------------------ */
static void test_expander_blend_arms(void)
{
    /* The two arms that mix two colours by the texel's brightness.  Rather
     * than quote a constant, each expectation is rebuilt here from the
     * instruction sequence -- the divide oracles above plus the shift and
     * pack the listing performs -- so an implementation that gets either
     * wrong disagrees. */
    BrTex3dTile t;
    static const uint8_t src[2] = { 0x8F, 0x00 };
    uint8_t raw[16];
    uint16_t out[8];
    int32_t hi[4], lo[4];
    int i, k, cBad = 0;

    printf("the blend arms (I4 -> ARGB1555, IA8 -> ARGB4444)\n");

    /* --- I4 blend, 4bpp fmt 4 mode 1, hi = white, lo = black.  The lerp is
     * then the identity, so the packed word is the intensity itself split
     * 1/5/5/5. */
    hi[0] = hi[1] = hi[2] = hi[3] = 255;
    lo[0] = lo[1] = lo[2] = lo[3] = 0;
    mktile(&t, 4, 0, 1, 0, 2, 0, 0);
    BrTex3dExpand(raw, 4, 0, src, NULL, 4, 0, 0, 0, 1, &t, 0, 1,
                  hi[0], hi[1], hi[2], hi[3], lo[0], lo[1], lo[2], lo[3], 0);
    memcpy(out, raw, 4);
    for (i = 0; i < 2; i++) {
        uint32_t b = src[0];
        int32_t  n = (i == 0) ? (int32_t)((b >> 4) | (b & 0xF0u))
                              : (int32_t)(((b << 4) | (b & 0xFu)) & 0xFFu);
        uint32_t v = (uint32_t)((n >> 7) & 0xFF);
        for (k = 0; k < 3; k++)
            v = (v << 5) | (uint32_t)((n >> 3) & 0xFF);
        if (out[i] != (uint16_t)v) cBad++;
    }
    check(cBad == 0, "I4 blend packs alpha>>7 then three channels>>3");
    check(out[1] == 0xFFFFu,
          "...so a full-intensity texel against white is all ones");

    /* THE ALPHA SHIFT NEEDS AN ASYMMETRIC HIGH COLOUR TO BE VISIBLE AT ALL.
     * Against a white high colour the blended channel is the replicated
     * nibble n = 17k, and bit 3 of 17k IS bit 7 of 17k -- the low nibble
     * contributes bits 0..3 and the high nibble bits 4..7, so the two are
     * the same bit of k.  Only the lowest bit of the alpha field survives
     * the 16-bit store, so `>> 3` and `>> 7` agree on EVERY texel of that
     * family and a test built on it cannot see the difference (checked: it
     * did not).  With alpha's high colour at 16 the quotient is 8, whose
     * bit 3 is set and bit 7 is not, and the two readings part company. */
    hi[0] = hi[1] = hi[2] = 255; hi[3] = 16;
    lo[0] = lo[1] = lo[2] = lo[3] = 0;
    BrTex3dExpand(raw, 4, 0, src, NULL, 4, 0, 0, 0, 1, &t, 0, 1,
                  hi[0], hi[1], hi[2], hi[3], lo[0], lo[1], lo[2], lo[3], 0);
    memcpy(out, raw, 4);
    check((out[0] & 0x8000u) == 0,
          "the alpha field is ch>>7, not ch>>3 -- 136*16/255 == 8 is opaque "
          "under the wrong shift and transparent under the right one");

    /* CHANNEL ORDER needs an ASYMMETRIC colour for the same reason: against
     * a grey high colour, permuting the three fields is invisible.  Argument
     * 14 is the field the listing shifts furthest left after alpha, so it is
     * red; drive it alone and the other two must stay zero. */
    hi[0] = 255; hi[1] = hi[2] = hi[3] = 0;
    lo[0] = lo[1] = lo[2] = lo[3] = 0;
    BrTex3dExpand(raw, 4, 0, src, NULL, 4, 0, 0, 0, 1, &t, 0, 1,
                  hi[0], hi[1], hi[2], hi[3], lo[0], lo[1], lo[2], lo[3], 0);
    memcpy(out, raw, 4);
    check(out[0] == (uint16_t)(17u << 10),
          "argument 14 is RED: it lands in bits 14..10, and the other two "
          "channels stay zero");

    /* --- the SIGNED divide shows only when hi < lo.  hi = 0, lo = 255 makes
     * the delta -255, and (n * -255) / 255 == -n exactly under the signed
     * idiom.  The unsigned idiom would give 16842873 for n = 136. */
    hi[0] = hi[1] = hi[2] = hi[3] = 0;
    lo[0] = lo[1] = lo[2] = lo[3] = 255;
    BrTex3dExpand(raw, 4, 0, src, NULL, 4, 0, 0, 0, 1, &t, 0, 1,
                  hi[0], hi[1], hi[2], hi[3], lo[0], lo[1], lo[2], lo[3], 0);
    memcpy(out, raw, 4);
    {
        int32_t n = (int32_t)((src[0] >> 4) | (src[0] & 0xF0u));   /* 136 */
        int32_t ch = 255 + magic_div255_signed(n * -255);          /* 119 */
        uint32_t v = (uint32_t)((ch >> 7) & 0xFF);
        for (k = 0; k < 3; k++)
            v = (v << 5) | (uint32_t)((ch >> 3) & 0xFF);
        check(out[0] == (uint16_t)v && out[0] == 0x39CEu,
              "I4 blend divides SIGNED, so an inverted ramp stays in range");
    }

    /* --- IA8 blend, 8bpp fmt 3 mode 1.  Alpha is the source's own low
     * nibble and is NOT blended; the three colour channels are >>4. */
    hi[0] = hi[1] = hi[2] = 255; hi[3] = 0;
    lo[0] = lo[1] = lo[2] = 0;   lo[3] = 0;
    mktile(&t, 3, 1, 1, 0, 2, 0, 0);
    BrTex3dExpand(raw, 4, 1, src, NULL, 3, 0, 0, 0, 1, &t, 0, 1,
                  hi[0], hi[1], hi[2], hi[3], lo[0], lo[1], lo[2], lo[3], 0);
    memcpy(out, raw, 4);
    check(out[0] == 0xF888u,
          "IA8 blend takes alpha from the texel's low nibble unmixed");

    /* --- and the UNSIGNED divide, which is the same experiment the other
     * way round.  n = 17, delta = -255: the unsigned idiom's quotient is
     * enormous and the nibbles bleed into each other, which the signed one
     * (0x0EEE) does not do.  The value is what the instructions produce. */
    hi[0] = hi[1] = hi[2] = 0; lo[0] = lo[1] = lo[2] = 255;
    {
        static const uint8_t one[1] = { 0x10 };
        uint32_t n = (uint32_t)((one[0] >> 4) | (one[0] & 0xF0u));   /* 17 */
        uint32_t v = one[0] & 0x0Fu;
        for (k = 0; k < 3; k++) {
            uint32_t prod = (uint32_t)((int32_t)(0u - 255u) * (int32_t)n);
            v = (v << 4) | ((255u + magic_div255_unsigned(prod)) >> 4);
        }
        mktile(&t, 3, 1, 0, 0, 1, 0, 0);
        BrTex3dExpand(raw, 2, 1, one, NULL, 3, 0, 0, 0, 1, &t, 0, 1,
                      hi[0], hi[1], hi[2], 0, lo[0], lo[1], lo[2], 0, 0);
        memcpy(out, raw, 2);
        check(out[0] == (uint16_t)v && out[0] == 0x1FFEu,
              "IA8 blend divides UNSIGNED -- the signed reading gives 0x0EEE");
    }
}

/* ------------------------------------------------------------------ */
static void test_expander_raw_index(void)
{
    /* 0x10025148.  With bit 1 of the descriptor flags set, LEVEL ONE of a CI4
     * texture is written out as the bare 4-bit index widened to a halfword --
     * no palette lookup at all.  Both halves of the guard are asserted,
     * because either alone would send the data through the palette. */
    BrTex3dTile a[2];
    static const uint8_t src[2] = { 0x3A, 0x51 };
    static const uint8_t pal[32] = { 0 };
    uint8_t raw[16];
    uint16_t out[8];

    printf("the raw-index arm (flags bit 1, level 1 only)\n");
    mktile(&a[0], 2, 0, 2, 0, 2, 0, 0);   /* 1<<(maskS-1) == 2 bytes */
    mktile(&a[1], 2, 0, 2, 0, 2, 0, 0);

    memset(raw, 0, sizeof(raw));
    BrTex3dExpand(raw, 8, 0, src, pal, 2, 0, 0, 1, 2, a, 2, 0,
                  0, 0, 0, 0, 0, 0, 0, 0, 0);
    memcpy(out, raw, 8);
    check(out[0] == 3 && out[1] == 0xA && out[2] == 5 && out[3] == 1,
          "level 1 with the flag emits the bare nibble as a halfword");

    /* the same call at level 0 goes through the (all-zero) palette instead */
    memset(raw, 0xFF, sizeof(raw));
    BrTex3dExpand(raw, 8, 0, src, pal, 2, 0, 0, 0, 1, a, 2, 0,
                  0, 0, 0, 0, 0, 0, 0, 0, 0);
    memcpy(out, raw, 8);
    check(out[0] == 0 && out[1] == 0 && out[2] == 0 && out[3] == 0,
          "level 0 does NOT take that arm even with the flag set");

    /* and level 1 without the flag likewise */
    memset(raw, 0xFF, sizeof(raw));
    BrTex3dExpand(raw, 8, 0, src, pal, 2, 0, 0, 1, 2, a, 0, 0,
                  0, 0, 0, 0, 0, 0, 0, 0, 0);
    memcpy(out, raw, 8);
    check(out[0] == 0 && out[1] == 0 && out[2] == 0 && out[3] == 0,
          "the flag alone decides it, not the level alone");
}

/* ------------------------------------------------------------------ */
static void test_expander_budget_and_lods(void)
{
    /* Two properties of the envelope rather than of any one arm.
     *
     * THE BUDGET IS TESTED AFTER THE STORE (`add edi,2 / cmp edi,ebp / jge`),
     * so a budget of zero still writes one element.  That is not a rounding
     * detail -- it is a one-element overrun the original really performs, and
     * a port that checks first would silently produce a shorter image for
     * every odd budget.
     *
     * THE LEVEL LOOP RUNS [lod, lodEnd) and the guard at 0x100250F5 is on the
     * first iteration, so an empty range writes nothing at all. */
    BrTex3dTile a[2];
    static const uint8_t src[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    uint8_t out[32];

    printf("the byte budget and the level loop\n");
    mktile(&a[0], 4, 1, 2, 0, 4, 0, 0);      /* I8, 4 bytes */
    mktile(&a[1], 4, 1, 2, 0, 4, 0, 0);

    memset(out, 0xCC, sizeof(out));
    BrTex3dExpand(out, 0, 1, src, NULL, 4, 0, 0, 0, 1, a, 0, 0,
                  0, 0, 0, 0, 0, 0, 0, 0, 0);
    check(out[0] == 1 && out[1] == 0xCC,
          "a zero budget still writes ONE element -- the test is after the "
          "store, and the original overruns by exactly that much");

    memset(out, 0xCC, sizeof(out));
    BrTex3dExpand(out, 3, 1, src, NULL, 4, 0, 0, 0, 1, a, 0, 0,
                  0, 0, 0, 0, 0, 0, 0, 0, 0);
    check(out[2] == 3 && out[3] == 0xCC, "a budget of 3 writes exactly 3");

    memset(out, 0xCC, sizeof(out));
    BrTex3dExpand(out, 32, 1, src, NULL, 4, 0, 0, 1, 1, a, 0, 0,
                  0, 0, 0, 0, 0, 0, 0, 0, 0);
    check(out[0] == 0xCC, "an empty level range writes nothing");

    memset(out, 0xCC, sizeof(out));
    BrTex3dExpand(out, 32, 1, src, NULL, 4, 0, 0, 0, 2, a, 0, 0,
                  0, 0, 0, 0, 0, 0, 0, 0, 0);
    check(out[4] == 1 && out[7] == 4,
          "two levels append into one buffer, each restarting from the "
          "texel base plus its own TMEM offset");
}

/* ------------------------------------------------------------------ */
static void test_expander_mirrors(void)
{
    /* 0x10025680 and 0x100256E8.  The S mirror re-emits the row backwards;
     * the T mirror re-emits the finished rows in reverse order.  Both are
     * asserted as reversals of what was just written, which is a property of
     * the code rather than a table of expected pixels. */
    BrTex3dTile t;
    static const uint8_t src[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    uint8_t out[64];
    int i, cS = 0, cT = 0;

    printf("the two mirrors\n");

    /* 4 texels wide, 2 rows, I8, S mirrored -> 8 texels per output row. */
    mktile(&t, 4, 1, 2, 1, 4, 1, 0);
    memset(out, 0xCC, sizeof(out));
    BrTex3dExpand(out, 16, 1, src, NULL, 4, 1, 0, 0, 1, &t, 0, 0,
                  0, 0, 0, 0, 0, 0, 0, 0, 0);
    for (i = 0; i < 4; i++) {
        if (out[4 + i] != out[3 - i]) cS++;
        if (out[12 + i] != out[11 - i]) cS++;
    }
    check(cS == 0, "the S mirror is the row just written, reversed");
    check(out[0] == 1 && out[8] == 5, "and the two rows came from `line`");

    /* the same without S, T mirrored -> 2 real rows then those 2 reversed */
    mktile(&t, 4, 1, 2, 1, 4, 0, 1);
    memset(out, 0xCC, sizeof(out));
    BrTex3dExpand(out, 16, 1, src, NULL, 4, 0, 1, 0, 1, &t, 0, 0,
                  0, 0, 0, 0, 0, 0, 0, 0, 0);
    for (i = 0; i < 4; i++) {
        if (out[8 + i] != out[4 + i]) cT++;      /* row 2 == row 1 */
        if (out[12 + i] != out[0 + i]) cT++;     /* row 3 == row 0 */
    }
    check(cT == 0, "the T mirror repeats the rows in reverse order");

    /* THE T MIRROR STEPS IN BYTES, and with a one-byte element that is
     * indistinguishable from stepping in elements.  Repeat it on a 16-bit
     * arm -- CI4 through an identity palette -- where the two differ, and
     * with S mirroring ON as well, which is what makes the step twice the
     * texel width. */
    {
        static uint8_t pal[32];
        static const uint8_t s4[2] = { 0x01, 0x23 };
        uint16_t o16[32];
        int cTW = 0;

        for (i = 0; i < 16; i++) {              /* pal[i] == i, big-endian */
            pal[i * 2 + 0] = 0;
            pal[i * 2 + 1] = (uint8_t)(i << 1); /* the rotate undoes the <<1 */
        }
        mktile(&t, 2, 0, 1, 1, 1, 1, 1);        /* 1<<(1-1) == 1 byte/row  */
        memset(out, 0xCC, sizeof(out));
        BrTex3dExpand(out, 32, 0, s4, pal, 2, 1, 1, 0, 1, &t, 0, 0,
                      0, 0, 0, 0, 0, 0, 0, 0, 0);
        memcpy(o16, out, 32);
        /* two source bytes, one per row; each row is 2 texels, S-mirrored
         * to 4, and T mirroring appends the two rows in reverse. */
        for (i = 0; i < 4; i++) {
            if (o16[8 + i] != o16[4 + i]) cTW++;
            if (o16[12 + i] != o16[0 + i]) cTW++;
        }
        check(cTW == 0,
              "the T mirror steps a whole output row in BYTES, including "
              "the width the S mirror added");
        check(o16[0] == 0 && o16[1] == 1 && o16[2] == 1 && o16[3] == 0,
              "...over rows the S mirror had already doubled");
    }
}

/* ------------------------------------------------------------------ */
static void test_rgba16_tests_the_row_bound_first(void)
{
    /* ONE ARM ORDERS ITS TWO EXIT TESTS DIFFERENTLY FROM THE OTHER EIGHT.
     * Every arm ends a texel with `cmp edi,ebp / jge <return>` and then
     * advances the column counter -- except the SWIZZLED RGBA16 loop, where
     * 0x10026FEE compares the column against the row width FIRST and jumps to
     * the row tail, and only the fall-through path at 0x10026FF4 checks the
     * budget.  So when the budget runs out on the very last texel of a
     * swizzled row, that arm still runs the row's S mirror (writing one more
     * element before it stops) where every other arm returns at once.
     *
     * It is one instruction's worth of difference and it survives every
     * assertion above, which is why it gets its own: an unwritten difference
     * is a difference nobody will preserve. */
    BrTex3dTile t;
    static uint8_t src[32];
    uint8_t out[80];
    uint16_t o16[40];
    int i;

    printf("the RGBA16 swizzled loop's exit order (0x10026FEE)\n");
    for (i = 0; i < 16; i++) {                  /* texel i == i+1 */
        src[i * 2 + 0] = 0;
        src[i * 2 + 1] = (uint8_t)((i + 1) << 1);
    }
    mktile(&t, 0, 2, 3, 1, 16, 1, 0);           /* 8 wide, 2 rows, S mirror */
    memset(out, 0xCC, sizeof(out));
    /* row 0 (even, linear) is 8 texels + 8 mirrored == 32 bytes; row 1
     * (odd, swizzled) is another 16, so 48 bytes is exactly its last texel */
    BrTex3dExpand(out, 48, 2, src, NULL, 0, 1, 0, 0, 1, &t, 0, 0,
                  0, 0, 0, 0, 0, 0, 0, 0, 1);
    memcpy(o16, out, 64);
    check(out[48] != 0xCC || out[49] != 0xCC,
          "a budget exhausted on a swizzled RGBA16 row still emits the "
          "first mirrored texel -- the row bound is tested first there");
    check(o16[24] == o16[23],
          "...and that element is the row's last texel, reversed");
    check(out[50] == 0xCC, "but only one, then it stops");
}

/* ------------------------------------------------------------------ */
static void test_expander_pitch(void)
{
    /* 0x100256BC adds the tile's `line` to the row cursor, and `line` is
     * NOT the row width: two of testdata/ce.rca's textures have a line
     * narrower than 1<<maskS, which is the N64 idiom for a non-power-of-two
     * image.  A wider one is the same mechanism -- padded rows -- and is
     * what this asserts, because a port that advanced by the row width
     * instead would read the padding as pixels. */
    BrTex3dTile t;
    static const uint8_t src[12] = { 1, 2, 3, 4, 0xEE, 0xEE,
                                     5, 6, 7, 8, 0xEE, 0xEE };
    uint8_t out[32];

    printf("the row pitch is `line`, not the row width\n");
    mktile(&t, 4, 1, 2, 1, 6, 0, 0);            /* I8, 4 wide, pitch 6 */
    memset(out, 0xCC, sizeof(out));
    BrTex3dExpand(out, 8, 1, src, NULL, 4, 0, 0, 0, 1, &t, 0, 0,
                  0, 0, 0, 0, 0, 0, 0, 0, 0);
    check(out[0] == 1 && out[3] == 4 && out[4] == 5 && out[7] == 8,
          "row 1 starts `line` bytes on, so the two padding bytes are "
          "never read");
}

int main(void)
{
    test_texel_rotation();
    test_row_swizzle();
    test_ci4_and_mirror();
    test_plant_and_dedup();
    test_no_texture_no_bind();
    test_unsupported_is_refused();
    test_divide_idioms();
    test_expander_byte_arms();
    test_expander_blend_arms();
    test_expander_raw_index();
    test_expander_budget_and_lods();
    test_expander_mirrors();
    test_rgba16_tests_the_row_bound_first();
    test_expander_pitch();
    printf(g_fail ? "\nFAILED\n" : "\nALL PASSED\n");
    return g_fail;
}
