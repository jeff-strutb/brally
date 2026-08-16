/* br_font.h -- the game's proportional bitmap font: where the glyph pixels
 * live, and the routine that turns a string into drawable output.
 *
 * ==========================================================================
 * WHERE THE GLYPH PIXELS COME FROM
 * ==========================================================================
 *
 * They are NOT in a file. They are compiled into the DLL, in `.data`, at the
 * very front of the section, and they are IA8 -- one byte per texel, high
 * nibble intensity, low nibble alpha.
 *
 * The chain that establishes this, end to end:
 *
 *   0x10019300  BrTextDraw            (slice1_03.c) resolves alignment, then
 *   0x10018590  BrTextEmitString      (ported below) emits the display list
 *   0x10073820  BrFontRegisterGlyphs  (ported below) fills the handle tables
 *               0x10018590 indexes, by calling the backend texture
 *               constructor at 0x118AA0AC once per glyph with a POINTER INTO
 *               THE DLL'S OWN .data.
 *
 * 0x10073820 is the proof. It runs four loops, and each one is
 *
 *     handle[i] = (*g_pfn118AA0AC)(strip_base + offset_table[i], 0,
 *                                  cell, cell, pitch, 1, 3, 0,0,0,0,0, 0,1,0);
 *
 * with `1` = the size code (BrGbiTexelsPerWord: 1 -> 8 texels per 64-bit
 * word -> 8 bits per texel) and `3` = the format code (G_IM_FMT_IA). Those
 * two agree exactly with the G_SETTILE the drawing routine emits for the
 * render tile, `0xF56803B0` -> fmt 3 (IA), siz 1 (8b).
 *
 * The four (base, offset table, pitch, cell) tuples, read out of the
 * instruction stream at 0x10073837/0x10073854, 0x1007387B/0x10073898,
 * 0x100738C0/0x100738DD and 0x10073905/0x10073922:
 *
 *   strip           base         offsets      pitch  rows  classes
 *   large letters   0x100946C8   0x100A60E0    704    40   28..53
 *   large digits    0x1009B4C8   0x100A6070    704    40    0..26
 *   small letters   0x100A22D0   0x100A61C0    392    20   28..53
 *   small digits    0x100A4170   0x100A6150    392    20    0..26
 *
 * The extents are pinned by arithmetic, not by guesswork -- every strip butts
 * exactly against the next object in the image:
 *
 *   0x100946C8 + 704*40 == 0x1009B4C8   (large letters -> large digits)
 *   0x1009B4C8 + 704*40 == 0x100A22C8   (+8 bytes of alignment padding)
 *   0x100A22D0 + 392*20 == 0x100A4170   (small letters -> small digits)
 *   0x100A4170 + 392*20 == 0x100A6010   (small digits -> the class map's
 *                                        first live entry, 0x100A5FEF+0x21)
 *   0x100A5FEF + 0x80   == 0x100A606F   (class map -> the offset table)
 *
 * There are 72,000 bytes of glyph pixels. Rendering one confirms the reading:
 * class 0 of the large digit strip, columns 0..25 of rows 0..39, draws the
 * digit "1" with a black outline (0x0F: I=0, A=15), a white body (0xFF) and
 * antialiased edges. The byte histogram over the whole strip is dominated by
 * 0x00, 0xFF and 0x0F, which is what an outlined IA8 font looks like and is
 * not what any other 8-bit format looks like.
 *
 * The two offset tables are the SAME tables slice6_76.c already recovered for
 * the width routine 0x100193C0 (`BrTextWidthLarge` / `BrTextWidthSmall`), and
 * that is the second, independent confirmation: a table of cumulative x
 * offsets into a strip is exactly what a proportional-width table is.
 * `offset[k]` is the glyph's left column; `offset[k+1] - offset[k]` is its
 * advance; the drawing routine samples one column MORE than that (see the
 * `+1` in BrFontGlyph) so adjacent glyphs overlap by a pixel.
 *
 * WHY THIS MODULE READS THE DLL INSTEAD OF CARRYING A LITERAL ARRAY
 *
 * br_data.c's convention is to write recovered bytes out as C initialisers.
 * That is right for tables of tens or hundreds of entries; at 72,000 bytes it
 * would be a third of a megabyte of source for data that is already in the
 * tree, byte for byte, in `orig/BRD3D.dll`. So this module reads the real
 * bytes at the addresses above out of the real image. Nothing is invented and
 * nothing is transcribed by hand -- the addresses, pitches and cell heights
 * above ARE the recovery record, and BrFontLoad checks them: it refuses an
 * image whose class map does not carry the expected sentinel values.
 *
 * ==========================================================================
 * THE OTHER TEXTURE: an 8x40 IA8 shading ramp
 * ==========================================================================
 *
 * 0x10018590 also loads a second texture, once per string, from one of four
 * 320-byte blocks at 0x100A74B8 / 0x100A75F8 (large) and 0x100A7738 /
 * 0x100A7878 (small); 0x104B0360 picks between the two of a pair. Every byte
 * in all four is nibble-replicated (0x00, 0x11, ... 0xFF), i.e. IA8 with
 * I == A, and laid out 8 texels per row for 40 rows it is a smooth vertical
 * ramp: 0xFF at the top falling to 0x00 at the bottom in the first block, a
 * V-shaped bevel in the second.
 *
 * The colour combiner says what it is for. 0x10018590 builds it with
 *
 *     cycle 0 colour: (PRIM - ENV) * TEXEL + ENV
 *     cycle 0 alpha :  TEXEL alpha
 *     cycle 1 colour:  COMBINED * TEXEL
 *
 * so the text is a vertical gradient from the primitive colour at the top to
 * the environment colour at the bottom, masked by the glyph. The default
 * pair (see BrTextEmitString) is 0xE6E600 over 0xC80000 -- yellow fading to
 * red, which is what this game's menus look like.
 *
 * UNRESOLVED, and stated rather than papered over: the two cycles' `c` slots
 * both carry token 1002 (G_CCMUX_TEXEL1) and the RDP's two-cycle mux makes
 * "TEXEL0" and "TEXEL1" mean different tiles in different cycles. Which cycle
 * sees the ramp and which sees the glyph is therefore not decidable from the
 * tokens alone. This module takes the only assignment that renders legible
 * text -- gradient from the ramp, mask from the glyph -- because the glyph
 * data plainly distinguishes outline (I=0, A=15) from body (I=15, A=15), and
 * an assignment that ignored the glyph's intensity would draw solid blobs.
 *
 * ==========================================================================
 * WHAT IS NOT MODELLED
 * ==========================================================================
 *
 * The display list this module emits is the original's, verbatim, including
 * the commands a rasteriser does not need (syncs, othermode, the TMEM load of
 * the ramp). BrFontRasteriseDL consumes only the four that carry geometry and
 * colour -- 0xDC (bind glyph), 0xF2 (tile size), 0xE3 (texture rectangle),
 * 0xFA/0xFB (primitive / environment colour) -- and ignores the rest. That is
 * a reference consumer, not the backend.
 */
