/* br_font.h -- the game's proportional bitmap font: where the glyph pixels
 * live, and the routine that turns a string into drawable output.
 *
 * ==========================================================================
 * TWO BUILDS, TWO FONT BLOBS, ONE FONT
 * ==========================================================================
 *
 * This module covers BOTH shipped renderers, and they do not store the font
 * the same way.  Per CONVENTIONS.md, `orig/BRGlide.dll` is the reference; the
 * D3D reading is kept because it is a legitimate second source for the shared
 * code and because having both lets them be diffed.  Everything below is
 * labelled with the build it was read from, and `BrFont::build` says which one
 * a loaded font came from.
 *
 *   BR_FONT_BUILD_D3D    orig/BRD3D.dll     emitter 0x10018590
 *   BR_FONT_BUILD_GLIDE  orig/BRGlide.dll   emitter 0x10015B10
 *
 * The two blobs hold the SAME font -- proven, not assumed.  For all 53
 * renderable classes in both sizes, every texel of the Glide blob equals the
 * corresponding D3D texel with its two NIBBLES SWAPPED, in the same row order.
 * The metrics (class map, both offset tables) and all four shading ramps are
 * byte-identical.  What differs is the pixel FORMAT and the pixel LAYOUT; see
 * the Glide section below.
 *
 * ==========================================================================
 * WHERE THE GLYPH PIXELS COME FROM -- D3D (orig/BRD3D.dll)
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
 * ==========================================================================
 * WHERE THE GLYPH PIXELS COME FROM -- GLIDE (orig/BRGlide.dll)
 * ==========================================================================
 *
 * Also in `.data`, also at the very front (`.data` begins at 0x1007B000 and
 * the first block is 0x1007B618), but stored differently in two ways:
 *
 *   FORMAT.  Glide's is AI44 -- high nibble ALPHA, low nibble INTENSITY, the
 *   OPPOSITE of the D3D build's IA8.  Three independent things say so:
 *
 *     1. 0x1006C790, the Glide analogue of 0x10073820, calls the texture
 *        constructor 0x10027FB0 as
 *            (0x1007B618, 0x40, 0x40, 4)   and   (0x1009D218, 0x20, 0x20, 4)
 *        Format code 4 is Glide's GR_TEXFMT_ALPHA_INTENSITY_44, and 0x40/0x20
 *        are the texture widths, which is where the row pitches below come
 *        from.  (D3D's constructor call passes fmt 3 = IA, siz 1 = 8b.)
 *     2. The byte histogram over the whole large block is dominated by 0x00,
 *        0xFF and 0xF0.  The D3D strips are dominated by 0x00, 0xFF and 0x0F.
 *        0xF0 is "opaque, intensity zero" -- the black outline -- only if the
 *        alpha nibble is the HIGH one.
 *     3. Nibble-swapping every D3D texel reproduces the Glide blob exactly.
 *
 *   The ramps cannot settle this, and that is worth stating: every ramp byte
 *   is nibble-replicated, so the swap is a no-op there and the four ramps come
 *   out byte-identical between the builds.  The format had to be established
 *   from the glyphs and from the constructor call.
 *
 *   LAYOUT.  D3D packs a whole run of glyphs into one wide strip and indexes
 *   it by column (`base + offset[k]`, pitch 704/392).  Glide gives every CLASS
 *   its own fixed-stride window (`base + stride*k`), pitch 64 (large) or 32
 *   (small) -- the texture widths from the constructor call:
 *
 *     size    base         stride   pitch  rows  classes
 *     large   0x1007B618   0xA00     64     40   0..53
 *     small   0x1009D218   0x280     32     20   0..53
 *
 *   As with D3D the extents are arithmetic, not guesswork:
 *
 *     0x1007B618 + 54*0xA00 == 0x1009D218   (large -> small)
 *     0x1009D218 + 54*0x280 == 0x100A5918   (small -> the class map's first
 *                                            live entry, 0x100A58F7 + 0x21)
 *     0x100A58F7 + 0x80     == 0x100A5977   (class map -> the offset table
 *                                            at 0x100A5978)
 *
 *   A window is 64 (or 32) columns wide but a glyph is at most 50 (or 27), so
 *   each window's TAIL still holds the following glyphs -- the blob was cut
 *   from a wide strip like D3D's, one 64-column slice per class, and the
 *   slices overlap.  Nothing samples the tail: the emitter's G_SETTILESIZE
 *   stops at `offset[k+1] - offset[k] + 1` columns, exactly as in D3D.
 *
 *   The Glide metrics live at 0x100A58F7 (class map), 0x100A5978 (large
 *   offsets) and 0x100A5A58 (small offsets), and the ramps at 0x100A6C78 /
 *   0x100A6DB8 (large) and 0x100A6EF8 / 0x100A7038 (small).  All are
 *   byte-identical to the D3D tables at 0x100A5FEF / 0x100A6070 / 0x100A6150
 *   and 0x100A74B8 / 0x100A75F8 / 0x100A7738 / 0x100A7878.
 *
 * WHY THIS MODULE READS THE DLL INSTEAD OF CARRYING A LITERAL ARRAY
 *
 * br_data.c's convention is to write recovered bytes out as C initialisers.
 * That is right for tables of tens or hundreds of entries; at 72,000 bytes it
 * would be a third of a megabyte of source for data that is already in the
 * tree, byte for byte, in `orig/BRD3D.dll` and `orig/BRGlide.dll`. So this
 * module reads the real bytes at the addresses above out of the real image.
 * Nothing is invented and nothing is transcribed by hand -- the addresses,
 * pitches and cell heights above ARE the recovery record, and BrFontLoad
 * checks them: it decides WHICH build it is looking at by the class map's
 * sentinel values and the offset table's shape, and refuses an image where
 * neither build's addresses hold them.
 *
 * ==========================================================================
 * THE OTHER TEXTURE: an 8x40 shading ramp
 * ==========================================================================
 *
 * 0x10018590 also loads a second texture, once per string, from one of four
 * 320-byte blocks at 0x100A74B8 / 0x100A75F8 (large) and 0x100A7738 /
 * 0x100A7878 (small); 0x104B0360 picks between the two of a pair.  The Glide
 * emitter 0x10015B10 does the same from 0x100A6C78 / 0x100A6DB8 and
 * 0x100A6EF8 / 0x100A7038, selected by 0x104ABB48, and its four blocks are
 * byte-identical to D3D's.  Every byte
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
 * and 0x10015B10 builds it with the SAME sixteen tokens -- the two calls were
 * compared argument by argument (0x10015C25..0x10015C87 against the D3D site)
 * and differ in nothing, including the one token that varies with the ramp
 * selection.
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
 * 0xFA/0xFB (primitive / environment colour) -- plus 0xDD, which only the
 * Glide emitter produces -- and ignores the rest. That is a reference
 * consumer, not the backend.
 *
 * ==========================================================================
 * WHAT ACTUALLY DIFFERS BETWEEN THE TWO EMITTERS
 * ==========================================================================
 *
 * 0x10018590 (D3D, 2992 bytes) and 0x10015B10 (Glide) were read command by
 * command.  A note on the size, because it has already misled once:
 * `config/functions_glide.csv` splits the Glide emitter into 0x10015B10 (1019
 * bytes) and 0x10015F0B (2287), and 0x10015F0B is mid-flow -- it is the `add
 * ecx,8` a `je` at 0x10015ED6 jumps to.  The real function runs 0x10015B10 ..
 * 0x100166FA, i.e. 3050 bytes, followed by two jump tables and their two 0x4A
 * index tables ending exactly at 0x100167FA.  That is the same shape as D3D's
 * (see the note on 0x10019140 in CONVENTIONS.md), and it means the Glide
 * emitter is very slightly BIGGER than the D3D one, not a third of its size.
 *
 * IDENTICAL in both: the whole preamble (all seventeen commands and the
 * combine call's sixteen tokens), the whole epilogue, the space advance
 * `(14*scale)/40 + 1`, the pen advance `(scale*(w-1))/cell`, the tile width
 * `off[k+1]-off[k]+1`, the G_SETTILESIZE packing, the 0x140/0xF0 bounds test,
 * the clamp-at-zero-only out-of-bounds arm, the `%%`/`%i`/`%n`/`%x` escape
 * handling, and both 0x4A-entry colour index tables with both 12-entry colour
 * tables (all four compared byte for byte).
 *
 * Exactly two things differ, and only one of them is visible in the output:
 *
 *   1. THE DETAIL GLOBAL IS D3D-ONLY.  0x100185E7 tests `[0x100B8C90] > 1`
 *      and forces the small font when it holds.  0x10015B67 has no such test
 *      -- Glide picks the size from the scale alone.  The same 9 bytes are
 *      the entire difference between the two width routines (0x100193C0 is
 *      207 bytes, 0x10016980 is 198), which is a second, independent sighting
 *      of the same edit.
 *
 *   2. GLYPH BINDING.  D3D pre-registers 106 textures (0x10073820) into two
 *      handle tables and emits one 0xDC per glyph carrying that class's
 *      handle.  Glide registers TWO textures (0x1006C790), one per size, and
 *      emits a 0xDD carrying the ADDRESS of the class's texel window
 *      (`base + stride*k`, 0x10015FD0..0x10015FE3) immediately before the
 *      0xDC -- it re-points one texture per glyph instead of switching
 *      between many.
 *
 * The three quirks this header used to state as facts about "the game" were
 * re-checked against Glide.  All three HOLD in both builds:
 *
 *   - the bound tile is one column wider than the advance, and a space adds
 *     +1 that the width routine does not (0x10015FB7 `inc ecx`, 0x100161E1
 *     `lea ebp,[ebp+edx+1]`, against 0x100169EB and 0x10016A2A which add
 *     neither) -- so a caption still draws wider than it measures;
 *   - the out-of-bounds arm still clamps corners at zero only and lets a rect
 *     past 320/240 out unchanged (0x10016112..0x100161A6);
 *   - the strips are still stored BOTTOM-UP, so T sampling is still inverted.
 *     This one is now stronger than it was: the Glide blob matches the D3D
 *     strips in the SAME row order, and the Glide extents are pinned by their
 *     own arithmetic, so two independently-laid-out blobs agree that row 0 is
 *     the bottom.
 */
