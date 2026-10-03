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

/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
/* Port signatures take a BrFont*.  Originals write/read globals and take
 * fewer args: RegisterPages is void(void); Measure is (psz, scale). */
#define BrFontRegisterPages BrFontRegisterPages_Portable
#define BrFontMeasure BrFontMeasure_Portable
#define BrTextEmitString BrTextEmitString_Portable
#include "br_coretypes.h"   /* br_globals: its objects */
#include "slice1_05.h"   /* br_globals: its objects */
#include "slice2_16.h"   /* br_globals: its objects */
#include "br_font.h"
#undef BrFontRegisterPages
#undef BrFontMeasure
#undef BrTextEmitString
/* BrFontRegisterPages: prototype in br_funcs.h */
/* BrFontMeasure: prototype in br_funcs.h */
/* The original takes ONE stack argument and reads its state out of fourteen
 * absolute globals -- br_font.h's BrTextEmit is the port's gathering of them
 * and is the accessor sub-case docs/VC5-IDIOMS.md records. */
/* BrTextEmitString: prototype in br_funcs.h */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* slice1_05.h owns 0x1002F900.  Declared the way slice6_76.c declares it --
 * by opaque pointer -- so this file does not have to pull in a header that
 * models a dozen unrelated things.  BrGfxWords is {uint32_t w0, w1}, i.e. one
 * display-list command, which is exactly what the original reserves. */
struct BrGfxWords;
/* BrRdpSetCombineLERP: prototype in br_funcs.h */

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

/* (port-only br_u16le removed) */


/* (port-only br_u32le removed) */


/* (port-only br_pe_open removed) */


/* The bytes at `va`, or NULL if the image does not hold them (which is how
 * .bss is told from .data -- see br_data.c's note on the same test). */
/* (port-only br_pe_at removed) */


/* `cb` is the whole block: pitch*height for a D3D strip, stride*54 for a
 * Glide block.  `height` stays the CELL height in both -- the rows a glyph
 * occupies -- because that is what BrFontGlyph reports and what the emitter's
 * G_SETTILESIZE bounds. */
/* (port-only br_font_strip removed) */


/* (port-only BrFontFree removed) */


/* Which build's addresses hold the font in this image, or -1.
 *
 * The test is the class map's three sentinels plus the offset table's shape.
 * Those are not a checksum: they are the three entries the rest of this file
 * relies on ('0' is the tenth digit class, 'A' opens the letter run, 'a' folds
 * onto 'A') and the two-run structure the class map depends on.  The two
 * builds put their tables 0x6F8 bytes apart, so a probe that passes at one
 * address cannot also pass at the other by accident -- and the loader tries
 * both and requires exactly one to answer. */
/* (port-only br_font_probe removed) */


/* (port-only BrFontLoad removed) */


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
/* FUN_10001000: prototype in br_funcs.h */
/* FUN_10027fb0: prototype in br_funcs.h */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

