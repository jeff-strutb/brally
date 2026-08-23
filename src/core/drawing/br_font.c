/* br_font.c -- see br_font.h for where the glyph pixels come from and how
 * that was established.  This file has three parts:
 *
 *   1. recovery   -- pull the tables and the texel blocks out of BRD3D.dll
 *                    (IA8 strips) or BRGlide.dll (AI44 windows)
 *   2. 0x10018590 / 0x10015B10 -- the string emitter, transcribed, and
 *      0x100193C0 / 0x10016980 -- the width routine
 *   3. a reference rasteriser for the display list part 2 produces
 *
 * BOTH BUILDS ARE KEPT.  BRGlide.dll is the reference per CONVENTIONS.md, but
 * the D3D reading is a legitimate second source for the shared core and having
 * both is what let the divergences be measured rather than guessed.  Every
 * address literal below is tagged with the build it came from.
 */

#include "br_font.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* slice1_05.h owns 0x1002F900.  Declared the way slice6_76.c declares it --
 * by opaque pointer -- so this file does not have to pull in a header that
 * models a dozen unrelated things.  BrGfxWords is {uint32_t w0, w1}, i.e. one
 * display-list command, which is exactly what the original reserves. */
struct BrGfxWords;
extern void BrRdpSetCombineLERP(struct BrGfxWords *pOut,
                                int a0,  int b0,  int c0,  int d0,
                                int Aa0, int Ab0, int Ac0, int Ad0,
                                int a1,  int b1,  int c1,  int d1,
                                int Aa1, int Ab1, int Ac1, int Ad1);

/* ======================================================================
 * PART 1 -- recovery
 * ====================================================================== */

/* --- D3D (orig/BRD3D.dll) ------------------------------------------------
 * Every address here is quoted from the instruction that loads it; the
 * arithmetic that pins the extents is in br_font.h. */
#define BR_FONT_VA_CLASSMAP     0x100A5FEFu   /* 0x100189FF, 0x10019424    */
#define BR_FONT_VA_OFF_LARGE    0x100A6070u   /* 0x100185FC, 0x100193EB    */
#define BR_FONT_VA_OFF_SMALL    0x100A6150u   /* 0x1001861D, 0x100193FA    */

#define BR_FONT_VA_LARGE_ALPHA  0x100946C8u   /* 0x10073898 */
#define BR_FONT_VA_LARGE_PUNCT  0x1009B4C8u   /* 0x10073854 */
#define BR_FONT_VA_SMALL_ALPHA  0x100A22D0u   /* 0x10073922 */
#define BR_FONT_VA_SMALL_PUNCT  0x100A4170u   /* 0x100738DD */

#define BR_FONT_VA_RAMP_LARGE_A 0x100A74B8u   /* 0x10018604 */
#define BR_FONT_VA_RAMP_LARGE_B 0x100A75F8u   /* 0x10018609 */
#define BR_FONT_VA_RAMP_SMALL_A 0x100A7738u   /* 0x10018625 */
#define BR_FONT_VA_RAMP_SMALL_B 0x100A7878u   /* 0x1001862A */

/* --- Glide (orig/BRGlide.dll) --------------------------------------------
 * Same provenance rule: each is the immediate at the quoted instruction. */
#define BR_FONT_GVA_CLASSMAP     0x100A58F7u  /* 0x10015FA2, 0x100169DB    */
#define BR_FONT_GVA_OFF_LARGE    0x100A5978u  /* 0x10015BAA, 0x100169B1    */
#define BR_FONT_GVA_OFF_SMALL    0x100A5A58u  /* 0x10015B74, 0x100169A2    */

#define BR_FONT_GVA_BLOCK_LARGE  0x1007B618u  /* 0x10015BC9, 0x1006C797    */
#define BR_FONT_GVA_BLOCK_SMALL  0x1009D218u  /* 0x10015B93, 0x1006C7D2    */

#define BR_FONT_GVA_RAMP_LARGE_A 0x100A6C78u  /* 0x10015BBB */
#define BR_FONT_GVA_RAMP_LARGE_B 0x100A6DB8u  /* 0x10015BC0 */
#define BR_FONT_GVA_RAMP_SMALL_A 0x100A6EF8u  /* 0x10015B85 */
#define BR_FONT_GVA_RAMP_SMALL_B 0x100A7038u  /* 0x10015B8A */

/* Counts the two register loops in 0x10073820 use: 0x6C/4 and 0x68/4. */
#define BR_FONT_N_PUNCT 27
#define BR_FONT_N_ALPHA 26

/* A mapped image plus enough of its section table to turn a virtual address
 * into a file offset.  Decoded byte-wise; no structure is overlaid on the
 * file, per CONVENTIONS.md, and every field is little-endian by the PE spec
 * regardless of the host. */
typedef struct BrPeView {
    const uint8_t *pFile;
    size_t         cbFile;
    uint32_t       imageBase;
    uint32_t       nSections;
    size_t         offSections;
} BrPeView;

static uint32_t br_u16le(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8);
}