#ifndef BR_FONT_H
#define BR_FONT_H

#include <stddef.h>
#include <stdint.h>

/* ======================================================================
 * Recovered geometry.  Every one of these is an immediate read out of
 * 0x10018590, 0x100193C0 or 0x10073820; see the addresses in the comments.
 * ====================================================================== */

/* Classes 0..26 are digits and punctuation, 28..53 are letters (upper and
 * lower share a class), 27 is the gap between the two runs.  55 entries so
 * that offset[53 + 1] exists. */
#define BR_FONT_CLASSES     55
#define BR_FONT_CLASS_GAP   27
#define BR_FONT_CLASS_ALPHA 28

/* The class map is indexed by the character itself; only 0x21..0x7F reach
 * it.  Stored for that range and indexed as [c - 0x21], matching
 * slice6_76.h's BrTextClassMap. */
#define BR_FONT_CLASS_LO    0x21
#define BR_FONT_CLASS_HI    0x7F
#define BR_FONT_CLASS_N     (BR_FONT_CLASS_HI - BR_FONT_CLASS_LO + 1)

/* Native cell heights, and the scale at or above which the large font is
 * chosen (0x100185F0 / 0x100185F7 / 0x10018618, and 0x100193DE). */
#define BR_FONT_LARGE_CELL  0x28
#define BR_FONT_SMALL_CELL  0x14
#define BR_FONT_LARGE_MIN   0x19

#define BR_FONT_LARGE_PITCH 704
#define BR_FONT_SMALL_PITCH 392

/* The shading ramp: 320 bytes read as 8 texels by 40 rows. */
#define BR_FONT_RAMP_W      8
#define BR_FONT_RAMP_H      40
#define BR_FONT_RAMP_BYTES  (BR_FONT_RAMP_W * BR_FONT_RAMP_H)

/* Index order for the two-element arrays below.  `size` 0 is the large font
 * because that is the arm 0x100185F7 takes first. */