#ifndef BR_FONT_H
#define BR_FONT_H

#include <stddef.h>
#include <stdint.h>

/* ======================================================================
 * Recovered geometry.  Every one of these is an immediate read out of
 * 0x10018590 / 0x100193C0 / 0x10073820 (D3D) or 0x10015B10 / 0x10016980 /
 * 0x1006C790 (Glide); see the addresses in the comments.
 * ====================================================================== */

/* Which DLL a font was recovered from.  Selects the texel nibble order, the
 * glyph layout and the emitter's two divergences; see the header comment. */
#define BR_FONT_BUILD_D3D    0   /* orig/BRD3D.dll   -- IA8,  I high, A low */
#define BR_FONT_BUILD_GLIDE  1   /* orig/BRGlide.dll -- AI44, A high, I low */

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
 * chosen (D3D 0x100185F0 / 0x100185F7 / 0x10018618 and 0x100193DE; Glide
 * 0x10015B67 / 0x10015BAF / 0x10015B79 and 0x10016995 -- same constants). */
#define BR_FONT_LARGE_CELL  0x28
#define BR_FONT_SMALL_CELL  0x14
#define BR_FONT_LARGE_MIN   0x19

/* D3D strip pitches: one wide strip per (size, run). */
#define BR_FONT_LARGE_PITCH 704
#define BR_FONT_SMALL_PITCH 392