void BrFontRegisterPages(void)
{
    int sum;
    DAT_1184c47c = BrTex3dCreateBlank(&(*(int *)&g_aBrFontBlockLarge), 0x40, 0x40, 4);
    sum = BrAdler32(0, 0, 0);
    sum = BrAdler32(sum, &(*(int *)&g_aBrFontBlockLarge), 0x21C00);
    DAT_1184c46c = BrTex3dCreateBlank(&(*(int *)&g_aBrFontBlockSmall), 0x20, 0x20, 4);
    BrAdler32(sum, &(*(int *)&g_aBrFontBlockSmall), 0x8700);
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
/* (port-only BrFontRegisterGlyphs removed) */


/* (port-only BrFontClassOf removed) */


/* (port-only BrFontGlyph removed) */


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

/* (port-only br_emit removed) */


/* The original's colour packer, verbatim: the FIRST component is not masked,
 * so a value above 255 bleeds into the bits above and is shifted out. */
/* (port-only br_pack_rgb removed) */


/* 0x10018B68 / 0x10018B8A / 0x10018BB7 / 0x10018BCD, all the same shape:
 *
 *     shl eax,2 / sar eax,2 / test ax,ax / jle -> 0 / movsx eax,ax
 *
 * The shift pair only clears bits 30 and 31, and everything after it looks at
 * AX alone, so the low sixteen bits are unaffected and the pair is elided
 * here.  The test is on AX SIGNED: a coordinate whose low word is negative or
 * zero collapses to 0, and one whose low word is positive survives
 * sign-extended -- so 0x10000 clamps to 0, not to 0x10000. */
/* (port-only br_clamp_lo16 removed) */


/* (port-only BrTextEmitInit removed) */


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

/* ==========================================================================
 * THE MATCHING ARM -- 0x10015B10, the Glide emitter
 *
 * The port arm below gathers fourteen absolute globals behind a BrTextEmit
 * pointer and routes every command through a helper.  The original does
 * neither: it takes ONE stack argument (the string), reads each global at its
 * own address, and inlines the eight-byte append at all 43 sites.  Both of
 * those are cause classes docs/VC5-IDIOMS.md names, and between them they are
 * the whole 1,097-byte shortfall.
 *
 * The colour codes are a pair of SWITCHES, not a table lookup: the original
 * carries a compiler-built byte map and jump table just past the function
 * (0x10016730 / 0x100166FC and 0x100167B0 / 0x1001677C), which is what the
 * port's s_aColourCase / s_aPrimColour / s_aEnvColour arrays are a hand
 * transcription of.  Written back as switches so VC5 rebuilds them.
 *
 * The DEVIATIONs the port arm documents -- the cursor bound check, the class
 * bound check, the zeroed sscanf slots, the stop-at-NUL step -- are all
 * absent here, because the original does not have them.
 *
 * STATE (2026-09-04): 751 of 751 instructions, every divergence.py region
 * delta 0 at key 10, whole-function register-blind multiset 6+6, sweep row
 * 420 positional diff bytes (was 2,294).  Compare the /O2 object: the
 * original has NO frame pointer (ebp is penX), so the /Oy- row the report
 * used to prefer was the wrong variant.  (fn.py's BYTES/INSNS still include
 * the four switch tables VC5 keeps in the COMDAT; read the code up to the
 * `ret`.)
 *
 * ================= RESIDUE: SLOT ASSIGNMENT, ONE PROMOTION ===============
 *
 * The frame is now the original's 0x2c and the allocation matches: pOff in
 * ebx with a home at +0x38, scale memory-homed, penX in ebp.  What is left:
 *
 *   1. Two locals sit in the wrong slots: the original has scale +0x10 and
 *      b +0x14, ours b +0x10 and scale +0x14.  Every other slot now agrees
 *      (r +0x18, top +0x1c, p +0x20, q +0x24, ... pOff +0x38, `g` packed
 *      into the dead parameter slot +0x40) -- that came from SCOPING `g`
 *      inside the %x arm, see the comment there.  Function-scope
 *      DECLARATION order is inert (three orders, including the original's
 *      slot order, byte-identical), and so is the order inside the inner
 *      declaration (all six).  Scoping b inside as well changes nothing;
 *      scoping r inside flips the colour pack to g-first and the switch
 *      index to ecx (+750 B), so r is function-scope in the original.
 *   2. In the glyph block the original promotes `stride` into ebx for the
 *      window address (`mov ebx,[stride] / imul ebx,edx / add ebx,[vaBlock]`)
 *      and loads `cell` only after the DC put; ours promotes `vaBlock`
 *      (`imul ecx,[stride]` early, `mov ebx,[vaBlock] / add ecx,ebx`).  The
 *      four spellings of `vaBlock + stride * cls` (either order, with and
 *      without the cast) are byte-identical, as are declaring stride before
 *      vaBlock and assigning it first in the arms (+12 diffs).
 *   3. The clamp arm keeps `top` in esi across both clamps (`lea eax,[esi*4]`);
 *      ours reloads it, a consequence of 2.
 *
 * DEAD PROBES -- do not re-run:
 *   - declaring `pOff` first / `scale` last; `register` on pOff
 *   - laying the declarations out in the original's slot order (twice: with
 *     scale in ebx and again with the frame right)
 *   - swapping the `|` operands of the second E3 word (canonicalised)
 *   - the four spellings of the window address, and the stride/vaBlock
 *     declaration and assignment orders (above)
 *   - the eight r/g/b scopings: all three inner (1163 B), any split with r
 *     inner (1163-1168 B), g inner alone or with b (411 B, kept)
 *
 * The N64 twin is 0x8022E4E0 in Top Gear Rally (paired by the six display
 * list constants it builds -- 0xB900, 0xBA00, 0xF510, 0xF568, 0xE700,
 * 0x0C18); it confirms the three globals, (30*scale)/40, the `scale < 25`
 * selection, the 20/40 cell and the two ramps.  It is a structure oracle
 * only: its coordinate packing is different and it cannot speak to x86
 * allocation.
 * ==========================================================================
 *
 * SOLVED, and each is now an entry in docs/VC5-IDIOMS.md:
 *   - the fourteen globals, and the ONE stack argument
 *   - the eight-byte append as a macro at all 43 sites
 *   - the colour codes as two SWITCHES, not three hand-written tables
 *   - the hi-res doubling read off the GLOBALS, not off the locals
 *     (`lea ebp,[edi+edi]` off the still-live load; the N64's local form
 *     is IDO copy-propagation, not the source)
 *   - the combine call as TWO CALLS with the slot taken inside each arm
 *   - the guard sums written INLINE at all four sites (`penX + drawW`,
 *     `top + scale`): global CSE carries them into the pass arm, the clamp
 *     arm recomputes them because they are only partially available there.
 *     A named local hoists them above the guard.
 *   - a NAMED `adv = w - 1` right after `w`: it holds edi across the glyph
 *     block, and that extra callee-saved demand is what flips the allocator
 *     from scale-in-ebx to pOff-in-ebx -- the whole 0x28 -> 0x2c frame.
 *   - the escape arms as the ELSE of the "%%" test, each ending in
 *     `goto next`: arms that never rejoin go out of line past the `ret`;
 *     a `goto` to a label placed after `return` is laid out INLINE.
 *   - `do { } while (*p != '\0')` with the increments before the test.
 * ========================================================================== */

/* 0x106E7710, the display-list cursor.  slice2_18's object under the Glide
 * build's number; a second model of it would be the aliased-storage bug. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* The eight-byte append, inlined at every site as the same five instructions.
 * A MACRO for the reason br_drawcar.c records: MSVC 5.0 will not inline a
 * static with more than one caller, and 43 calls is 43 too many. */
#define put(w0_, w1_)                                                    \
    do { uint32_t *p_ = g_BrGfxPtr;                                      \
         g_BrGfxPtr += 2;                                                \
         p_[0] = (w0_);                                                  \
         p_[1] = (w1_); } while (0)

/* The combine command's slot: the original advances the cursor and hands the
 * builder the OLD slot, so a caller reading the cursor mid-flight sees it
 * already moved.  It has to be a NAMED TEMP -- a comma expression that
 * subtracts the step back off costs an `add eax,-8` the original does not
 * have. */

/* The combine command's slot, taken as an argument expression so the cursor
 * advance is duplicated into both arms of the ramp test. */
#define BR_PUT_SLOT()                                                    \
    (pCombine = (struct BrGfxWords *)g_BrGfxPtr,                         \
     g_BrGfxPtr += 2, pCombine)

/* The colour packer, inlined at four sites.  The FIRST component is not
 * masked, so a value above 255 bleeds upward and is shifted out. */
#define BR_PACK(r_, g_, b_)                                              \
    (((((((uint32_t)(r_) << 8) | ((uint32_t)(g_) & 0xFFu)) << 8)         \
        | ((uint32_t)(b_) & 0xFFu)) << 8) | 0xFFu)

/* A screen coordinate as the original writes it: the shift pair only clears
 * bits 30 and 31 and the mask makes it a no-op, but it is in the bytes at
 * every one of the eight field sites and eliding it was what made the port
 * arm's rectangles shorter than the original's. */
#define BR_F12(v)     ((uint32_t)(((v) << 2) >> 2) & 0xFFFu)

/* The same shift pair, then a SIGNED 16-BIT test: a coordinate whose low word
 * is negative or zero collapses to 0, one whose low word is positive survives
 * sign-extended.  So 0x10000 clamps to 0, not to 0x10000. */
#define BR_CLAMP16(v)                                                    \
    (((int16_t)(((v) << 2) >> 2) > 0)                                    \
        ? (int32_t)(int16_t)(((v) << 2) >> 2) : 0)

/* 64-bit core: declared once, in br_globals.h or its struct's header */              /* 0x104ABB28 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */              /* 0x104ABB2C */
/* 64-bit core: declared once, in br_globals.h or its struct's header */          /* 0x104ABB30 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */          /* 0x106ED674 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */      /* 0x104ABB40, a BYTE            */
/* 64-bit core: declared once, in br_globals.h or its struct's header */        /* 0x104ABB48, a BYTE            */
/* 64-bit core: declared once, in br_globals.h or its struct's header */     /* 0x104ABB4C, a DWORD           */
/* 64-bit core: declared once, in br_globals.h or its struct's header */          /* 0x104ABB50 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */          /* 0x104ABB54 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */          /* 0x104ABB58 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */           /* 0x100A6C68 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */           /* 0x100A6C6C */
/* 64-bit core: declared once, in br_globals.h or its struct's header */           /* 0x100A6C70 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */         /* 0x106E72E8, the 0xBA000C02    */
/* 64-bit core: declared once, in br_globals.h or its struct's header */       /* 0x1184C46C */
/* 64-bit core: declared once, in br_globals.h or its struct's header */       /* 0x1184C47C */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x100A5978 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x100A5A58 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */      /* 0x100A5918, [c - 0x21]   */
/* The four ramp blocks and the two glyph blocks are referenced by ADDRESS --
 * `mov reg, OFFSET`, never a load -- so they are arrays, not pointers. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */  /* 0x100A6C78 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */  /* 0x100A6DB8 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */  /* 0x100A6EF8 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */  /* 0x100A7038 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */  /* 0x1007B618 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */  /* 0x1009D218 */

/* WHAT IT DOES: draw a string of text into the display list -- walks the
 * characters, looks each glyph's cell up in the font page, and emits the
 * texture and rectangle commands to paint it at the current pen position,
 * colour and scale. The engine's only text output path. */
/* @t4-pass 0x10015B10 1 2026-09-07 probes 119 bytes 3306 insns 875 regions 9 rows 136 census yes  (tools/crank.py) */
/* @t4-pass 0x10015B10 2 2026-09-07 probes 119 bytes 3306 insns 875 regions 9 rows 136 census yes  (tools/crank.py) */
/* @t4-pass 0x10015B10 3 2026-09-09 probes 11 bytes 3306 insns 752 regions 13 rows 12 census yes
 *   (table-aware gates: insn gap 0, tables byte-equal at +0xbec.  Named/split
 *    window-address temps, stride typing and declaration slot, cls/w typing,
 *    drawW placement, cr/cb order, adv respelling: 9 byte-identical, 2
 *    regressions.  Census: code-zone mnemonic histograms equal except the
 *    classified lea~shl fork; call 2/2, imul 5/5, idiv 2/2, jmp 37/37.) */
/* @t4-pass 0x10015B10 4 2026-09-09 probes 10 bytes 3306 insns 752 regions 13 rows 12 census yes
 *   (hPage-mask hoist, guard-sum commutes, E3 first-word operand swap, named
 *    pen-advance, p/q increment order, cell<<2, w*0x4000, drawW parens,
 *    clamp-mask spelling: 8 byte-identical, 2 regressions, 0 improvements.) */
/* @t3 0x10015B10 2026-09-09 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 3306/3306 insns 752/752 rows 6+6 regions 13 oracle UNCLASSIFIED
 * @t3-effort passes 4 zero-movement 3 4
 * Size- and insn-exact, tables byte-equal.  The residue is the slot pair
 * (scale/b transposed at +0x10/+0x14) and the stride-vs-vaBlock promotion in
 * the glyph window, whose downstream echo is the one classified lea~shl fork
 * in the clamp arm -- see RESIDUE and DEAD PROBES in this header.
 * Do not reopen before the end-grind. */
/* @implements 0x10015B10 glide BrTextEmitString */
void BrTextEmitString(const char *psz)
{
    /* Declaration order does NOT set the slots here -- laying these out in
     * the original's slot order changed nothing, with either frame.  See
     * the residue note above. */
    const int32_t *pOff;
    struct BrGfxWords *pCombine;
    const char    *p, *q;
    uint32_t       hRampA, hRampB, hPage, vaBlock, stride;
    int32_t        penX, top, cell, scale;
    uint32_t       r, b;        /* `g` is scoped INSIDE the %x arm */

    plat_text_emit(psz);   /* the script player's waittext (not in the original) */
    scale = (*(int32_t *)&DAT_104abb30);                          /* 0x10015B16 */
    penX  = (*(int32_t *)&DAT_104abb28);                              /* 0x10015B1D */
    top   = (*(int32_t *)&DAT_104abb2c) - (30 * (*(int32_t *)&DAT_104abb30)) / 40;  /* 0x10015B23 */
    /* scale and penX doubled from the GLOBALS, top from the local.  penX is
     * in ebp in both builds and the original writes `mov edi,[x] / mov
     * ebp,edi` then `lea ebp,[edi+edi]` -- the global's load kept alive for
     * a second read -- where `penX <<= 1` is `shl ebp,1`.  The N64 twin's
     * `sll` off the locals is IDO's copy propagation, not the source. */
    if ((*(int32_t *)((char *)&g_aBrEntRecs + 0x44)) != 0) {                       /* 0x10015B4F */
        top   <<= 1;
        scale = (*(int32_t *)&DAT_104abb30) * 2;
        penX  = (*(int32_t *)&DAT_104abb28) * 2;
    }

    if (scale < BR_FONT_LARGE_MIN) {                /* 0x10015B67 */
        cell    = BR_FONT_SMALL_CELL;
        pOff    = g_aBrFontOffSmall;
        hRampA  = br_addr32(g_aBrFontRampSmallA);
        hRampB  = br_addr32(g_aBrFontRampSmallB);
        hPage   = (*(uint32_t *)&DAT_1184c46c);
        vaBlock = br_addr32(g_aBrFontBlockSmall);
        stride  = 0x280u;
    } else {
        cell    = BR_FONT_LARGE_CELL;
        pOff    = g_aBrFontOffLarge;
        hRampA  = br_addr32(g_aBrFontRampLargeA);
        hRampB  = br_addr32(g_aBrFontRampLargeB);
        hPage   = (*(uint32_t *)&DAT_1184c47c);
        vaBlock = br_addr32(g_aBrFontBlockLarge);
        stride  = 0xA00u;
    }

    put(0xE7000000u, 0u);                           /* 0x10015BD9 */
    put(0xBA001402u, 0x00100000u);

    /* Only b1 depends on the ramp selection -- but it is TWO CALLS, not one
     * with a ternary: `? 1001 : 0` is a value VC5 makes branchlessly
     * (`neg al / sbb eax,eax / and eax,0x3e9`), and the original branches and
     * duplicates the seven pushes from the end down to b1, cross-jumping the
     * rest. */
    /* The slot is taken INSIDE each arm, as the call's first argument -- and
     * because cdecl evaluates right to left, its `add eax,8` / `mov [cur],eax`
     * land between the pushes.  Hoisting it above the `if` lets VC5 hoist the
     * six common pushes with it and cross-jump the arms down to one. */
    if ((*(char *)&g_4B0360) != 0) {
        BrRdpSetCombineLERP(BR_PUT_SLOT(),          /* 0x10015C87 */
                            1003, 1005, 1002, 1005,
                            0,    0,    0,    1001,
                            1000, 1001, 1002, 0,
                            0,    0,    0,    1002);
    } else {
        BrRdpSetCombineLERP(BR_PUT_SLOT(),
                            1003, 1005, 1002, 1005,
                            0,    0,    0,    1001,
                            1000, 0,    1002, 0,
                            0,    0,    0,    1002);
    }

    put(0xB900031Du, 0x0C184240u);                  /* 0x10015CA2 */
    put(0xBA000C02u, g_BrEnvOthermode);
    put(0xBA000E02u, 0u);
    put(0xBA001301u, 0u);
    put(0xBA001001u, 0u);
    put(0xBB000001u, 0xFFFFFFFFu);
    put(0xE8000000u, 0u);
    put(0xE6000000u, 0u);
    put(0xE7000000u, 0u);

    /* The shading ramp: SETTILE(load tile 7) / SETTIMG / LOADBLOCK /
     * SETTILE(render tile 1) / SETTILESIZE. */
    put(0xF51001B0u, 0x07000000u);                  /* 0x10015D9A */
    put(0xFD100000u, ((*(char *)&g_4B0360) != 0) ? hRampB : hRampA);
    put(0xF3000000u, 0x0713F000u);
    put(0xF56803B0u, 0x01098030u);
    put(0xF2002002u, 0x0101E09Eu);

    if ((*(int32_t *)&DAT_104abb4c) != 0) {                  /* 0x10015E29 */
        put(0xFB000000u, BR_PACK((*(int32_t *)&DAT_100a6c68), (*(int32_t *)&DAT_100a6c6c), (*(int32_t *)&DAT_100a6c70)));
        put(0xFA00FFFFu, BR_PACK((*(int32_t *)&DAT_104abb50), (*(int32_t *)&DAT_104abb54), (*(int32_t *)&DAT_104abb58)));
    } else if ((*(char *)&g_br4B0358) != 0) {            /* 0x10015EC7 */
        put(0xFB000000u, 0xFF7F00FFu);
        put(0xFA00FFFFu, 0xFFFF7FFFu);
    } else {
        put(0xFB000000u, 0xC80000FFu);
        put(0xFA00FFFFu, 0xE6E600FFu);
    }

    p = psz;                                        /* 0x10015F42 */

    /* The emptiness test reads the PARAMETER, not `p` (`mov cl,[eax]` with
     * eax still holding psz), which is what keeps psz live past `p = psz`
     * and stops VC5 giving `p` the parameter's own stack slot. */
    if (*psz != '\0') {
        /* `q` is two characters ahead throughout; the escape paths step
         * both.  Set INSIDE the emptiness test -- the original computes it
         * after the `je` to the epilogue. */
        q = psz + 2;
        do {
            if (*p != ' ') {                        /* 0x10015F63 */
                /* ONE chain, not two `*p == '%'` tests: not a percent, or a
                 * percent at the end of the string, both fall straight into
                 * the glyph.  The escape arms are the ELSE of the "%%" test
                 * and every one of them ends in `goto next`, which is what
                 * puts them OUT OF LINE past the epilogue (0x10016256 follows
                 * the `ret` at 0x10016255): the "%%" arm falls into the
                 * glyph, the arms that never rejoin are deferred to the end. */
                if (*p == '%' && p[1] != '\0') {
                    if (p[1] == '%') {
                        ++p;                        /* "%%": swallow one */
                        ++q;
                    } else if (p[1] == 'i') {       /* 0x10016256 */
                        ++p;
                        ++q;
                        goto next;
                    } else if (p[1] == 'n') {
                        ++p;
                        ++q;
                        goto next;
                    } else if (p[1] == 'x') {       /* 0x10016269 */
                        /* Block-scoped, and ONLY this one: under /O2 a
                         * block-scoped local is homed AFTER the function-
                         * scope ones, which is what packs `g` into the dead
                         * parameter slot (+0x40) and shifts r/top/p up into
                         * +0x18/+0x1c/+0x20.  Scoping `r` here too flips
                         * the pack's accumulator to g-first (+750 B). */
                        uint32_t g;
                /* Six hex digits out of `q`, the character after "%x".  A
                 * short or malformed field leaves these slots holding
                 * whatever was in them. */
                sscanf(q, "%02x%02x%02x", &r, &g, &b);
                put(0xFA00FFFFu, BR_PACK(r, g, b));

                /* The same triple brightened by 0x80 and saturated at 0xFF
                 * becomes the other end of the gradient, so "%xRRGGBB" sets
                 * both ends from one value. */
                r += 0x80u;
                g += 0x80u;
                b += 0x80u;
                if (r > 0xFFu) r = 0xFFu;
                if (g > 0xFFu) g = 0xFFu;
                if (b > 0xFFu) b = 0xFFu;

                /* Seven characters, unconditionally -- which walks off the end
                 * of a truncated "%x" field. */
                p += 7;
                q += 7;
                put(0xFB000000u, BR_PACK(r, g, b));
                        goto next;
                    } else if (*q != '\0') {        /* 0x10016367 */
            put(0xE7000000u, 0u);

            /* The first letter sets the primitive colour, the second the
             * environment colour -- the two ends of the gradient.  An
             * unrecognised letter changes neither and still eats both. */
            switch (p[1]) {                         /* 0x1001639C */
            case 'r': put(0xFA00FFFFu, 0xBE0000FFu); break;
            case 'o': put(0xFA00FFFFu, 0xCD5F00FFu); break;
            case 'O': put(0xFA00FFFFu, 0xFF7800FFu); break;
            case 'y': put(0xFA00FFFFu, 0xFFF500FFu); break;
            case 'Y': put(0xFA00FFFFu, 0xFFFA80FFu); break;
            case 'g': put(0xFA00FFFFu, 0x009600FFu); break;
            case 'b': put(0xFA00FFFFu, 0x0000C8FFu); break;
            case 'p': put(0xFA00FFFFu, 0xC800C8FFu); break;
            case '1':
            case 'w': put(0xFA00FFFFu, 0xFFFFFFFFu); break;
            case '5': put(0xFA00FFFFu, 0x808080FFu); break;
            case '0': put(0xFA00FFFFu, 0x000000FFu); break;
            }
            switch (*q) {                           /* 0x1001652A */
            case 'r': put(0xFB000000u, 0xC80000FFu); break;
            case 'o': put(0xFB000000u, 0xCD5F00FFu); break;
            case 'O': put(0xFB000000u, 0xFF7800FFu); break;
            case 'y': put(0xFB000000u, 0xD2BE00FFu); break;
            case 'Y': put(0xFB000000u, 0xD2C869FFu); break;
            case 'g': put(0xFB000000u, 0x009600FFu); break;
            case 'b': put(0xFB000000u, 0x0000C8FFu); break;
            case 'p': put(0xFB000000u, 0xC800C8FFu); break;
            case '1':
            case 'w': put(0xFB000000u, 0xFFFFFFFFu); break;
            case '5': put(0xFB000000u, 0x808080FFu); break;
            case '0': put(0xFB000000u, 0x000000FFu); break;
            }
                        p += 2;
                        q += 2;
                        goto next;
                    }
                    /* one letter left: draw the '%' instead */
                }
                {
                    signed char sc = (signed char)*p;

                    /* Signed compares, so 0x80..0xFF are outside. */
                    if (sc >= BR_FONT_CLASS_LO && sc <= BR_FONT_CLASS_HI) {
                        int32_t  cls = g_aBrFontClass[sc - BR_FONT_CLASS_LO];
                        int32_t  w   = pOff[cls + 1] - pOff[cls] + 1;
                        int32_t  adv = w - 1;
                        int32_t  drawW;

                        /* ONE texture per size, re-aimed at this class's
                         * window by the 0xDD in front of the 0xDC. */
                        put(0xDD000000u | (hPage & 0x00FFFFFFu),
                            vaBlock + stride * (uint32_t)cls);
                        put(0xDC000000u | (hPage & 0x00FFFFFFu), 1u);
                        put(0xDE000000u, 0x3F800000u);   /*  1.0f */
                        put(0xDF000000u, 0xBF800000u);   /* -1.0f */

                        /* SETTILESIZE, tile 0, 10.2: uls = ult = 0.5,
                         * lrs = w - 0.5, lrt = cell - 0.5. */
                        put(0xF2002002u,
                            ((((uint32_t)w << 14) - 0x2000u) & 0x00FFF000u)
                                | ((uint32_t)(cell * 4 - 2) & 0xFFFu));

                        drawW     = (scale * w) / cell;

                        if (penX >= 0 && penX + drawW <= 0x140 &&
                            top >= 0 && top + scale <= 0xF0) {
                            put(0xE3000000u | (BR_F12(penX + drawW) << 12)
                                            | BR_F12(top + scale),
                                (BR_F12(penX) << 12) | BR_F12(top));
                        } else {
                            /* The SAME rectangle with every corner clamped at
                             * zero and ONLY at zero -- past the right or the
                             * bottom edge it still goes out unchanged, so this
                             * is not a scissor. */
                            int32_t cr = BR_CLAMP16(penX + drawW);
                            int32_t cb = BR_CLAMP16(top + scale);

                            put(0xE3000000u
                                    | ((uint32_t)(cr & 0xFFF) << 12)
                                    | (uint32_t)(cb & 0xFFF),
                                ((uint32_t)(BR_CLAMP16(penX) & 0xFFF) << 12)
                                    | (uint32_t)(BR_CLAMP16(top) & 0xFFF));
                        }

                        /* The pen advances by ONE LESS than the tile width --
                         * the quantity the width routine sums. */
                        penX += (scale * adv) / cell;
                    }
                }
            } else {
                /* (14 * scale) / 40, PLUS ONE.  The width routine computes the
                 * same quotient and does not add the one, so a string with
                 * spaces measures narrower than it draws. */
                penX += (14 * scale) / 40 + 1;      /* 0x100161C3 */
            }

        next:
            ++p;                                    /* 0x100161E5 */
            ++q;
        } while (*p != '\0');
    }

    put(0xE7000000u, 0u);                           /* 0x100161FC */
    put(0xBA001301u, 0x00080000u);
    put(0xBA001402u, 0u);
}

#undef put
#undef BR_PUT_SLOT
#undef BR_PACK
#undef BR_F12
#undef BR_CLAMP16


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
/* WHAT IT DOES: measure how wide a string would be if drawn, by summing each
 * character's advance at the given scale. Used to centre and right-align
 * text without drawing it first. */
/* @t4-pass 0x10016980 1 2026-09-07 probes 79 bytes 197 insns 79 regions 6 rows 6 census yes  (tools/crank.py) */
/* @t4-pass 0x10016980 2 2026-09-07 probes 79 bytes 197 insns 79 regions 6 rows 6 census yes  (tools/crank.py) */
/* @t4-pass 0x10016980 3 2026-09-10 probes 40 bytes 199 insns 79 regions 5 rows 0 census yes  (tools/crank.py) */
/* @t4-pass 0x10016980 4 2026-09-10 probes 40 bytes 199 insns 79 regions 5 rows 0 census yes  (tools/crank.py) */
/* @t4-pass 0x10016980 5 2026-09-10 probes 40 bytes 199 insns 79 regions 5 rows 0 census yes  (tools/crank.py) */
/* @t3 0x10016980 2026-09-10 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 199/198 insns 79/79 rows 0+0 regions 5 oracle EQUIVALENT
 * @t3-effort passes 5 zero-movement 4 5
 * residue after tools/crank.py: 40 compiles this pass, levers accepted: none;
 * every candidate and score is in build/match/crank.log.
 * Do not reopen before the end-grind. */
/* @implements 0x10016980 glide BrFontMeasure */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

int32_t BrFontMeasure(const char *psz, int32_t scale)
{
    int total;
    int s;
    int *pOff;

    total = 0;
    /* The doubling is spelled on `scale` before `s` takes its value: VC5 gives
     * a self-doubling `s <<= 1` an `add R,R`, where the original has `shl R,1`.
     * `scale` is dead here either way -- both arms below overwrite it. */
    if ((*(int *)((char *)&g_aBrEntRecs + 0x44)) != 0)
        scale = scale << 1;
    s = scale;
    if (s < BR_FONT_LARGE_MIN) {
        scale = BR_FONT_SMALL_CELL;
        pOff = (*(int (*)[])&g_aBrFontOffSmall);
    } else {
        scale = BR_FONT_LARGE_CELL;
        pOff = (*(int (*)[])&g_aBrFontOffLarge);
    }

    /* THE CHARACTER IS CACHED IN A LOCAL and every test reads the local.  The
     * original keeps *psz in al across the whole body: psz[1] goes to cl, the
     * percent test is `cmp cl,al` (register against register, not against
     * 0x25) and the glyph index is `movsx eax,al`, not a re-read of [esi].
     * Re-dereferencing costs all three (register-blind 2+2 with the shift
     * fixed; 0+0 with this).  The earlier note here said caching made VC5
     * strength-reduce psz[1]/psz[2] into walking pointers -- it does not when
     * only the character itself is named. */
    while (*psz != '\0') {
        const char c = *psz;

        if (c < (char)BR_FONT_CLASS_LO ||
            c > (char)BR_FONT_CLASS_HI) {
            total += (14 * s) / 40;
        } else if (c == '%' && psz[1] != '\0') {
            if (psz[1] == c) {
                psz++;
                goto glyph;
            }
            if (psz[1] == 'i' || psz[1] == 'n') {
                psz++;
            } else if (psz[2] != '\0') {
                psz += 2;
            } else {
                goto glyph;
            }
        } else {
        glyph:
            total += ((pOff[DAT_100a58f7[c] + 1] -
                       pOff[DAT_100a58f7[c]]) * s) / scale;
        }
        psz++;
    }

    if ((*(int *)((char *)&g_aBrEntRecs + 0x44)) != 0)
        total >>= 1;
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

/* (port-only br_unpack removed) */


/* A 4-bit IA nibble spans 0..15; 17 maps it onto 0..255 exactly. */
#define BR_FONT_N4(x)  ((int32_t)(x) * 17)

/* (port-only br_blend removed) */


/* (port-only BrFontRasteriseDL removed) */



/* 0x10073980
 *
 * Fourteen constant arguments through the backend texture constructor at
 * 0x118AA0B0 -- cdecl, last-arg-first: 0x20 x 0x40, fmt 0, siz 4, source
 * 0x100B9CB0, result stored at 0x11829108.  Same shape as 0x100739B0. */
/* WHAT IT DOES: turns a baked-in 32-by-64 picture into a texture the rest of
 * the game can draw with, and remembers the handle the graphics backend
 * returns. */
/* @implements 0x10073980 d3d BrSub10073980 */
typedef uint32_t (*BrSub10073980Fn)(void *pSrc, int a2, int w, int h,
                                 int fmt, int siz, int b31, int b30,
                                 int b29, int b28, int a11, int a12,
                                 int a13, int a14);

/* 64-bit core: declared once, in br_globals.h or its struct's header */     /* 0x118AA0B0 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */     /* 0x11829108 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* 0x100B9CB0 */

void BrSub10073980(void)
{
    DAT_1184c468 = (*(BrSub10073980Fn *)&(*(funcptr *)&g_pfn18AA0B0))(g_0B9CB0, 0, 0x20, 0x40, 0, 4,
                          0, 0, 0, 0, 0, 0, 1, 0);
}

/* -- Ghidra-matched functions --------------------------- */
/* funcptr: br_coretypes.h */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* FUN_10027fb0: prototype in br_funcs.h */
/* FUN_10001190: prototype in br_funcs.h */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* WHAT IT DOES: create the primary font texture via the registered texture-loader callback. */
/* @implements 0x1006C750 glide BrFontTexCreate */

int BrFontTexCreate(void)

{
  DAT_1184c470 = (*(*(funcptr *)&g_pfn18AA0B0))(&DAT_100ba2d0,0,0x40,0x40,1,4,0,0,1,1,0xf,0xf,1,0);
  (*(int *)&g_BrEnvTexDefault) = DAT_1184c470;
  return;
}

/* WHAT IT DOES: create an alternate 64x64 font texture. */
/* @implements 0x1006C800 glide BrFontTexCreateAlt */

int BrFontTexCreateAlt(void)

{
  DAT_1184c484 = BrTex3dCreateBlank(&DAT_100b64b0,0x40,0x40,2);
  return;
}

/* WHAT IT DOES: free all font glyph textures and the sheet texture. */
/* @implements 0x10058300 glide BrFontTexFreeAll */

void BrFontTexFreeAll(void)

{
  Img *piVar1;
  int iVar2;

  iVar2 = 0;
  if ((unsigned short)DAT_10ac5c2c > 0) {
    piVar1 = g_img;
    do {
      if (piVar1->surf != 0) {
        BrSurfFree(piVar1->surf);
        piVar1->surf = 0;
      }
      iVar2 = iVar2 + 1;
      piVar1 = piVar1 + 1;
    } while (iVar2 < (int)(DAT_10ac5c2c & 0xffff));
  }
  if (DAT_10ac5d84 != 0) {
    BrSurfFree(DAT_10ac5d84);
    DAT_10ac5d84 = 0;
  }
  return;
}

/* WHAT IT DOES: free all font textures, then exit the process with code 0 (CRT exit
 * through the import table). */
/* @implements 0x10058360 glide BrFontFreeAndExit */

int BrFontFreeAndExit(void)

{
  BrFontTexFreeAll();
  exit(0);
  return 0;
}

/* The original's clipped-sprite blitter takes six arguments; the port's
 * br_uispr.h prototype takes seven, so it is declared unprototyped here. */
/* BrUiSprClip: prototype in br_funcs.h */

/* WHAT IT DOES: draw one clipped sprite glyph from the font sheet. */
/* @implements 0x10058380 glide BrSprFontDraw */

int BrSprFontDraw(int param_1,int param_2,unsigned int param_3,int *param_4,
                 int param_5)

{
  BrUiSprBlit(DAT_10ac5d84,param_1,param_2,g_img[param_3 & 0xffff].surf,param_4,param_5)
  ;
  return;
}

/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* WHAT IT DOES: store the render-destination pointer for the font subsystem. */
/* @implements 0x1006E020 glide BrFontSetRenderDst */

int BrFontSetRenderDst(int param_1)

{
  DAT_118ed1a0 = param_1;
  return;
}

/* FUN_1006c750: prototype in br_funcs.h */
/* FUN_1006c800: prototype in br_funcs.h */
/* FUN_1006c880: prototype in br_funcs.h */
/* FUN_1006c8b0: prototype in br_funcs.h */


/* WHAT IT DOES: create all font and UI textures and register font pages. */
/* @implements 0x1006E030 glide BrFontTexInitAll */

int BrFontTexInitAll(void)

{
  BrFontTexCreate();
  BrFontRegisterPages();
  BrFontTexCreateAlt();
  BrSub10073980();
  BrSub100739B0();
  BrFontTexCreateFlat();
  BrFontTexCreatePair();
  return;
}

/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* WHAT IT DOES: create a pair of tileable font textures for the UI. */
/* @implements 0x1006C8B0 glide BrFontTexCreatePair */

int BrFontTexCreatePair(void)

{
  g_BrEnvTexLookup[0] = (uint32_t)(*(*(funcptr *)&g_pfn18AA0B0))(&g_BrEnvBitmap,0,0x40,0x40,1,4,0,0,1,1,0,0,1,0);
  _DAT_1184c464 = (uint32_t)(*(*(funcptr *)&g_pfn18AA0B0))(&DAT_104b05c8,0,0x40,0x40,1,4,0,0,1,1,0,0,1,0);
  return;
}

/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* WHAT IT DOES: create a flat (no mip, no wrap) font texture via the registered callback. */
/* @implements 0x1006C880 glide BrFontTexCreateFlat */

int BrFontTexCreateFlat(void)

{
  (*(int *)&g_BrDrawReflectTexB) = (*(*(funcptr *)&g_pfn18AA0B0))(&DAT_100b84a8,0,0x40,0x40,0,4,0,0,0,0,0,0,0,0);
  return;
}