#define BR_FONT_LARGE       0
#define BR_FONT_SMALL       1
#define BR_FONT_STRIP_PUNCT 0    /* classes 0..26  */
#define BR_FONT_STRIP_ALPHA 1    /* classes 28..53 */

/* One glyph strip: `pIA8` is pitch*height bytes of IA8. */
typedef struct BrFontStrip {
    uint8_t *pIA8;
    int32_t  pitch;
    int32_t  height;
} BrFontStrip;

typedef struct BrFont {
    /* 0x100A5FEF + 0x21, indexed [c - BR_FONT_CLASS_LO]. */
    signed char aClass[BR_FONT_CLASS_N];
    /* 0x100A6070 (large) and 0x100A6150 (small).  Cumulative x offsets. */
    int32_t     aOff[2][BR_FONT_CLASSES];
    /* [size][BR_FONT_STRIP_*] */
    BrFontStrip aStrip[2][2];
    /* [size][variant], variant selected by BrTextEmit::fAltRamp. */
    uint8_t     aRamp[2][2][BR_FONT_RAMP_BYTES];
    /* The analogue of the original's 0x11829238 / 0x11829158 handle tables.
     * The original stores whatever the backend's texture constructor
     * returned; the port stores an opaque token BrFontRasteriseDL can decode
     * back into (size, class).  Either way the drawing routine only ever
     * copies it into the low 24 bits of a 0xDC command, so the token has to
     * fit in 24 bits and must not be zero. */
    uint32_t    ahTex[2][BR_FONT_CLASSES];
} BrFont;

/* The port's stand-in for a backend texture handle.  Both fit in 24 bits. */
#define BR_FONT_TOK_GLYPH(size, cls) \
    (0x00A00000u | ((uint32_t)(size) << 8) | (uint32_t)(cls))
#define BR_FONT_TOK_RAMP(size, variant) \
    (0x00B00000u | ((uint32_t)(size) << 4) | (uint32_t)(variant))
#define BR_FONT_TOK_IS_GLYPH(t)  (((t) & 0x00F00000u) == 0x00A00000u)
#define BR_FONT_TOK_IS_RAMP(t)   (((t) & 0x00F00000u) == 0x00B00000u)
#define BR_FONT_TOK_SIZE(t)      (int)(((t) >> 8) & 0xFu)
#define BR_FONT_TOK_CLASS(t)     (int)((t) & 0xFFu)
#define BR_FONT_TOK_RAMP_SIZE(t) (int)(((t) >> 4) & 0xFu)
#define BR_FONT_TOK_RAMP_VAR(t)  (int)((t) & 0xFu)

/* Read every table and strip above out of a copy of BRD3D.dll (or BRGlide.dll
 * -- the font is in the shared core and both carry it).  Returns 0 on
 * success.  Allocates; call BrFontFree.
 *
 * The PE is decoded byte-wise and no structure is ever overlaid on the file
 * image, per CONVENTIONS.md. */
int  BrFontLoad(BrFont *pFont, const char *pszDllPath);
void BrFontFree(BrFont *pFont);

/* The class of a character, or -1 if it has none (which is every byte outside
 * 0x21..0x7F -- and note the original compares the byte SIGNED, so 0x80..0xFF
 * are outside too).  Class BR_FONT_CLASS_GAP never comes back. */
int  BrFontClassOf(const BrFont *pFont, int ch);

/* Where a class's pixels are.  `w` is offset[k+1] - offset[k] + 1, the width
 * the drawing routine puts in its G_SETTILESIZE; `h` is the cell height.
 * Returns 0 for the gap class or an out-of-range one. */
typedef struct BrGlyph {
    const uint8_t *pIA8;      /* top-left texel, inside the strip */
    int32_t        pitch;
    int32_t        w, h;
} BrGlyph;
int  BrFontGlyph(const BrFont *pFont, int cls, int size, BrGlyph *pOut);

/* ======================================================================
 * 0x10018590 -- emit a string as a display list
 * ======================================================================
 *
 * The original reads fourteen globals and writes through the display-list
 * cursor at 0x106C0680.  slice1_03.h already models most of them as
 * `BrTextState`, reached through BrTextGetState(); this structure is the same
 * set plus the four the emitter needs and BrTextState does not carry.  The
 * host wires one into the other -- they are two views of the same globals,
 * and per CONVENTIONS.md only ONE may own the storage.  slice1_03.c's
 * BrTextState owns x/y/scale/align and the two colour triples; everything
 * here is copied in before the call, never written back.
 *
 * DEVIATION: `pGfxEnd`.  The original writes through an unchecked cursor.
 * This port stops when the buffer is full and reports how many words it
 * wanted, so a caller can size a buffer without running off the end. */