/* Glide window pitches and strides: one fixed-size window per class.  The
 * pitches are the texture widths 0x1006C790 passes to the constructor (0x40
 * and 0x20); the strides are the multipliers at 0x10015B9B / 0x10015BD1. */
#define BR_FONT_G_LARGE_PITCH   64
#define BR_FONT_G_SMALL_PITCH   32
#define BR_FONT_G_LARGE_STRIDE  0xA00
#define BR_FONT_G_SMALL_STRIDE  0x280
/* Classes 0..53 get a window; 54 is the offset table's terminator and has
 * none.  This count is what pins the extents (see the header comment). */
#define BR_FONT_G_CELLS         54

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

/* One block of glyph texels.
 *
 * D3D uses both entries of aStrip[size][]: a whole run packed into one wide
 * strip, indexed by COLUMN (`pTexels + off[k]`), with `stride` 0.
 *
 * Glide uses only aStrip[size][0]: all 54 windows back to back, indexed by
 * CLASS (`pTexels + stride*k`), with `pitch` 64/32 and `stride` 0xA00/0x280.
 * `height` is the cell height in both cases -- the rows that carry glyph. */
typedef struct BrFontStrip {
    uint8_t *pTexels;
    int32_t  pitch;
    int32_t  height;
    int32_t  stride;      /* Glide: bytes per class window.  D3D: 0. */
} BrFontStrip;