static uint32_t br_u32le(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int br_pe_open(BrPeView *pv, const uint8_t *pFile, size_t cb)
{
    uint32_t lfanew, cbOpt;

    pv->pFile  = pFile;
    pv->cbFile = cb;

    if (cb < 0x40u || pFile[0] != 'M' || pFile[1] != 'Z')
        return -1;
    lfanew = br_u32le(pFile + 0x3C);
    if ((size_t)lfanew + 24u > cb)
        return -1;
    if (memcmp(pFile + lfanew, "PE\0\0", 4) != 0)
        return -1;

    pv->nSections = br_u16le(pFile + lfanew + 6);
    cbOpt         = br_u16le(pFile + lfanew + 20);
    if (cbOpt < 32u)
        return -1;
    /* PE32 optional header: ImageBase is at +28.  (PE32+ puts it at +24 and
     * is 64-bit, but this image is PE32 and the magic says so.) */
    if (br_u16le(pFile + lfanew + 24) != 0x010Bu)
        return -1;
    pv->imageBase   = br_u32le(pFile + lfanew + 24 + 28);
    pv->offSections = (size_t)lfanew + 24u + cbOpt;
    if (pv->offSections + (size_t)pv->nSections * 40u > cb)
        return -1;
    return 0;
}

/* The bytes at `va`, or NULL if the image does not hold them (which is how
 * .bss is told from .data -- see br_data.c's note on the same test). */
static const uint8_t *br_pe_at(const BrPeView *pv, uint32_t va, size_t n)
{
    uint32_t rva = va - pv->imageBase;
    uint32_t i;

    for (i = 0; i < pv->nSections; ++i) {
        const uint8_t *pSec = pv->pFile + pv->offSections + (size_t)i * 40u;
        uint32_t vsize   = br_u32le(pSec + 8);
        uint32_t vaddr   = br_u32le(pSec + 12);
        uint32_t rawSize = br_u32le(pSec + 16);
        uint32_t rawPtr  = br_u32le(pSec + 20);
        uint32_t span    = (vsize > rawSize) ? vsize : rawSize;
        uint32_t off;

        if (rva < vaddr || rva - vaddr >= span)
            continue;
        off = rva - vaddr;
        if (off >= rawSize || (uint32_t)(rawSize - off) < (uint32_t)n)
            return NULL;               /* past the raw data: .bss */
        if ((size_t)rawPtr + off + n > pv->cbFile)
            return NULL;
        return pv->pFile + rawPtr + off;
    }
    return NULL;
}

/* `cb` is the whole block: pitch*height for a D3D strip, stride*54 for a
 * Glide block.  `height` stays the CELL height in both -- the rows a glyph
 * occupies -- because that is what BrFontGlyph reports and what the emitter's
 * G_SETTILESIZE bounds. */
static int br_font_strip(const BrPeView *pv, BrFontStrip *pOut, uint32_t va,
                         int32_t pitch, int32_t height, int32_t stride,
                         size_t cb)
{
    const uint8_t *p = br_pe_at(pv, va, cb);

    if (p == NULL)
        return -1;
    pOut->pTexels = (uint8_t *)malloc(cb);
    if (pOut->pTexels == NULL)
        return -1;
    memcpy(pOut->pTexels, p, cb);
    pOut->pitch  = pitch;
    pOut->height = height;
    pOut->stride = stride;
    return 0;
}

void BrFontFree(BrFont *pFont)
{
    int s, k;

    if (pFont == NULL)
        return;
    for (s = 0; s < 2; ++s)
        for (k = 0; k < 2; ++k) {
            free(pFont->aStrip[s][k].pTexels);
            pFont->aStrip[s][k].pTexels = NULL;
        }
}

/* Which build's addresses hold the font in this image, or -1.
 *
 * The test is the class map's three sentinels plus the offset table's shape.
 * Those are not a checksum: they are the three entries the rest of this file
 * relies on ('0' is the tenth digit class, 'A' opens the letter run, 'a' folds
 * onto 'A') and the two-run structure the class map depends on.  The two
 * builds put their tables 0x6F8 bytes apart, so a probe that passes at one
 * address cannot also pass at the other by accident -- and the loader tries
 * both and requires exactly one to answer. */
static int br_font_probe(const BrPeView *pv, uint32_t vaClassMap,
                         uint32_t vaOffLarge, uint32_t vaOffSmall)
{
    const uint8_t *pc = br_pe_at(pv, vaClassMap + BR_FONT_CLASS_LO,
                                (size_t)BR_FONT_CLASS_N);
    const uint8_t *pl = br_pe_at(pv, vaOffLarge, (size_t)BR_FONT_CLASSES * 4u);
    const uint8_t *ps = br_pe_at(pv, vaOffSmall, (size_t)BR_FONT_CLASSES * 4u);

    if (pc == NULL || pl == NULL || ps == NULL)
        return 0;
    if ((signed char)pc['0' - BR_FONT_CLASS_LO] != 9 ||
        (signed char)pc['A' - BR_FONT_CLASS_LO] != BR_FONT_CLASS_ALPHA ||
        (signed char)pc['a' - BR_FONT_CLASS_LO] != BR_FONT_CLASS_ALPHA)
        return 0;
    /* Both runs open at column 0, and the table therefore DROPS at the run
     * boundary -- the fact that makes class 27 unusable. */
    if (br_u32le(pl) != 0u || br_u32le(ps) != 0u ||
        br_u32le(pl + BR_FONT_CLASS_ALPHA * 4) != 0u ||
        br_u32le(ps + BR_FONT_CLASS_ALPHA * 4) != 0u)
        return 0;
    return 1;
}

int BrFontLoad(BrFont *pFont, const char *pszDllPath)
{
    static const uint32_t aRampVaD3D[2][2] = {
        { BR_FONT_VA_RAMP_LARGE_A, BR_FONT_VA_RAMP_LARGE_B },
        { BR_FONT_VA_RAMP_SMALL_A, BR_FONT_VA_RAMP_SMALL_B }
    };
    static const uint32_t aRampVaGlide[2][2] = {
        { BR_FONT_GVA_RAMP_LARGE_A, BR_FONT_GVA_RAMP_LARGE_B },
        { BR_FONT_GVA_RAMP_SMALL_A, BR_FONT_GVA_RAMP_SMALL_B }
    };
    const uint32_t (*paRampVa)[2];
    uint32_t       vaClassMap, vaOffLarge, vaOffSmall;
    BrPeView       pv;
    FILE          *f;
    uint8_t       *pFile = NULL;
    long           cb;
    const uint8_t *p;
    int            i, s, v, isD3D, isGlide, rc = -1;

    if (pFont == NULL || pszDllPath == NULL)
        return -1;
    memset(pFont, 0, sizeof(*pFont));

    f = fopen(pszDllPath, "rb");
    if (f == NULL)
        return -1;
    if (fseek(f, 0, SEEK_END) != 0)
        goto done;
    cb = ftell(f);
    if (cb <= 0 || fseek(f, 0, SEEK_SET) != 0)
        goto done;
    pFile = (uint8_t *)malloc((size_t)cb);
    if (pFile == NULL)
        goto done;
    if (fread(pFile, 1, (size_t)cb, f) != (size_t)cb)
        goto done;
    if (br_pe_open(&pv, pFile, (size_t)cb) != 0)
        goto done;

    /* Which build is this?  An image where BOTH probes answer, or neither, is
     * refused: silently picking one would produce garbage glyphs, which is
     * exactly the failure this check exists to prevent. */
    isGlide = br_font_probe(&pv, BR_FONT_GVA_CLASSMAP,
                            BR_FONT_GVA_OFF_LARGE, BR_FONT_GVA_OFF_SMALL);
    isD3D   = br_font_probe(&pv, BR_FONT_VA_CLASSMAP,
                            BR_FONT_VA_OFF_LARGE, BR_FONT_VA_OFF_SMALL);
    if (isGlide == isD3D)
        goto done;

    if (isGlide) {
        pFont->build = BR_FONT_BUILD_GLIDE;
        vaClassMap   = BR_FONT_GVA_CLASSMAP;
        vaOffLarge   = BR_FONT_GVA_OFF_LARGE;
        vaOffSmall   = BR_FONT_GVA_OFF_SMALL;
        paRampVa     = aRampVaGlide;
    } else {
        pFont->build = BR_FONT_BUILD_D3D;
        vaClassMap   = BR_FONT_VA_CLASSMAP;
        vaOffLarge   = BR_FONT_VA_OFF_LARGE;
        vaOffSmall   = BR_FONT_VA_OFF_SMALL;
        paRampVa     = aRampVaD3D;
    }

    /* Class map: chars BR_FONT_CLASS_LO..HI, i.e. the table base plus 0x21. */
    p = br_pe_at(&pv, vaClassMap + BR_FONT_CLASS_LO, (size_t)BR_FONT_CLASS_N);
    if (p == NULL)
        goto done;
    for (i = 0; i < BR_FONT_CLASS_N; ++i)
        pFont->aClass[i] = (signed char)p[i];

    p = br_pe_at(&pv, vaOffLarge, (size_t)BR_FONT_CLASSES * 4u);
    if (p == NULL)
        goto done;
    for (i = 0; i < BR_FONT_CLASSES; ++i)
        pFont->aOff[BR_FONT_LARGE][i] = (int32_t)br_u32le(p + i * 4);

    p = br_pe_at(&pv, vaOffSmall, (size_t)BR_FONT_CLASSES * 4u);
    if (p == NULL)
        goto done;
    for (i = 0; i < BR_FONT_CLASSES; ++i)
        pFont->aOff[BR_FONT_SMALL][i] = (int32_t)br_u32le(p + i * 4);

    if (pFont->build == BR_FONT_BUILD_GLIDE) {
        /* Two blocks of 54 fixed-stride windows.  0x1006C790 copies exactly
         * 0x21C00 and 0x8700 bytes, which is 54 * stride in each case -- the
         * count is the original's, not an inference from the extents. */
        if (br_font_strip(&pv, &pFont->aStrip[BR_FONT_LARGE][0],
                          BR_FONT_GVA_BLOCK_LARGE, BR_FONT_G_LARGE_PITCH,
                          BR_FONT_LARGE_CELL, BR_FONT_G_LARGE_STRIDE,
                          (size_t)BR_FONT_G_LARGE_STRIDE * BR_FONT_G_CELLS)
                != 0 ||
            br_font_strip(&pv, &pFont->aStrip[BR_FONT_SMALL][0],
                          BR_FONT_GVA_BLOCK_SMALL, BR_FONT_G_SMALL_PITCH,
                          BR_FONT_SMALL_CELL, BR_FONT_G_SMALL_STRIDE,
                          (size_t)BR_FONT_G_SMALL_STRIDE * BR_FONT_G_CELLS)
                != 0)
            goto done;
        pFont->aBlockVa[BR_FONT_LARGE] = BR_FONT_GVA_BLOCK_LARGE;
        pFont->aBlockVa[BR_FONT_SMALL] = BR_FONT_GVA_BLOCK_SMALL;
    } else {
        if (br_font_strip(&pv,
                          &pFont->aStrip[BR_FONT_LARGE][BR_FONT_STRIP_PUNCT],
                          BR_FONT_VA_LARGE_PUNCT, BR_FONT_LARGE_PITCH,
                          BR_FONT_LARGE_CELL, 0,
                          (size_t)BR_FONT_LARGE_PITCH * BR_FONT_LARGE_CELL)
                != 0 ||
            br_font_strip(&pv,
                          &pFont->aStrip[BR_FONT_LARGE][BR_FONT_STRIP_ALPHA],
                          BR_FONT_VA_LARGE_ALPHA, BR_FONT_LARGE_PITCH,
                          BR_FONT_LARGE_CELL, 0,
                          (size_t)BR_FONT_LARGE_PITCH * BR_FONT_LARGE_CELL)
                != 0 ||
            br_font_strip(&pv,
                          &pFont->aStrip[BR_FONT_SMALL][BR_FONT_STRIP_PUNCT],
                          BR_FONT_VA_SMALL_PUNCT, BR_FONT_SMALL_PITCH,
                          BR_FONT_SMALL_CELL, 0,
                          (size_t)BR_FONT_SMALL_PITCH * BR_FONT_SMALL_CELL)
                != 0 ||
            br_font_strip(&pv,
                          &pFont->aStrip[BR_FONT_SMALL][BR_FONT_STRIP_ALPHA],
                          BR_FONT_VA_SMALL_ALPHA, BR_FONT_SMALL_PITCH,
                          BR_FONT_SMALL_CELL, 0,
                          (size_t)BR_FONT_SMALL_PITCH * BR_FONT_SMALL_CELL)
                != 0)
            goto done;
    }

    for (s = 0; s < 2; ++s)
        for (v = 0; v < 2; ++v) {
            p = br_pe_at(&pv, paRampVa[s][v], (size_t)BR_FONT_RAMP_BYTES);
            if (p == NULL)
                goto done;
            memcpy(pFont->aRamp[s][v], p, (size_t)BR_FONT_RAMP_BYTES);
        }

    if (pFont->build == BR_FONT_BUILD_GLIDE)
        BrFontRegisterPages(pFont);
    else
        BrFontRegisterGlyphs(pFont);
    rc = 0;

done:
    free(pFile);
    fclose(f);
    if (rc != 0)
        BrFontFree(pFont);
    return rc;
}

/* 0x1006C790 (Glide).  The whole function, and it is short enough to quote:
 *
 *     g_1184C47C = MakeTexture(0x1007B618, 0x40, 0x40, 4);
 *     p = Copy(0, 0, 0);
 *     p = Copy(p, 0x1007B618, 0x21C00);
 *     g_1184C46C = MakeTexture(0x1009D218, 0x20, 0x20, 4);
 *         Copy(p, 0x1009D218, 0x8700);
 *
 * TWO textures, not 106.  The dimensions are the 64x64 / 32x32 the header
 * comment quotes; format 4 is GR_TEXFMT_ALPHA_INTENSITY_44.  The port has no
 * backend, so it plants the token BrFontRasteriseDL understands, and the two
 * staging copies have no analogue here -- this module already holds the
 * blocks. */
/* WHAT IT DOES: makes the game's lettering available to the graphics hardware
 * in the Glide build, as just two textures -- one holding every big character,
 * one holding every small one. Drawing a particular letter then means aiming at
 * a window inside the right sheet. */
/* @implements 0x1006C790 glide BrFontRegisterPages */
void BrFontRegisterPages(BrFont *pFont)
{
    pFont->ahPage[BR_FONT_LARGE] = BR_FONT_TOK_PAGE(BR_FONT_LARGE);
    pFont->ahPage[BR_FONT_SMALL] = BR_FONT_TOK_PAGE(BR_FONT_SMALL);
}

/* 0x10073820 (D3D).  Four loops of 27, 26, 27, 26 over the two offset tables, in
 * the original's order: large punctuation, large letters, small punctuation,
 * small letters.  Class BR_FONT_CLASS_GAP is written by neither, so it stays
 * 0 -- which is exactly what the original's .bss slot holds, and is why the
 * emitter can be handed a class-27 index without faulting. */
/* WHAT IT DOES: the Direct3D build's version of the same job -- it gives every
 * single character of both sizes its own texture rather than sharing two big
 * sheets, so drawing a letter means switching to that letter's texture. The
 * unused slot between the punctuation run and the alphabet run is deliberately
 * left blank. */
/* @d3donly 0x10073820 BrFontRegisterGlyphs -- glide twin 0x1006C790 COMDAT-folded onto BrFontRegisterPages above */
void BrFontRegisterGlyphs(BrFont *pFont)
{
    int i;

    for (i = 0; i < BR_FONT_N_PUNCT; ++i)
        pFont->ahTex[BR_FONT_LARGE][i] = BR_FONT_TOK_GLYPH(BR_FONT_LARGE, i);
    for (i = 0; i < BR_FONT_N_ALPHA; ++i)
        pFont->ahTex[BR_FONT_LARGE][BR_FONT_CLASS_ALPHA + i] =
            BR_FONT_TOK_GLYPH(BR_FONT_LARGE, BR_FONT_CLASS_ALPHA + i);
    for (i = 0; i < BR_FONT_N_PUNCT; ++i)
        pFont->ahTex[BR_FONT_SMALL][i] = BR_FONT_TOK_GLYPH(BR_FONT_SMALL, i);
    for (i = 0; i < BR_FONT_N_ALPHA; ++i)
        pFont->ahTex[BR_FONT_SMALL][BR_FONT_CLASS_ALPHA + i] =
            BR_FONT_TOK_GLYPH(BR_FONT_SMALL, BR_FONT_CLASS_ALPHA + i);
}

int BrFontClassOf(const BrFont *pFont, int ch)
{
    /* The original compares AL SIGNED (`cmp al,0x21 / jl`, `cmp al,0x7f /
     * jg`), so 0x80..0xFF land on the same side as a control character. */
    signed char c = (signed char)(unsigned char)ch;

    if (c < (signed char)BR_FONT_CLASS_LO || c > (signed char)BR_FONT_CLASS_HI)
        return -1;
    return pFont->aClass[(int)c - BR_FONT_CLASS_LO];
}

int BrFontGlyph(const BrFont *pFont, int cls, int size, BrGlyph *pOut)
{
    const BrFontStrip *pStrip;
    int32_t left, w;
    int     which;

    if (pFont == NULL || pOut == NULL || size < 0 || size > 1)
        return -1;
    if (cls < 0 || cls >= BR_FONT_CLASSES - 1 || cls == BR_FONT_CLASS_GAP)
        return -1;

    /* Glide keeps every class in aStrip[size][0]; D3D splits the two runs. */
    which  = (pFont->build == BR_FONT_BUILD_GLIDE) ? 0
           : (cls < BR_FONT_CLASS_GAP)             ? BR_FONT_STRIP_PUNCT
                                                   : BR_FONT_STRIP_ALPHA;
    pStrip = &pFont->aStrip[size][which];
    if (pStrip->pTexels == NULL)
        return -1;

    left = pFont->aOff[size][cls];
    /* The `+1` is D3D 0x10018A21's / Glide 0x10015FB7's `inc ecx`: the tile
     * the drawing routine binds is one column WIDER than the advance, so
     * neighbouring glyphs overlap by a pixel.  NEITHER width routine adds it
     * (0x100193C0, 0x10016980), which is why the measured width of a string
     * is one pixel per glyph short of the ink drawn -- in both builds. */
    w = pFont->aOff[size][cls + 1] - left + 1;
    if (w <= 0)
        return -1;

    pOut->fAlphaHigh = (pFont->build == BR_FONT_BUILD_GLIDE);
    pOut->pitch      = pStrip->pitch;
    pOut->w          = w;
    pOut->h          = pStrip->height;

    if (pFont->build == BR_FONT_BUILD_GLIDE) {
        /* Indexed by CLASS, not column: 0x10015FDC's `imul ebx, edx`.  `left`
         * plays no part in addressing here -- it only supplied the width. */
        if (w > pStrip->pitch)
            return -1;
        pOut->pTexels = pStrip->pTexels + (size_t)pStrip->stride * (size_t)cls;
    } else {
        if (left < 0 || left + w > pStrip->pitch)
            return -1;
        pOut->pTexels = pStrip->pTexels + left;
    }
    return 0;
}

/* ======================================================================
 * PART 2 -- 0x10018590
 * ====================================================================== */

/* 0x10019174 and 0x100191F4 are byte-for-byte identical 0x4A-entry tables
 * mapping (ch - 0x30) to a case; 12 is the default.  The characters that map
 * to a case are, in order: 0 1 5 O Y b g o p r w y. */
static const unsigned char s_aColourCase[0x4A] = {
     0, 1,12,12,12, 2,12,12,12,12,
    12,12,12,12,12,12,12,12,12,12,
    12,12,12,12,12,12,12,12,12,12,
    12, 3,12,12,12,12,12,12,12,12,
    12, 4,12,12,12,12,12,12,12,12,
     5,12,12,12,12, 6,12,12,12,12,
    12,12,12, 7, 8,12, 9,12,12,12,
    12,10,12,11
};

/* Case -> the payload of the 0xFA (primitive colour) command emitted by the
 * FIRST directive character.  Case 12 emits nothing. */
static const uint32_t s_aPrimColour[12] = {
    0x000000FFu, 0xFFFFFFFFu, 0x808080FFu, 0xFF7800FFu,
    0xFFFA80FFu, 0x0000C8FFu, 0x009600FFu, 0xCD5F00FFu,
    0xC800C8FFu, 0xBE0000FFu, 0xFFFFFFFFu, 0xFFF500FFu
};

/* Case -> the payload of the 0xFB (environment colour) command emitted by the
 * SECOND directive character.  Four entries differ from the table above; the
 * pair is a gradient, not a colour repeated. */
static const uint32_t s_aEnvColour[12] = {
    0x000000FFu, 0xFFFFFFFFu, 0x808080FFu, 0xFF7800FFu,
    0xD2C869FFu, 0x0000C8FFu, 0x009600FFu, 0xCD5F00FFu,
    0xC800C8FFu, 0xC80000FFu, 0xFFFFFFFFu, 0xD2BE00FFu
};

static void br_emit(BrTextEmit *pSt, uint32_t w0, uint32_t w1)
{
    pSt->cWordsWanted += 2;
    /* DEVIATION: the original never checks. */
    if (pSt->pGfx != NULL && pSt->pGfx + 2 <= pSt->pGfxEnd) {
        pSt->pGfx[0] = w0;
        pSt->pGfx[1] = w1;
        pSt->pGfx += 2;
    }
}

/* The original's colour packer, verbatim: the FIRST component is not masked,
 * so a value above 255 bleeds into the bits above and is shifted out. */
static uint32_t br_pack_rgb(int32_t r, int32_t g, int32_t b)
{
    uint32_t v = (uint32_t)r << 8;

    v |= (uint32_t)g & 0xFFu;
    v <<= 8;
    v |= (uint32_t)b & 0xFFu;
    v <<= 8;
    return v | 0xFFu;                       /* `or cl, 0xff` */
}

/* 0x10018B68 / 0x10018B8A / 0x10018BB7 / 0x10018BCD, all the same shape:
 *
 *     shl eax,2 / sar eax,2 / test ax,ax / jle -> 0 / movsx eax,ax
 *
 * The shift pair only clears bits 30 and 31, and everything after it looks at
 * AX alone, so the low sixteen bits are unaffected and the pair is elided
 * here.  The test is on AX SIGNED: a coordinate whose low word is negative or
 * zero collapses to 0, and one whose low word is positive survives
 * sign-extended -- so 0x10000 clamps to 0, not to 0x10000. */
static int32_t br_clamp_lo16(int32_t v)
{
    int16_t lo = (int16_t)(uint16_t)((uint32_t)v & 0xFFFFu);

    return (lo <= 0) ? 0 : (int32_t)lo;
}

void BrTextEmitInit(BrTextEmit *pSt, const BrFont *pFont,
                    uint32_t *pGfx, size_t cWords)
{
    memset(pSt, 0, sizeof(*pSt));
    pSt->pGfx    = pGfx;
    pSt->pGfxEnd = (pGfx != NULL) ? pGfx + cWords : NULL;
    pSt->detail  = 1;                       /* br_data.c: 0x100B8C90 == 1 */
    pSt->scale   = BR_FONT_LARGE_CELL;
    if (pFont != NULL) {
        pSt->build      = pFont->build;
        pSt->pClassMap  = pFont->aClass;
        pSt->pOffLarge  = pFont->aOff[BR_FONT_LARGE];
        pSt->pOffSmall  = pFont->aOff[BR_FONT_SMALL];
        pSt->ahTexLarge = pFont->ahTex[BR_FONT_LARGE];
        pSt->ahTexSmall = pFont->ahTex[BR_FONT_SMALL];

        pSt->hPageLarge   = pFont->ahPage[BR_FONT_LARGE];
        pSt->hPageSmall   = pFont->ahPage[BR_FONT_SMALL];
        pSt->vaBlockLarge = pFont->aBlockVa[BR_FONT_LARGE];
        pSt->vaBlockSmall = pFont->aBlockVa[BR_FONT_SMALL];
        pSt->strideLarge  = pFont->aStrip[BR_FONT_LARGE][0].stride;
        pSt->strideSmall  = pFont->aStrip[BR_FONT_SMALL][0].stride;
    }
    pSt->hRampLargeA = BR_FONT_TOK_RAMP(BR_FONT_LARGE, 0);
    pSt->hRampLargeB = BR_FONT_TOK_RAMP(BR_FONT_LARGE, 1);
    pSt->hRampSmallA = BR_FONT_TOK_RAMP(BR_FONT_SMALL, 0);
    pSt->hRampSmallB = BR_FONT_TOK_RAMP(BR_FONT_SMALL, 1);
}

/* 0x10018590 (D3D) and 0x10015B10 (Glide).
 *
 * ONE transcription for both, because the two disassemblies were compared
 * command by command and diverge in exactly two places -- each marked
 * `BUILD DIVERGENCE` below with both addresses.  Everything else, from the
 * seventeen preamble commands through the escape handling to the three
 * epilogue commands, is the same code at different addresses; writing it out
 * twice would double the surface without recording anything the comparison
 * did not already establish. */
/* WHAT IT DOES: draws a line of the game's text. It picks the big or the small
 * lettering according to how large the caller asked for, sets up the two-colour
 * gradient the characters are shaded with, then walks the string stamping one
 * character at a time and moving the pen along. Text can carry colour codes
 * inline: a "%" followed by two letters picks a preset gradient, "%xRRGGBB"
 * gives an exact colour, "%%" prints a literal percent sign, and a few codes
 * are simply swallowed. Characters that would fall off the left or top of the
 * screen get pulled back to the edge, but ones falling off the right or bottom
 * are still drawn in full. */
/* TWO CLAIMS, because this body genuinely implements both: `fGlide` selects
 * between the two divergences marked below and nothing else differs.  The
 * Glide line was missing for the same reason BrFontMeasure's was -- see the
 * note there -- which left the reference build's emitter unclaimed while the
 * D3D one was claimed, in a file whose whole point is that it does both. */
/* @implements 0x10015B10 glide BrTextEmitString */
void BrTextEmitString(BrTextEmit *pSt, const char *psz)
{
    uint32_t        aCombine[2];
    const int32_t  *pOff;
    const uint32_t *pahTex;
    uint32_t        hRampA, hRampB, hPage, vaBlock;
    int32_t         scale, penX, top, cell, stride;
    int             fGlide;
    const char     *p, *q;

    if (pSt == NULL || psz == NULL)
        return;

    fGlide = (pSt->build == BR_FONT_BUILD_GLIDE);

    scale = pSt->scale;
    penX  = pSt->x;

    /* (30 * scale) / 40 by reciprocal multiply with a sign correction, i.e.
     * truncating toward zero, which is what C division already does. */
    top = pSt->y - (30 * scale) / 40;

    if (pSt->fHiRes != 0) {
        top   <<= 1;
        scale <<= 1;
        penX  <<= 1;
    }

    /* BUILD DIVERGENCE 1 -- the detail global is D3D-ONLY.
     *
     * D3D 0x100185E7: `cmp dword [0x100B8C90],1 / jg small`, then the scale
     * compare at 0x100185F0.  Glide 0x10015B67 goes straight to the scale
     * compare; nothing in 0x10015B10..0x100166FA reads 0x100B8C90 or any
     * analogue of it.  The same nine bytes are the whole difference between
     * the two width routines (0x100193C0 is 207 bytes, 0x10016980 is 198),
     * which is a second sighting of the same edit.
     *
     * Both compares are signed, and the threshold is tested against the
     * ALREADY-DOUBLED scale -- so hi-res can select the large font for a size
     * that would otherwise take the small one.  Both width routines agree. */
    if ((fGlide || pSt->detail <= 1) && scale >= BR_FONT_LARGE_MIN) {
        cell    = BR_FONT_LARGE_CELL;
        pOff    = pSt->pOffLarge;
        pahTex  = pSt->ahTexLarge;
        hRampA  = pSt->hRampLargeA;
        hRampB  = pSt->hRampLargeB;
        hPage   = pSt->hPageLarge;
        vaBlock = pSt->vaBlockLarge;
        stride  = pSt->strideLarge;
    } else {
        cell    = BR_FONT_SMALL_CELL;
        pOff    = pSt->pOffSmall;
        pahTex  = pSt->ahTexSmall;
        hRampA  = pSt->hRampSmallA;
        hRampB  = pSt->hRampSmallB;
        hPage   = pSt->hPageSmall;
        vaBlock = pSt->vaBlockSmall;
        stride  = pSt->strideSmall;
    }
    if (pOff == NULL || pSt->pClassMap == NULL)
        return;                             /* DEVIATION: no table, no text */

    br_emit(pSt, 0xE7000000u, 0x00000000u);
    br_emit(pSt, 0xBA001402u, 0x00100000u);

    /* The combine command: the original reserves the slot, advances the
     * cursor, then lets 0x1002F900 fill it in.  Only ONE of the sixteen
     * tokens depends on the ramp selection -- b1, which is G_CCMUX_0 for the
     * first ramp and G_CCMUX_TEXEL0 (1001) for the second. */
    aCombine[0] = 0;
    aCombine[1] = 0;
    BrRdpSetCombineLERP((struct BrGfxWords *)aCombine,
                        1003, 1005, 1002, 1005,
                        0,    0,    0,    1001,
                        1000, (pSt->fAltRamp != 0) ? 1001 : 0, 1002, 0,
                        0,    0,    0,    1002);
    br_emit(pSt, aCombine[0], aCombine[1]);

    br_emit(pSt, 0xB900031Du, 0x0C184240u);
    br_emit(pSt, 0xBA000C02u, pSt->f6C0258);
    br_emit(pSt, 0xBA000E02u, 0x00000000u);
    br_emit(pSt, 0xBA001301u, 0x00000000u);
    br_emit(pSt, 0xBA001001u, 0x00000000u);
    br_emit(pSt, 0xBB000001u, 0xFFFFFFFFu);
    br_emit(pSt, 0xE8000000u, 0x00000000u);
    br_emit(pSt, 0xE6000000u, 0x00000000u);
    br_emit(pSt, 0xE7000000u, 0x00000000u);

    /* Load the shading ramp: SETTILE(load tile 7) / SETTIMG / LOADBLOCK /
     * SETTILE(render tile 1) / SETTILESIZE.  The SETTIMG payload is the only
     * part that varies. */
    br_emit(pSt, 0xF51001B0u, 0x07000000u);
    br_emit(pSt, 0xFD100000u, (pSt->fAltRamp != 0) ? hRampB : hRampA);
    br_emit(pSt, 0xF3000000u, 0x0713F000u);
    br_emit(pSt, 0xF56803B0u, 0x01098030u);
    br_emit(pSt, 0xF2002002u, 0x0101E09Eu);

    if (pSt->fUserColour != 0) {
        br_emit(pSt, 0xFB000000u,
                br_pack_rgb(pSt->envR, pSt->envG, pSt->envB));
        br_emit(pSt, 0xFA00FFFFu,
                br_pack_rgb(pSt->primR, pSt->primG, pSt->primB));
    } else if (pSt->fAltColour != 0) {
        br_emit(pSt, 0xFB000000u, 0xFF7F00FFu);
        br_emit(pSt, 0xFA00FFFFu, 0xFFFF7FFFu);
    } else {
        br_emit(pSt, 0xFB000000u, 0xC80000FFu);
        br_emit(pSt, 0xFA00FFFFu, 0xE6E600FFu);
    }

    p = psz;
    /* `q` is set ONCE, to psz + 2, and then stepped in lockstep with `p`, so
     * it is always two characters ahead.  Kept explicit rather than written
     * as p + 2 because the escape paths step the two before rejoining. */
    q = psz + 2;

    while (*p != '\0') {
        int c          = (unsigned char)*p;
        int fDrawGlyph = 0;

        if (c == ' ') {
            /* (14 * scale) / 40, plus one.  0x100193C0 computes the same
             * quotient and does NOT add the one, so a string with spaces
             * measures narrower than it draws.  Both are preserved. */
            penX += (14 * scale) / 40 + 1;
        } else if (c == '%' && p[1] != '\0' && p[1] != '%') {
            int c2 = (unsigned char)p[1];

            if (c2 == 'i' || c2 == 'n') {
                /* Consumed, no output. */
                ++p;
                ++q;
            } else if (c2 == 'x') {
                unsigned int r = 0, g = 0, b = 0;
                int32_t br, bg, bb;

                /* Six hex digits out of `q`, the character after "%x".  A
                 * short or malformed field leaves the original's stack slots
                 * holding garbage; this port zeroes them first.  DEVIATION,
                 * and the only one on this path. */
                (void)sscanf(q, "%02x%02x%02x", &r, &g, &b);

                br_emit(pSt, 0xFA00FFFFu,
                        br_pack_rgb((int32_t)r, (int32_t)g, (int32_t)b));

                /* The same triple brightened by 0x80 and saturated at 0xFF
                 * (unsigned compares) becomes the other end of the gradient,
                 * so "%xRRGGBB" sets both ends from one value. */
                br = (int32_t)r + 0x80;
                bg = (int32_t)g + 0x80;
                bb = (int32_t)b + 0x80;
                if ((uint32_t)br > 0xFFu) br = 0xFF;
                if ((uint32_t)bg > 0xFFu) bg = 0xFF;
                if ((uint32_t)bb > 0xFFu) bb = 0xFF;

                /* The original steps seven characters unconditionally, which
                 * walks off the end of a truncated "%x" field.  DEVIATION:
                 * the step stops at the terminator.  It can only differ where
                 * the original was already reading out of bounds. */
                {
                    int n = 7;
                    while (n-- > 0 && *p != '\0') { ++p; ++q; }
                }
                br_emit(pSt, 0xFB000000u, br_pack_rgb(br, bg, bb));
            } else if (*q == '\0') {
                /* The colour code needs TWO letters; with only one left the
                 * '%' is drawn as an ordinary glyph instead. */
                fDrawGlyph = 1;
            } else {
                int k1 = (int)(signed char)p[1] - 0x30;
                int k2 = (int)(signed char)*q  - 0x30;

                br_emit(pSt, 0xE7000000u, 0x00000000u);

                /* The first letter sets the primitive colour, the second the
                 * environment colour -- the two ends of the gradient.  An
                 * unrecognised letter (case 12) emits nothing, so "%zz"
                 * silently changes neither and still eats both characters. */
                if ((uint32_t)k1 <= 0x49u && s_aColourCase[k1] < 12u)
                    br_emit(pSt, 0xFA00FFFFu, s_aPrimColour[s_aColourCase[k1]]);
                if ((uint32_t)k2 <= 0x49u && s_aColourCase[k2] < 12u)
                    br_emit(pSt, 0xFB000000u, s_aEnvColour[s_aColourCase[k2]]);

                p += 2;
                q += 2;
            }
        } else {
            if (c == '%' && p[1] == '%') {
                /* "%%": swallow one, draw the other. */
                ++p;
                ++q;
            }
            fDrawGlyph = 1;
        }

        if (fDrawGlyph) {
            signed char sc = (signed char)(unsigned char)*p;

            /* Signed compares in the original, so 0x80..0xFF are outside. */
            if (sc >= (signed char)BR_FONT_CLASS_LO &&
                sc <= (signed char)BR_FONT_CLASS_HI) {
                int      cls = pSt->pClassMap[(int)sc - BR_FONT_CLASS_LO];
                int32_t  left, w, drawW, rightEdge, bottom;
                uint32_t hTex, lrs, lrt;

                /* DEVIATION: the original trusts the class map. */
                if (cls >= 0 && cls < BR_FONT_CLASSES - 1) {
                    left = pOff[cls];
                    w    = pOff[cls + 1] - left + 1;   /* 0x10018A21 inc ecx */

                    /* BUILD DIVERGENCE 2 -- how the glyph is bound.
                     *
                     * D3D 0x100189D4 emits ONE command, 0xDC carrying the
                     * handle 0x10073820 registered for this class: 106
                     * textures, switched between.
                     *
                     * Glide 0x10015FC1..0x10015FFD emits TWO.  The handle is
                     * the SAME for every glyph of a size (0x1184C47C /
                     * 0x1184C46C, the two textures 0x1006C790 made), and a
                     * 0xDD in front of the 0xDC re-points it at this class's
                     * window -- `base + stride*cls`, the `imul ebx,edx / add
                     * ebx,[esp+0x30]` at 0x10015FDC.  ONE texture, re-aimed.
                     *
                     * The payload here is the ORIGINAL virtual address, not
                     * an invented token, so the emitted word is the word the
                     * original emitted and the rasteriser can decode the
                     * class straight back out of it. */
                    if (fGlide) {
                        br_emit(pSt, 0xDD000000u | (hPage & 0x00FFFFFFu),
                                vaBlock + (uint32_t)stride * (uint32_t)cls);
                        br_emit(pSt, 0xDC000000u | (hPage & 0x00FFFFFFu), 1u);
                    } else {
                        hTex = (pahTex != NULL) ? pahTex[cls] : 0u;
                        br_emit(pSt, 0xDC000000u | (hTex & 0x00FFFFFFu), 1u);
                    }
                    br_emit(pSt, 0xDE000000u, 0x3F800000u);   /*  1.0f */
                    br_emit(pSt, 0xDF000000u, 0xBF800000u);   /* -1.0f */

                    /* SETTILESIZE, tile 0, 10.2 fixed point: uls = ult = 0.5
                     * (the 0x2002 in w0), lrs = w - 0.5, lrt = cell - 0.5. */
                    lrs = (((uint32_t)w << 14) - 0x2000u) & 0x00FFF000u;
                    lrt = (uint32_t)(cell * 4 - 2) & 0xFFFu;
                    br_emit(pSt, 0xF2002002u, lrs | lrt);

                    drawW     = (scale * w) / cell;
                    rightEdge = penX + drawW;
                    bottom    = top + scale;

                    if (penX >= 0 && rightEdge <= 0x140 &&
                        top >= 0 && bottom <= 0xF0) {
                        br_emit(pSt,
                                0xE3000000u |
                                    (((uint32_t)rightEdge & 0xFFFu) << 12) |
                                    ((uint32_t)bottom & 0xFFFu),
                                (((uint32_t)penX & 0xFFFu) << 12) |
                                    ((uint32_t)top & 0xFFFu));
                    } else {
                        /* The other arm emits the SAME rectangle with every
                         * corner clamped at zero -- and only at zero.  A
                         * rectangle past the right or bottom edge still goes
                         * out unchanged, so this is not a scissor. */
                        int32_t cr = br_clamp_lo16(rightEdge);
                        int32_t cb = br_clamp_lo16(bottom);
                        int32_t cl = br_clamp_lo16(penX);
                        int32_t ct = br_clamp_lo16(top);

                        br_emit(pSt,
                                0xE3000000u |
                                    (((uint32_t)cr & 0xFFFu) << 12) |
                                    ((uint32_t)cb & 0xFFFu),
                                (((uint32_t)cl & 0xFFFu) << 12) |
                                    ((uint32_t)ct & 0xFFFu));
                    }

                    /* The pen advances by ONE LESS than the tile width -- the
                     * quantity 0x100193C0 sums. */
                    penX += (scale * (w - 1)) / cell;
                }
            }
        }

        /* The original reads the NEXT byte and then steps, so a NUL ends the
         * loop without being classified.  The `*p` half of the test is the
         * DEVIATION above: an escape path may have parked `p` on the
         * terminator, where the original would read past it. */
        if (*p == '\0' || p[1] == '\0')
            break;
        ++p;
        ++q;
    }

    br_emit(pSt, 0xE7000000u, 0x00000000u);
    br_emit(pSt, 0xBA001301u, 0x00080000u);
    br_emit(pSt, 0xBA001402u, 0x00000000u);
}

/* 0x100193C0 (D3D) and 0x10016980 (Glide).
 *
 * The two are byte-for-byte the same routine apart from the nine bytes of
 * detail test at 0x100193D5 that Glide does not have, so they are transcribed
 * together like the emitter, with the one divergence marked.
 *
 * Note what is NOT here, in EITHER build: the `+1` per glyph and the `+1` per
 * space that the emitter adds.  The emitter's PEN ADVANCE is
 * `(scale*(w-1))/cell` with `w = off[k+1]-off[k]+1`, which is exactly the
 * `(off[k+1]-off[k])*scale/cell` summed below -- the two agree about where
 * the next glyph starts.  What they disagree about is the TILE, which is one
 * column wider, and the SPACE, which the emitter pads by one
 * (0x100161E1 `lea ebp,[ebp+edx+1]` against 0x10016A2A's plain `add`).  So a
 * caption with spaces draws wider than it measures in both builds.
 *
 * WHY THIS CLAIM LINE HAD TO BE ADDED, which is the interesting part.
 *
 * A census of every d3d-tagged claim looking for functions where the builds
 * diverge and the port followed BRD3D reported 0x100193C0 as a hit, pointing
 * at slice6_76.c's BrSub_100193C0 and its `g_i0B8C90 <= 1 &&`.  That reading
 * is CORRECT -- it is the D3D one and the claim says `d3d` -- and the tree
 * has the build-aware routine right here.  The census could not see it,
 * because THIS FUNCTION CARRIED NO @implements LINE AT ALL, so the only claim
 * on the pair was the D3D-only twin's.
 *
 * A function with no claim is invisible to every tool that reads the manifest,
 * and the failure direction is the dangerous one CONVENTIONS.md names: it
 * fails toward "missing", so the census reported work to do that was already
 * done.  The Glide claim below is what makes the pair visible.
 *
 * The D3D number stays with slice6_76.c.  Two bodies implement 0x100193C0 and
 * that is a real duplicate (br_font.h:547 records it); merging them is a
 * separate job and is not made better by moving a label. */
/* @implements 0x10016980 glide BrFontMeasure */
int32_t BrFontMeasure(const BrFont *pFont, const char *psz,
                      int32_t scale, int32_t fHiRes, int32_t detail)
{
    const int32_t *pOff;
    const char    *p;
    int32_t        divisor, total = 0;
    int            c;

    if (pFont == NULL || psz == NULL)
        return 0;

    if (fHiRes != 0)
        scale <<= 1;

    /* BUILD DIVERGENCE -- see BrTextEmitString.  Signed compares (`jg`,
     * `jl`), and the threshold sees the already-doubled scale. */
    if ((pFont->build == BR_FONT_BUILD_GLIDE || detail <= 1) &&
        scale >= BR_FONT_LARGE_MIN) {
        divisor = BR_FONT_LARGE_CELL;
        pOff    = pFont->aOff[BR_FONT_LARGE];
    } else {
        divisor = BR_FONT_SMALL_CELL;
        pOff    = pFont->aOff[BR_FONT_SMALL];
    }

    p = psz;
    c = (unsigned char)*p;
    while (c != 0) {
        int fGlyph = 1;

        /* The original compares AL SIGNED, so every byte from 0x80 up takes
         * the same branch as a space or a control character. */
        if ((signed char)(unsigned char)c < (signed char)BR_FONT_CLASS_LO ||
            (signed char)(unsigned char)c > (signed char)BR_FONT_CLASS_HI) {
            /* 14*scale/40 by reciprocal multiply, truncating toward zero --
             * which C division already does.  NOT scaled by the font cell. */
            total += (14 * scale) / 40;
            fGlyph = 0;
        } else if (c == '%' && p[1] != '\0') {
            if ((unsigned char)p[1] == (unsigned char)c) {
                /* "%%" -- consume one, then measure '%' as a glyph. */
                ++p;
            } else if (p[1] == 'i' || p[1] == 'n') {
                ++p;
                fGlyph = 0;
            } else if (p[2] != '\0') {
                /* ORIGINAL BUG, preserved and present in BOTH builds
                 * (0x10016A08 / 0x10019451): this steps by 2 and the shared
                 * advance below steps again, so THREE characters vanish and
                 * none is measured.  It also means "%xRRGGBB" -- which the
                 * emitter consumes seven characters for -- is measured as
                 * "%x" plus SIX GLYPHS here, so any caption using the hex
                 * colour escape measures far too wide.  Both builds. */
                p += 2;
                fGlyph = 0;
            }
            /* else p[2] is NUL: falls through with AL still '%', so the '%'
             * is measured and the directive letter is measured next time. */
        }

        if (fGlyph) {
            int k = pFont->aClass[c - BR_FONT_CLASS_LO];

            /* DEVIATION: the original trusts the class map, which cannot
             * yield the gap here because no character maps to it. */
            if (k >= 0 && k < BR_FONT_CLASSES - 1)
                total += ((pOff[k + 1] - pOff[k]) * scale) / divisor;
        }

        /* Read the NEXT byte, then step -- so a NUL ends the loop. */
        c = (unsigned char)p[1];
        ++p;
    }

    if (fHiRes != 0)
        total >>= 1;            /* arithmetic shift in the original (`sar`) */

    return total;
}

/* ======================================================================
 * PART 3 -- reference rasteriser
 * ======================================================================
 *
 * NOT in the original: this is the consumer side of the display list, the
 * job BRD3D.dll hands to DirectDraw and BRGlide.dll to Glide.  It exists so
 * the port can prove the recovered pixels are glyphs, and so a host with no
 * backend yet can still put a caption on screen.
 *
 * These commands carry everything that matters:
 *
 *   0xDC  bind the glyph texture   (low 24 bits = the handle table entry)
 *   0xDD  GLIDE ONLY: point that texture at a class's window (w1 = address)
 *   0xFD  bind the shading ramp    (w1 = the ramp's handle)
 *   0xF2  tile size                (tile 0 = the glyph, tile 1 = the ramp)
 *   0xFA  primitive colour         (the gradient's top)
 *   0xFB  environment colour       (the gradient's bottom)
 *   0xE3  texture rectangle        (integer corners -- see README on 0xE1)
 *
 * Everything else -- syncs, othermode, the TMEM load -- is skipped.
 */

typedef struct BrFontRgba { int32_t r, g, b, a; } BrFontRgba;

static BrFontRgba br_unpack(uint32_t v)
{
    BrFontRgba c;
    c.r = (int32_t)((v >> 24) & 0xFFu);
    c.g = (int32_t)((v >> 16) & 0xFFu);
    c.b = (int32_t)((v >>  8) & 0xFFu);
    c.a = (int32_t)( v        & 0xFFu);
    return c;
}

/* A 4-bit IA nibble spans 0..15; 17 maps it onto 0..255 exactly. */
#define BR_FONT_N4(x)  ((int32_t)(x) * 17)

static void br_blend(uint8_t *pPix, int32_t r, int32_t g, int32_t b, int32_t a)
{
    if (a <= 0)
        return;
    if (a >= 255) {
        pPix[0] = (uint8_t)r; pPix[1] = (uint8_t)g;
        pPix[2] = (uint8_t)b; pPix[3] = 255;
        return;
    }
    pPix[0] = (uint8_t)((r * a + pPix[0] * (255 - a)) / 255);
    pPix[1] = (uint8_t)((g * a + pPix[1] * (255 - a)) / 255);
    pPix[2] = (uint8_t)((b * a + pPix[2] * (255 - a)) / 255);
    pPix[3] = (uint8_t)(a + pPix[3] * (255 - a) / 255);
}

size_t BrFontRasteriseDL(const BrFont *pFont,
                         const uint32_t *pDL, size_t cWords,
                         uint8_t *pRgba, int32_t cx, int32_t cy)
{
    BrGlyph    glyph;
    BrFontRgba prim = { 255, 255, 255, 255 };
    BrFontRgba env  = { 255, 255, 255, 255 };
    const uint8_t *pRamp = NULL;
    uint32_t   hGlyph = 0;
    int32_t    tileW = 0, tileH = 0;
    size_t     i, cDrawn = 0;
    int        fHaveGlyph = 0;

    if (pFont == NULL || pDL == NULL || pRgba == NULL)
        return 0;

    memset(&glyph, 0, sizeof(glyph));

    for (i = 0; i + 1 < cWords; i += 2) {
        uint32_t w0 = pDL[i], w1 = pDL[i + 1];

        switch (w0 >> 24) {
        case 0xDCu:
            hGlyph = w0 & 0x00FFFFFFu;
            /* D3D: the handle names the class.  Glide: the handle names only
             * the SIZE, and the class came from the 0xDD just before -- so
             * leave the glyph the 0xDD resolved and only reject a 0xDC that
             * names neither. */
            if (BR_FONT_TOK_IS_GLYPH(hGlyph))
                fHaveGlyph = BrFontGlyph(pFont, BR_FONT_TOK_CLASS(hGlyph),
                                         BR_FONT_TOK_SIZE(hGlyph),
                                         &glyph) == 0;
            else if (!BR_FONT_TOK_IS_PAGE(hGlyph))
                fHaveGlyph = 0;
            break;

        case 0xDDu: {
            /* GLIDE ONLY (0x10015FD0).  w1 is the ORIGINAL address of the
             * class's window, so the class is (w1 - base) / stride.  The
             * division must come out exact -- a payload that lands mid-window
             * is not something the emitter can produce, and quietly rounding
             * it would hide a real bug. */
            int size = BR_FONT_TOK_PAGE_SIZE(w0 & 0x00FFFFFFu);

            fHaveGlyph = 0;
            if (BR_FONT_TOK_IS_PAGE(w0 & 0x00FFFFFFu) &&
                size >= 0 && size < 2) {
                uint32_t base   = pFont->aBlockVa[size];
                uint32_t stride = (uint32_t)pFont->aStrip[size][0].stride;

                if (base != 0u && stride != 0u && w1 >= base &&
                    (w1 - base) % stride == 0u) {
                    uint32_t cls = (w1 - base) / stride;
                    fHaveGlyph = cls < (uint32_t)BR_FONT_G_CELLS &&
                                 BrFontGlyph(pFont, (int)cls, size,
                                             &glyph) == 0;
                }
            }
            break;
        }

        case 0xFDu:
            if (BR_FONT_TOK_IS_RAMP(w1)) {
                int s = BR_FONT_TOK_RAMP_SIZE(w1);
                int v = BR_FONT_TOK_RAMP_VAR(w1);
                if (s >= 0 && s < 2 && v >= 0 && v < 2)
                    pRamp = pFont->aRamp[s][v];
            }
            break;

        case 0xF2u:
            /* Tile 0 only; the preamble's is tile 1 (the ramp). */
            if (((w1 >> 24) & 7u) == 0u) {
                tileW = (int32_t)(((w1 >> 12) & 0xFFFu) + 2u) / 4;
                tileH = (int32_t)((w1 & 0xFFFu) + 2u) / 4;
            }
            break;

        case 0xFAu: prim = br_unpack(w1); break;
        case 0xFBu: env  = br_unpack(w1); break;

        case 0xE3u: {
            int32_t lrx = (int32_t)((w0 >> 12) & 0xFFFu);
            int32_t lry = (int32_t)( w0        & 0xFFFu);
            int32_t ulx = (int32_t)((w1 >> 12) & 0xFFFu);
            int32_t uly = (int32_t)( w1        & 0xFFFu);
            int32_t dw  = lrx - ulx, dh = lry - uly;
            int32_t px, py;

            if (!fHaveGlyph || dw <= 0 || dh <= 0 || tileW <= 0 || tileH <= 0)
                break;
            if (tileW > glyph.w) tileW = glyph.w;
            if (tileH > glyph.h) tileH = glyph.h;

            for (py = 0; py < dh; ++py) {
                int32_t dy = uly + py;
                /* T IS SAMPLED BOTTOM-UP.
                 *
                 * The strips are stored with row 0 at the BOTTOM of the glyph:
                 * dumping class 39 ('L') straight out of .data gives the foot
                 * at row 6 and the stem below it -- an upside-down L. The
                 * retail game plainly drew it the right way up, so the sampling
                 * inverts T.
                 *
                 * It is not a dtdy sign: this build's 0xE3 is the TWO-word
                 * integer-corner variant, so there are no step words in the
                 * stream to carry one.
                 *
                 * The alternative explanation -- that the strip base addresses
                 * are off by (rows-1)*pitch and we are reading from the wrong
                 * end -- is ruled out by the extents being arithmetic: each
                 * strip butts exactly against the next object in the image, so
                 * the bases are pinned and cannot be shifted by a row.
                 *
                 * BOTH BUILDS.  The Glide blob is laid out differently and its
                 * extents are pinned by their own arithmetic, yet it matches
                 * the D3D strips in the SAME row order -- so two independent
                 * layouts agree that row 0 is the bottom.  This is not a D3D
                 * artefact. */
                int32_t sy = tileH - 1 - ((py * tileH) / dh);
                int32_t ramp = 255;

                if (dy < 0 || dy >= cy)
                    continue;
                if (pRamp != NULL) {
                    int32_t ry = (py * BR_FONT_RAMP_H) / dh;
                    if (ry >= BR_FONT_RAMP_H) ry = BR_FONT_RAMP_H - 1;
                    /* The ramp's columns differ only by the dither pattern;
                     * column 0 is representative and avoids reproducing a
                     * checkerboard at an unrelated scale. */
                    ramp = BR_FONT_N4(pRamp[ry * BR_FONT_RAMP_W] >> 4);
                }

                for (px = 0; px < dw; ++px) {
                    int32_t dx = ulx + px;
                    int32_t sx = (px * tileW) / dw;
                    uint8_t texel;
                    int32_t inten, alpha, r, g, b;

                    if (dx < 0 || dx >= cx)
                        continue;
                    texel = glyph.pTexels[sy * glyph.pitch + sx];
                    /* D3D IA8 puts intensity high and alpha low; Glide AI44
                     * is the other way round.  BrFontGlyph reports which. */
                    alpha = BR_FONT_N4(BR_FONT_TEXEL_A(texel,
                                                       glyph.fAlphaHigh));
                    if (alpha == 0)
                        continue;
                    inten = BR_FONT_N4(BR_FONT_TEXEL_I(texel,
                                                       glyph.fAlphaHigh));

                    /* cycle 0: (PRIM - ENV) * ramp + ENV
                     * cycle 1: COMBINED * glyph intensity
                     * See the header for why the ramp and the glyph are
                     * assigned to those two slots and not the other way. */
                    r = env.r + ((prim.r - env.r) * ramp) / 255;
                    g = env.g + ((prim.g - env.g) * ramp) / 255;
                    b = env.b + ((prim.b - env.b) * ramp) / 255;

                    br_blend(pRgba + ((size_t)dy * (size_t)cx + (size_t)dx) * 4,
                             (r * inten) / 255, (g * inten) / 255,
                             (b * inten) / 255, alpha);
                }
            }
            ++cDrawn;
            break;
        }

        default:
            break;
        }
    }

    return cDrawn;
}

/* Sized so the longest caption in the game fits: the preamble is 21 commands
 * and every glyph costs 5 (D3D) or 6 (Glide -- the extra 0xDD), so 4096 words
 * holds well over 200 characters either way. */
#define BR_FONT_DL_WORDS 4096

size_t BrFontDrawString(const BrFont *pFont, const char *psz,
                        int32_t scale, int32_t x, int32_t y,
                        uint8_t *pRgba, int32_t cx, int32_t cy)
{
    static uint32_t s_aDL[BR_FONT_DL_WORDS];
    BrTextEmit st;

    if (pFont == NULL || psz == NULL)
        return (size_t)-1;

    BrTextEmitInit(&st, pFont, s_aDL, BR_FONT_DL_WORDS);
    st.x     = x;
    st.y     = y;
    st.scale = scale;
    BrTextEmitString(&st, psz);

    if (st.cWordsWanted > BR_FONT_DL_WORDS)
        return (size_t)-1;

    return BrFontRasteriseDL(pFont, s_aDL, st.cWordsWanted, pRgba, cx, cy);
}

#ifdef BR_MATCHING_BUILD
/* 0x10073980
 *
 * Fourteen constant arguments through the backend texture constructor at
 * 0x118AA0B0 -- cdecl, last-arg-first: 0x20 x 0x40, fmt 0, siz 4, source
 * 0x100B9CB0, result stored at 0x11829108.  Same shape as 0x100739B0. */
/* WHAT IT DOES: turns a baked-in 32-by-64 picture into a texture the rest of
 * the game can draw with, and remembers the handle the graphics backend
 * returns. */
/* @implements 0x10073980 d3d BrSub10073980 */
typedef void *(*BrSub10073980Fn)(void *pSrc, int a2, int w, int h,
                                 int fmt, int siz, int b31, int b30,
                                 int b29, int b28, int a11, int a12,
                                 int a13, int a14);

extern BrSub10073980Fn g_18AA0B0;     /* 0x118AA0B0 */
extern void           *g_1829108;     /* 0x11829108 */
extern unsigned char   g_0B9CB0[];    /* 0x100B9CB0 */

void BrSub10073980(void)
{
    g_1829108 = g_18AA0B0(g_0B9CB0, 0, 0x20, 0x40, 0, 4,
                          0, 0, 0, 0, 0, 0, 1, 0);
}
#endif

/* ── Ghidra-matched functions ─────────────────────────── */
#ifdef BR_MATCHING_BUILD
typedef int (*funcptr)();
extern int DAT_100b64b0;
extern int DAT_100ba2d0;
extern int DAT_1184c470;
extern int DAT_1184c478;
extern int DAT_1184c484;
extern int DAT_10ac53e8;
extern int DAT_10ac5c2c;
extern int DAT_10ac5d84;
int FUN_10027fb0();
void FUN_10001190();
extern funcptr g_pfn18AA0B0;

/* WHAT IT DOES: create the primary font texture via the registered texture-loader callback. */
/* @implements 0x1006C750 glide BrFontTexCreate */

int BrFontTexCreate(void)

{
  DAT_1184c470 = (*g_pfn18AA0B0)(&DAT_100ba2d0,0,0x40,0x40,1,4,0,0,1,1,0xf,0xf,1,0);
  DAT_1184c478 = DAT_1184c470;
  return;
}

/* WHAT IT DOES: create an alternate 64x64 font texture. */
/* @implements 0x1006C800 glide BrFontTexCreateAlt */

int BrFontTexCreateAlt(void)

{
  DAT_1184c484 = FUN_10027fb0(&DAT_100b64b0,0x40,0x40,2);
  return;
}

/* WHAT IT DOES: free all font glyph textures and the sheet texture. */
/* @implements 0x10058300 glide BrFontTexFreeAll */

void BrFontTexFreeAll(void)

{
  int *piVar1;
  int iVar2;

  iVar2 = 0;
  if ((unsigned short)DAT_10ac5c2c > 0) {
    piVar1 = &DAT_10ac53e8;
    do {
      if (*piVar1 != 0) {
        FUN_10001190(*piVar1);
        *piVar1 = 0;
      }
      iVar2 = iVar2 + 1;
      piVar1 = piVar1 + 2;
    } while (iVar2 < (int)(DAT_10ac5c2c & 0xffff));
  }
  if (DAT_10ac5d84 != 0) {
    FUN_10001190(DAT_10ac5d84);
    DAT_10ac5d84 = 0;
  }
  return;
}

#endif /* BR_MATCHING_BUILD */