typedef struct BrTextEmit {
    uint32_t *pGfx;          /* 0x106C0680, advanced by 2 per command      */
    uint32_t *pGfxEnd;       /* DEVIATION: one past the last writable word */
    size_t    cWordsWanted;  /* DEVIATION: words the original would emit   */

    int32_t   x;             /* 0x104B0340 */
    int32_t   y;             /* 0x104B0344 */
    int32_t   scale;         /* 0x104B0348 -- the RENDERED cell height     */

    int32_t   fHiRes;        /* 0x106C65E4 -- doubles every coordinate     */
    int32_t   detail;        /* 0x100B8C90 -- > 1 forces the small font    */
    int32_t   fAltRamp;      /* 0x104B0360 -- picks the second ramp        */
    int32_t   fAltColour;    /* 0x104B0358 -- picks the second default pair*/
    int32_t   fUserColour;   /* 0x104B0364 -- raised by BrTextSetColors    */

    int32_t   envR, envG, envB;    /* 0x100A74A8 / AC / B0 */
    int32_t   primR, primG, primB; /* 0x104B0368 / 6C / 70 */

    uint32_t  f6C0258;       /* 0x106C0258, copied into a 0xBA000C02       */

    /* 0x100A5FEF + 0x21, indexed [c - BR_FONT_CLASS_LO]. */
    const signed char *pClassMap;
    /* 0x100A6070 and 0x100A6150 -- the emitter reads the SAME two tables the
     * width routine does, which is why the two agree about advances. */
    const int32_t  *pOffLarge;
    const int32_t  *pOffSmall;

    /* The 0x11829238 / 0x11829158 tables.  NULL means "emit handle 0",
     * which the original would do before BrFontRegisterGlyphs had run. */
    const uint32_t *ahTexLarge;
    const uint32_t *ahTexSmall;

    /* The SETTIMG payloads for the two ramps of the selected size -- the
     * original's 0x100A74B8/0x100A75F8 and 0x100A7738/0x100A7878. */
    uint32_t  hRampLargeA, hRampLargeB;
    uint32_t  hRampSmallA, hRampSmallB;
} BrTextEmit;

/* 0x10018590.  Emits the string at (pSt->x, pSt->y).  Reads no globals. */
void BrTextEmitString(BrTextEmit *pSt, const char *psz);

/* 0x10073820.  Fills pFont->ahTex.  The original calls the backend texture
 * constructor 106 times; the port has no backend here, so it plants the token
 * BrFontRasteriseDL understands.  The LOOP STRUCTURE is the original's: four
 * runs of 27, 26, 27, 26 over the two offset tables, leaving class 27 -- the
 * gap -- untouched in both size tables. */
void BrFontRegisterGlyphs(BrFont *pFont);

/* Point a BrTextEmit at a loaded font: fills ahTexLarge/ahTexSmall and the
 * four ramp handles, and sets the fields the original defaults.  NOT in the
 * original, which reaches all of this through fixed addresses. */
void BrTextEmitInit(BrTextEmit *pSt, const BrFont *pFont,
                    uint32_t *pGfx, size_t cWords);

/* ======================================================================
 * Reference rasteriser
 * ====================================================================== */

/* Walk an emitted display list and write RGBA8888 into a cx-by-cy target,
 * blending over what is there.  Returns the number of glyph rectangles drawn.
 *
 * Only 0xDC, 0xF2, 0xE3, 0xFA and 0xFB are interpreted; see the header
 * comment.  Rectangles are clipped against the target, which the original
 * does NOT do -- its own bounds test only clamps negative corners to zero
 * (0x10018B55) and lets a rectangle past the right edge through unchanged. */
size_t BrFontRasteriseDL(const BrFont *pFont,
                         const uint32_t *pDL, size_t cWords,
                         uint8_t *pRgba, int32_t cx, int32_t cy);

/* Emit + rasterise in one call, into a scratch display list.  `x` is the LEFT
 * edge: alignment belongs to 0x10019300 (slice1_03.c's BrTextDraw), which
 * resolves it with the width routine 0x100193C0 before this ever runs, and is
 * deliberately not duplicated here.  Returns the number of glyphs drawn, or
 * (size_t)-1 if the scratch display list overflows. */
size_t BrFontDrawString(const BrFont *pFont, const char *psz,
                        int32_t scale, int32_t x, int32_t y,
                        uint8_t *pRgba, int32_t cx, int32_t cy);

#endif /* BR_FONT_H */