typedef struct BrFont {
    /* BR_FONT_BUILD_D3D or BR_FONT_BUILD_GLIDE, decided by BrFontLoad. */
    int32_t     build;
    /* D3D 0x100A5FEF + 0x21 / Glide 0x100A58F7 + 0x21, indexed
     * [c - BR_FONT_CLASS_LO].  Byte-identical between the builds. */
    signed char aClass[BR_FONT_CLASS_N];
    /* D3D 0x100A6070/0x100A6150, Glide 0x100A5978/0x100A5A58.  Cumulative x
     * offsets, and byte-identical between the builds. */
    int32_t     aOff[2][BR_FONT_CLASSES];
    /* [size][BR_FONT_STRIP_*]; Glide fills only [size][0]. */
    BrFontStrip aStrip[2][2];
    /* The original virtual address of each Glide block, so the 0xDD payload
     * the Glide emitter produces is the ORIGINAL address rather than an
     * invented token, and BrFontRasteriseDL can decode the class back out of
     * it.  Zero for a D3D font. */
    uint32_t    aBlockVa[2];
    /* [size][variant], variant selected by BrTextEmit::fAltRamp. */
    uint8_t     aRamp[2][2][BR_FONT_RAMP_BYTES];
    /* D3D: the analogue of 0x11829238 / 0x11829158, one handle per class.
     * The original stores whatever the backend's texture constructor
     * returned; the port stores an opaque token BrFontRasteriseDL can decode
     * back into (size, class).  Either way the drawing routine only ever
     * copies it into the low 24 bits of a 0xDC command, so the token has to
     * fit in 24 bits and must not be zero.
     *
     * Glide leaves this zero and uses ahPage instead -- it has no per-class
     * handle to store. */
    uint32_t    ahTex[2][BR_FONT_CLASSES];
    /* Glide: the analogue of 0x1184C47C (large) and 0x1184C46C (small), the
     * ONE texture per size that 0x1006C790 creates.  Zero for a D3D font. */
    uint32_t    ahPage[2];
} BrFont;

/* The port's stand-in for a backend texture handle.  All fit in 24 bits. */
#define BR_FONT_TOK_GLYPH(size, cls) \
    (0x00A00000u | ((uint32_t)(size) << 8) | (uint32_t)(cls))
#define BR_FONT_TOK_RAMP(size, variant) \
    (0x00B00000u | ((uint32_t)(size) << 4) | (uint32_t)(variant))
/* Glide only: the per-SIZE texture, re-pointed per glyph by the 0xDD. */
#define BR_FONT_TOK_PAGE(size) \
    (0x00C00000u | (uint32_t)(size))
#define BR_FONT_TOK_IS_GLYPH(t)  (((t) & 0x00F00000u) == 0x00A00000u)
#define BR_FONT_TOK_IS_RAMP(t)   (((t) & 0x00F00000u) == 0x00B00000u)
#define BR_FONT_TOK_IS_PAGE(t)   (((t) & 0x00F00000u) == 0x00C00000u)
#define BR_FONT_TOK_SIZE(t)      (int)(((t) >> 8) & 0xFu)
#define BR_FONT_TOK_CLASS(t)     (int)((t) & 0xFFu)
#define BR_FONT_TOK_PAGE_SIZE(t) (int)((t) & 0xFu)
#define BR_FONT_TOK_RAMP_SIZE(t) (int)(((t) >> 4) & 0xFu)
#define BR_FONT_TOK_RAMP_VAR(t)  (int)((t) & 0xFu)

/* Read every table and block above out of a copy of BRD3D.dll or BRGlide.dll.
 * Returns 0 on success and sets pFont->build to say which it found; the two
 * layouts share no address, so the probe cannot be ambiguous.  Allocates; call
 * BrFontFree.
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
 * Returns 0 for the gap class or an out-of-range one.
 *
 * `pTexels` is one byte per texel in BOTH builds, but the nibbles are the
 * other way round, so the two accessors below exist rather than an open-coded
 * shift.  Row 0 is the BOTTOM of the glyph in both builds. */
typedef struct BrGlyph {
    const uint8_t *pTexels;   /* row 0, column 0, inside the block */
    int32_t        pitch;
    int32_t        w, h;
    int32_t        fAlphaHigh;  /* 1 for Glide AI44, 0 for D3D IA8 */
} BrGlyph;
int  BrFontGlyph(const BrFont *pFont, int cls, int size, BrGlyph *pOut);

/* The two nibbles of a glyph texel, each 0..15.  `fAlphaHigh` comes straight
 * off a BrGlyph, so a consumer never has to know which build it holds. */
#define BR_FONT_TEXEL_A(t, fAlphaHigh) \
    ((fAlphaHigh) ? ((int)(t) >> 4) : ((int)(t) & 0xF))
#define BR_FONT_TEXEL_I(t, fAlphaHigh) \
    ((fAlphaHigh) ? ((int)(t) & 0xF) : ((int)(t) >> 4))

/* ======================================================================
 * 0x10018590 (D3D) / 0x10015B10 (Glide) -- emit a string as a display list
 * ======================================================================
 *
 * One structure serves both, because the two functions differ in exactly two
 * places (see the header comment).  `build` selects between them.  The Glide
 * globals sit at different addresses but hold the same things:
 *
 *     field         D3D          Glide
 *     x             0x104B0340   0x104ABB28
 *     y             0x104B0344   0x104ABB2C
 *     scale         0x104B0348   0x104ABB30
 *     fHiRes        0x106C65E4   0x106ED674
 *     detail        0x100B8C90   -- not read --
 *     fAltRamp      0x104B0360   0x104ABB48
 *     fAltColour    0x104B0358   0x104ABB40
 *     fUserColour   0x104B0364   0x104ABB4C
 *     env triple    0x100A74A8   0x100A6C68
 *     prim triple   0x104B0368   0x104ABB50
 *     f6C0258       0x106C0258   0x106E72E8
 *     gfx cursor    0x106C0680   0x106E7710
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
    /* BR_FONT_BUILD_*; BrTextEmitInit copies it from the font. */
    int32_t   build;

    uint32_t *pGfx;          /* 0x106C0680, advanced by 2 per command      */
    uint32_t *pGfxEnd;       /* DEVIATION: one past the last writable word */
    size_t    cWordsWanted;  /* DEVIATION: words the original would emit   */

    int32_t   x;             /* 0x104B0340 */
    int32_t   y;             /* 0x104B0344 */
    int32_t   scale;         /* 0x104B0348 -- the RENDERED cell height     */

    int32_t   fHiRes;        /* 0x106C65E4 -- doubles every coordinate     */
    /* D3D ONLY (0x100185E7).  The Glide emitter never reads it, so on a
     * Glide font this field is ignored however it is set. */
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

    /* D3D ONLY: the 0x11829238 / 0x11829158 tables.  NULL means "emit handle
     * 0", which the original would do before BrFontRegisterGlyphs had run. */
    const uint32_t *ahTexLarge;
    const uint32_t *ahTexSmall;

    /* GLIDE ONLY: 0x1184C47C / 0x1184C46C, the one texture per size, and the
     * base address and stride the 0xDD payload is built from
     * (0x10015BCD/0x10015BD1 and 0x10015B97/0x10015B9B). */
    uint32_t  hPageLarge, hPageSmall;
    uint32_t  vaBlockLarge, vaBlockSmall;
    uint32_t  strideLarge, strideSmall;

    /* The SETTIMG payloads for the two ramps of the selected size -- the
     * original's 0x100A74B8/0x100A75F8 and 0x100A7738/0x100A7878 (D3D) or
     * 0x100A6C78/0x100A6DB8 and 0x100A6EF8/0x100A7038 (Glide). */
    uint32_t  hRampLargeA, hRampLargeB;
    uint32_t  hRampSmallA, hRampSmallB;
} BrTextEmit;

/* 0x10018590 (D3D) or 0x10015B10 (Glide), chosen by pSt->build.  Emits the
 * string at (pSt->x, pSt->y).  Reads no globals. */
void BrTextEmitString(BrTextEmit *pSt, const char *psz);

/* 0x10073820 (D3D).  Fills pFont->ahTex.  The original calls the backend
 * texture constructor 106 times; the port has no backend here, so it plants
 * the token BrFontRasteriseDL understands.  The LOOP STRUCTURE is the
 * original's: four runs of 27, 26, 27, 26 over the two offset tables, leaving
 * class 27 -- the gap -- untouched in both size tables. */
void BrFontRegisterGlyphs(BrFont *pFont);

/* 0x1006C790 (Glide).  Fills pFont->ahPage.  The original makes exactly TWO
 * textures -- 64x64 and 32x32, both format 4 (GR_TEXFMT_ALPHA_INTENSITY_44) --
 * and copies the two blocks into the backend's staging buffer.  There are no
 * per-class handles to make: the emitter re-points these two. */
void BrFontRegisterPages(BrFont *pFont);

/* Point a BrTextEmit at a loaded font: copies `build`, fills whichever handle
 * set that build uses and the four ramp handles, and sets the fields the
 * original defaults.  NOT in the original, which reaches all of this through
 * fixed addresses. */
void BrTextEmitInit(BrTextEmit *pSt, const BrFont *pFont,
                    uint32_t *pGfx, size_t cWords);

/* 0x100193C0 (D3D) or 0x10016980 (Glide), chosen by pFont->build: the width
 * of `psz` at `scale`.  The two are the same 198 bytes of code apart from
 * D3D's `detail` test, and both sum `(off[k+1]-off[k])*scale/cell` per glyph
 * and `(14*scale)/40` per non-glyph -- NEITHER adds the +1 the emitter adds,
 * which is why a caption draws wider than it measures.
 *
 * slice6_76.c's BrSub_100193C0 is a separate, standalone port of the D3D
 * routine against its own transcribed copy of the tables.  It is NOT checked
 * against this one -- linking it here would drag in half that packet -- so if
 * one is corrected the other must be corrected by hand.  What
 * test_br_font_glide.c does check is that this routine gives the same answer
 * for both builds over a corpus of strings and scales, which is the claim
 * that the two ORIGINALS are the same code outside the detail arm. */
int32_t BrFontMeasure(const BrFont *pFont, const char *psz,
                      int32_t scale, int32_t fHiRes, int32_t detail);

/* ======================================================================
 * Reference rasteriser
 * ====================================================================== */

/* Walk an emitted display list and write RGBA8888 into a cx-by-cy target,
 * blending over what is there.  Returns the number of glyph rectangles drawn.
 *
 * Only 0xDC, 0xDD, 0xF2, 0xE3, 0xFA and 0xFB are interpreted; see the header
 * comment.  Rectangles are clipped against the target, which the original
 * does NOT do -- its own bounds test only clamps negative corners to zero
 * (D3D 0x10018B55, Glide 0x10016125: the same code) and lets a rectangle past
 * the right edge through unchanged. */
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
