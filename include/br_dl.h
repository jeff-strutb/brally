/* br_dl.h -- the display-list machine.  BRGlide.dll 0x10023C90 and its
 * 256-entry handler table at 0x100A9A58.
 *
 * WHAT THIS IS, AND WHY IT IS NOT A NEW INVENTION
 * ----------------------------------------------------------------------
 * Boss Rally's PC builds do not have a "3D path" in the usual sense.  Both
 * BRD3D.dll and BRGlide.dll carry a *software RSP/RDP*: the game emits N64
 * F3D display lists exactly as the console version did, and a table-driven
 * interpreter walks them and turns them into backend calls.  This is the same
 * pattern br_font.c already documents for text -- the text emitter's output
 * is fed to THIS interpreter, not to a separate one.  There is one table and
 * one loop in the whole binary.
 *
 * The interpreter (0x10023C90 Glide / 0x10024A90 D3D, 29 bytes, byte-identical
 * in both builds) is:
 *
 *     while (p) p = table[((const uint8_t *)p)[3]](p);
 *
 * Note `[3]`: the opcode is byte 3 of the command *in memory*, because the
 * whole list has already been byte-swapped into host order by the loader
 * (BrDlPatch below, 0x10019040).  w0/w1 are therefore plain host u32s, which
 * is exactly what slice1_05.h's BrGfxWords says.
 *
 * MEASURED FACTS THE DESIGN RESTS ON
 * ----------------------------------------------------------------------
 *   - 28 of the 256 opcodes have a handler; the other 228 all point at the
 *     same 8-byte "return p+8" stub (0x10021240 Glide / 0x100243D0 D3D).
 *   - The two builds handle EXACTLY the same 28 opcodes.  18 of the 28
 *     handlers are byte-identical shared code; 8 diverge (0x04, 0x06, 0xB1,
 *     0xBF, 0xDC, 0xE2, 0xED, 0xF8) -- geometry, texture binding, scissor,
 *     fog colour.  So the command set is a property of the GAME, not of the
 *     backend, and it is closed.
 *   - Handlers return the NEXT command pointer.  Most return p+8; 0xE4
 *     returns p+0x18 (three double-words); 0xDC returns p + 8*w1 (it can
 *     stand in for a run of commands); 0xB8 returns the popped return
 *     address or NULL; 0x06 returns w1 (the callee).
 *   - The combiner is NOT open-ended.  0x1001E7A0 is a chain of exact
 *     equality tests on the (w0,w1) pair: TEN recognised configurations plus
 *     a default.  Render mode (0x10021270) is the same shape: nine exact
 *     values plus a bit-tested fallback.  Both are enumerable, which is what
 *     makes a fixed set of Metal pipeline states the right answer.
 *
 * NOT MODELLED HERE, and deliberately: TMEM.  The interpreter SKIPS 0xF5
 * (SETTILE), 0xF3 (LOADBLOCK), 0xF0 (LOADTLUT), 0xFD (SETTIMG) and 0xBB
 * (G_TEXTURE) at draw time -- they fall to the default stub.  Texture binding
 * happens through 0xDC/0xDD, which the loader plants, and through the
 * load-time fixup of 0xFD in BrDlPatch.
 *
 * HOW A TEXTURE REACHES THE BIND OPCODE
 * ----------------------------------------------------------------------
 * A shipped .rca contains NO 0xDC at all -- measured, testdata/bb.rca has 84
 * 0xFD, 24 0xF3, 15 0xF0, 6 0xF5, 6 0xF2 and zero 0xDC -- yet this
 * interpreter has no handler for any of those and textures plainly appear.
 * The resolution is that the 0xDC is WRITTEN INTO THE LIST AT LOAD TIME, over
 * the top of the run it replaces.  A typical run in bb.rca, at file 0x97E8:
 *
 *      FD SETTIMG   E6 LOADSYNC   F3 LOADBLOCK   F5 SETTILE   F2 SETTILESIZE
 *
 * and the chain that rewrites it, all addresses BRGlide:
 *
 *   0x10028820  the scan.  Walks a just-loaded display list to G_ENDDL with a
 *               nine-state machine.  Reached through the renderer vtable slot
 *               0x118ED1DC, which 0x10029B50 fills; its callers are 0x10030770
 *               (the .rca fixup), 0x100314D0 and 0x100316D0 (the .trk load)
 *               and 0x100302A0 -- i.e. LOAD time, once per list.
 *   0x10029420  G_SETTIMG.  Records `siz` and the image ADDRESS (already
 *               segment-fixed by BrDlPatch's 0xFD arm, so it is a real host
 *               address into the loaded .rca), and -- only from state 0 --
 *               records this command as the START of the run.
 *   0x10029480  G_LOADTLUT.  Publishes the palette address in 0x106B7A98 and
 *               copies ((lrs-uls)+1)*((lrt-ult)+1)*2 bytes to [0x100A9E58].
 *   0x10029510  G_LOADBLOCK.  Publishes the texel address in 0x105D17F0 and
 *               stages 2*((lrs-uls)+1) bytes at 0x105D17F8.
 *   0x100295B0  G_SETTILE / 0x100296B0 G_SETTILESIZE fill the eight 0x40-byte
 *               tile records at 0x10697840 (fmt +0, siz +4, ... maskS +0x20,
 *               maskT +0x24, uls +0x30 .. lrt +0x3C).
 *   0x10028B50  THE SEAM.  Called when the run ends (at G_VTX / G_TRI).  It
 *               calls the registrar, and on success does exactly this:
 *
 *                   pRunStart->w0 = 0xDC000000 | (id & 0x00FFFFFF);
 *                   pRunStart->w1 = (pRunEnd - pRunStart) / 8;
 *
 *               So the FIRST command of the run becomes 0xDC and its w1 is the
 *               run length in commands -- which is precisely why the 0xDC
 *               handler (0x1001E2E0) returns `p + 8*w1`: it steps over the
 *               commands it replaced.  Nothing else needs a handler.
 *   0x10028BB0  the registrar.  Builds a 0x2B0-byte descriptor on the stack
 *               (tmu +0, evenOdd +4, w +8, h +0xC, GrTextureFormat +0x10,
 *               lods +0x18/+0x1C, aspect +0x20, clampS/T +0x24/+0x28,
 *               texel source +0x48, palette source +0x4C, a verbatim copy of
 *               all eight tile records at +0x60..+0x25F, mode +0x264, source
 *               w/h +0x2A0/+0x2A4), looks for an identical one, and appends a
 *               new record when there is none.  Width and height come from
 *               `1 << maskS` / `1 << maskT`, doubled when the tile mirrors,
 *               or from `(lr-ul+4)>>2` -- the same formula br_dl_settilesize
 *               uses.
 *   0x10027A70  the dedup (== BRD3D 0x10028630 == slice1_04's BrTblFind).
 *               Keys on the texel source address, the palette source address
 *               and eight render-state bytes; returns an existing index or -1.
 *   0x10027A10  AppendTexture -- the growable array itself: count 0x10697A58,
 *               capacity 0x10697A5C, base 0x106B7AA0, stride 0x2B4 (0x2B8 in
 *               D3D), grown 0x100 records at a time.  THE 24-BIT VALUE IN THE
 *               0xDC COMMAND IS AN INDEX INTO THIS ARRAY.
 *   0x10027710  make the Glide texture: 0x10027B60 converts the texels,
 *               0x10028200 allocates a TMEM slot and fills a 0xD8-byte record
 *               at 0x10661844 (whose +0xC0..+0xD3 IS a GrTexInfo), 0x100283C0
 *               calls grTexDownloadMipMap.
 *   0x100284E0  what 0xDC actually invokes.  0x1001E2E0 calls the pointer at
 *               0x118ED1CC, which 0x10029B50 sets to 0x100284E0; that reads
 *               record[id] and issues grTexSource / grTexClampMode /
 *               grTexFilterMode / grTexLodBiasValue / grTexMipMapMode, and
 *               sets the two texel-scale globals 0x118ED1A4 / 0x118ED1A8 that
 *               br_dl_finish_vtx holds at 1.0.  0xDD (0x1001E300) calls
 *               0x118ED1D0 == 0x100285E0, which re-downloads the same slot at
 *               a new address -- the one-texture scheme br_font.c uses.
 *
 * WHERE THE PIXELS COME FROM, AND IN WHAT FORMAT
 * ----------------------------------------------------------------------
 * Three separate sources, and only the first goes through the path above:
 *
 *  1. THE .rca ITSELF.  G_SETTIMG's address, after the segment fixup, points
 *     into the loaded model image; slice2_20's BrTexCopyRecords (0x10031AC0
 *     Glide) has already placed the texel blobs and TLUTs there.  The staging
 *     copies at 0x105D17F8 / [0x100A9E58] are side copies -- the expander
 *     reads the ORIGINAL bytes through the descriptor's +0x48 / +0x4C.
 *  2. cargfx/skytex{desert,mountain,coast,mine,amazon}[n].{ci4,lut4}, a ten
 *     entry table of literal filenames at 0x100B5330 referenced only from the
 *     track records that the .trk loader 0x100311C0 reads.  Raw CI4 + a
 *     16-entry RGBA5551 palette; port/src/br_n64tex.c already decodes them.
 *  3. images\*.bmp and Paint\*.bmp -- 24-bit Windows BMPs, the 2D sprite
 *     sheets and the car liveries.  These do NOT enter this path; the paint
 *     ones are SUBSTITUTED for an .rca texture inside 0x10023D70, which is why
 *     0x10027710 takes its pixel pointer as an argument.
 *
 * The format is not inferred from byte statistics -- three earlier attempts in
 * this project failed that way.  It is read off two functions:
 *
 *   0x10027220 (== BRD3D 0x10027B90 == slice1_04's BrTexFormatCode) maps the
 *   tile's (siz, fmt) onto a Glide GrTextureFormat_t.  Argument order is
 *   (siz, fmt, mode); the font pins it -- 0x1006C790 registers an IA/8b font
 *   as Glide format 4, and only (siz=1, fmt=3) reaches 4.  Exhaustively, the
 *   only three inputs that do NOT return the catch-all 11 == ARGB_1555 are
 *
 *       I4   (siz 0, fmt 4)  ->  11 if mode == 1 else 2  == ALPHA_8
 *       IA8  (siz 1, fmt 3)  ->  12 if mode == 1 else 4  == ALPHA_INTENSITY_44
 *       I8   (siz 1, fmt 4)  ->  2  always               == ALPHA_8
 *
 *   `mode` is 0x106B7AAC, which br_tex3d.h shows is 0 for retail car models.
 *
 *   THIS ENTRY USED TO SAY "CI4, CI8 and RGBA16 -- everything the models
 *   actually use -- fall to the catch-all ... only IA4/I8/IA8 pick anything
 *   else", AND BOTH HALVES WERE WRONG.  It named IA4, which is a catch-all
 *   case (siz 0, fmt 3 -> 11), where the real third case is I4; and the
 *   parenthesis about the models was a guess that nobody had decoded.
 *
 *   DECODED, over all nineteen shipped assets in testdata/ (16 .rca cars, two
 *   .trk tracks, BossRally.pod).  Every G_SETTILE (0xF5) sitting in the
 *   canonical texture-load idiom -- a G_SETTIMG, a load (0xF3/0xF4/0xF0),
 *   this 0xF5, then a 0xF2 G_SETTILESIZE, syncs only in between -- was
 *   counted.  Coincidental bytes cannot satisfy that five-command shape, and
 *   a plain linear scan for 0xF5 was run alongside as the recall check.
 *
 *       RGBA16  1155   fifteen cars (not hm.rca), both tracks -- 733 of them
 *                      in desert.trk and 386 in race.trk
 *       CI4      277   fifteen cars (not bb.rca), and desert.trk 8
 *       I4        32   desert.trk 8, BossRally.pod 24
 *       IA8       16   desert.trk 4, BossRally.pod 12
 *       CI8        0   NOWHERE
 *
 *   So the models use FOUR formats, not three; CI8 is used by nothing at all;
 *   and the two extra ones are exactly two of the three that do NOT fall to
 *   the catch-all.  desert.trk's twelve are self-consistent five ways over --
 *   each tile's `line` (64-bit words per row) equals width * bytes-per-texel
 *   exactly, for all five distinct textures (IA8 64x32 and 64x64, I4 16x16
 *   and two 32x8), and every G_SETTIMG address resolves inside the file.
 *   BossRally.pod's are NOT counted as evidence: CONVENTIONS.md records the
 *   POD as a leftover holding one entry, and its sites repeat at a fixed
 *   0x1A0 stride with three G_SETTILEs in a row, which is not the idiom.
 *
 *   WHAT THIS COSTS, stated rather than papered over.  BrTexFormatCode itself
 *   is transcribed correctly and test_slice1_04.c pins all three non-default
 *   arms, so the MAPPING is fine.  The gap is downstream: br_tex3d.c's
 *   `br_tex3d_bpp` accepts CI4, CI8 and RGBA16 and returns 0 for everything
 *   else, because it was written to this claim.  desert.trk's five IA8/I4
 *   textures therefore come back BR_TEX3D_UNSUPPORTED and do not expand.
 *   That is an honest frontier, not a fabrication -- but it is a frontier
 *   nobody knew was there.  Closing it means transcribing two more arms of
 *   the 8480-byte expander 0x100250D0; the bpp values are not in doubt (the
 *   data confirms 8 and 4) but the per-texel conversion has not been read,
 *   and inventing one would be exactly the wrong-but-plausible function
 *   CONVENTIONS.md warns about.  It is left undone and named.
 *
 *   0x100271F0, forty-four bytes, is the per-texel conversion, and it is
 *   exactly:
 *
 *       v = bswap16(v);                    /  the N64 halfword is BIG-endian
 *       return (v >> 1) | ((v & 1) << 15); /  rotate right one bit
 *
 *   which turns N64 RGBA5551 (rrrrrgggggbbbbba) into Glide ARGB1555
 *   (arrrrrgggggbbbbb).  The CI4 arm of the 8480-byte expander 0x100250D0
 *   (from 0x10025467) reads a byte, splits the nibbles high-first, indexes the
 *   16-bit palette, and puts each result through 0x100271F0 -- so a CI4
 *   texture leaves the loader as 16 bits per texel, ARGB1555, little-endian,
 *   PALETTE ALREADY APPLIED.  br_n64tex.h's independent claim that the .lut4
 *   files are big-endian RGBA5551 is the same fact from the other end.
 *
 *   For a Metal backend that means one texture format for essentially all 3D
 *   geometry -- BGR5A1 / A1BGR5 depending on channel order, or expanded to
 *   RGBA8 on upload -- and no palette state at draw time.
 *
 * STILL UNKNOWN: the meaning of the mode flag 0x106B7AAC (it selects the
 * richer format for the three intensity cases and is part of the dedup key);
 * the other twenty-odd arms of 0x100250D0, including the N64 TMEM row
 * interleave, which is visible in its 4-forward/8-back cursor arithmetic but
 * has not been transcribed; and 0x1002F790, which an earlier note nominated
 * for this investigation and which is NOT part of it -- in BRGlide it is a
 * mutex-driven worker calling 0x1006Cxxx, and the 350-byte D3D function at
 * the same address is a phase state machine.
 */
#ifndef BR_DL_H
#define BR_DL_H

#include <stdint.h>
#include <stddef.h>

#include "br_mat.h"      /* BrMat4 -- row-major, row-vector, as the original */
#include "br_seg.h"      /* BrSegMap, BrSegFixup                             */

/* ---------------------------------------------------------------------
 * Vertex
 * ---------------------------------------------------------------------
 * The transformed-vertex array is 0x105CE318, stride 0x68 = 104 bytes, 32
 * entries.  The first 0x3C bytes are a Glide `GrVertex` with two TMUs -- the
 * triangle handlers hand `&v[i]` straight to grDrawTriangle, so this prefix
 * is not a guess, it is the ABI.  The rest is the walker's own scratch.
 *
 * Offsets are quoted from 0x10021A20 (the transform) and 0x1001ECF0 (TRI1).
 */
typedef struct BrDlVtx {
    float   x, y, z;        /* 0x00 0x04 0x08  screen x/y, z unused here   */
    float   r, g, b;        /* 0x0C 0x10 0x14  colour (see the note below) */
    float   ooz;            /* 0x18                                        */
    float   a;              /* 0x1C                                        */
    float   oow;            /* 0x20  1/w -- written by the transform       */
    float   tmu0[3];        /* 0x24 0x28 0x2C  sow, tow, oow               */
    float   tmu1[3];        /* 0x30 0x34 0x38  sow, tow, oow               */
    int32_t outcode;        /* 0x3C  six frustum bits plus w               */
    float   f40;            /* 0x40  the clip list's `next` -- see below   */
    float   cx, cy, cz;     /* 0x44 0x48 0x4C  clip space                  */
    float   s, t;           /* 0x50 0x54  texture coords, pre-divide       */
    float   cw;             /* 0x58  clip space w                          */
    float   n0, n1, n2;     /* 0x5C 0x60 0x64  the Vtx's last three bytes  */
} BrDlVtx;

/* 0x40 was recorded as "never read".  It is read, by the clipper: 0x1001EE70
 * builds a circular singly-linked list whose NODES are `&vtx->f40`, so f40 is
 * the next-pointer and the nine floats from f40+4 to f40+0x24 -- cx cy cz s t
 * cw n0 n1 n2, in that order -- are exactly the nine attributes the shared
 * interpolator 0x1001F200 lerps.  The clip node is therefore a 0x28-byte
 * record overlaid on the vertex at +0x40.
 *
 * The port cannot overlay it: `next` is a host pointer and f40 is 32 bits
 * (CONVENTIONS.md, byte offsets are 32-bit-only).  So the three input
 * vertices are COPIED into three seed nodes.  Nothing observable changes --
 * the seven plane routines read only these nine floats and the link.
 *
 * The node type, the interpolator and the plane routines are NOT declared
 * here: they are slice1_03's `BrClipVert` / `BrClipLerpVert` / `BrClipPlane*`
 * (0x1001D810 and family in the D3D map, 0x1001F0D0 and family in Glide's),
 * and the 64-node pool at 0x105CCFF0 must stay ONE object -- see
 * CONVENTIONS.md, "Aliased storage: a link-clean bug".  br_dl.c owns only the
 * driver 0x1001EE70, which has no D3D counterpart and so was invisible from
 * that packet.  BR_DL_CLIP_POOL is the pool SIZE, stated here because
 * br_dl.c provides the storage. */
#define BR_DL_CLIP_POOL   64

/* 0x1001EE70's frame is 0x224 bytes and holds the output GrVertex array at
 * ebp-0x224 with stride 0x3C, so NINE vertices fit exactly (0x21C) before the
 * two locals at ebp-8/ebp-4.  Sutherland-Hodgman across seven planes can
 * produce ten, so the original has a latent one-vertex stack overflow; the
 * port clamps and counts instead.  DEVIATION, recorded at the site. */
#define BR_DL_CLIP_MAX     9

/* The colour slots.
 *
 * THE LIGHTING PASS HAS NOW BEEN FOUND.  It is not a second pass over the
 * vertex array and it is not inside the triangle submitters: it is a SECOND
 * VERTEX TRANSFORM, and the G_VTX handler is swapped for it.  See the
 * "Lighting" section of br_dl.c for the whole chain; the one-line version is
 *
 *     0x1001FD70 rewrites dispatch-table slots 0x04 / 0xB1 / 0xBF
 *     according to the geometry mode, and 0x100A9A68 IS slot 0x04
 *     (0x100A9A58 + 4*4).
 *
 * so `G_LIGHTING` selects 0x10021C70 (or its decal twin 0x100221D0) in place
 * of 0x10021A20, and THAT routine turns the Vtx's trailing three bytes --
 * which are a NORMAL, measured: every one of the 864 vertices in bb.rca and
 * the 931 in ce.rca has |(b12,b13,b14)| == 127.0 +/- 0.6 -- into a colour.
 *
 * So for an unlit model 0x10021A20's verbatim copy stands (and the combiner
 * such a model uses ignores the iterated colour anyway); for a lit one the
 * lighting overwrites r/g/b AND n0/n1/n2, the latter so that the clipper,
 * which interpolates the nine floats from +0x44, carries the LIT colour
 * across a clipped edge rather than the normal. */

#define BR_DL_VTX_COUNT   32      /* 0x105CE318 .. one G_VTX can fill it   */
#define BR_DL_MTX_STACK   11      /* 0x105CCD10, stride 0x40, index wraps  */
#define BR_DL_DL_STACK    10      /* 0x105CE2E8; 0x10021020 exit(1)s at 10 */
#define BR_DL_LIGHTS       8      /* 0x105CCC78, 16 bytes each             */

/* ---------------------------------------------------------------------
 * Geometry mode (0x105D17C8), and why these bits are not a guess
 * ---------------------------------------------------------------------
 * 0x1001FD70 tests them one at a time and each test lands on a Glide call
 * that names the bit: 0x10000 -> fog mode, 0x1000/0x2000 -> cull mode,
 * 0x0001 -> depth-buffer function, 0x0200 -> the flat/smooth triangle pair.
 * They are F3D's `G_*` values exactly.  Corroboration from the emitter side:
 * the game issues 0xB7 with 0x00020205 == ZBUFFER|SHADE|SHADING_SMOOTH|
 * LIGHTING at 0x1000AA18 and 0x10015ADD, and 0xB6 with 0x000C0000 ==
 * TEXTURE_GEN|TEXTURE_GEN_LINEAR at six sites. */
#define BR_DL_GEO_ZBUFFER          0x00000001u
#define BR_DL_GEO_TEXTURE_ENABLE   0x00000002u
#define BR_DL_GEO_SHADE            0x00000004u
#define BR_DL_GEO_SHADING_SMOOTH   0x00000200u
#define BR_DL_GEO_CULL_FRONT       0x00001000u
#define BR_DL_GEO_CULL_BACK        0x00002000u
#define BR_DL_GEO_FOG              0x00010000u
#define BR_DL_GEO_LIGHTING         0x00020000u
#define BR_DL_GEO_TEXTURE_GEN      0x00040000u
#define BR_DL_GEO_TEXTURE_GEN_LIN  0x00080000u

/* ---------------------------------------------------------------------
 * The light record, READ OFF THE CONSUMER
 * ---------------------------------------------------------------------
 * Eight 16-byte records at 0x105CCC78.  The layout below is taken from the
 * three places that touch them, not from libultra:
 *
 *   0x1002386E (G_MOVEMEM): destination is
 *       0x105CCC78 + ((idx - 0x86) >> 1) * 16, for (w0 & 0xFFFF) bytes.
 *       So a slot is 16 bytes and idx 0x86,0x88,... are slots 0,1,...
 *   0x10023A05 (G_MOVEWORD LIGHTCOL): byte address is (off >> 5) * 16, and
 *       the low nibble of `off` picks +0 (nibble 0) or +4 (nibble 4) for the
 *       three colour bytes -- so a record holds TWO colour triples.
 *   0x10021C70 (the lit transform) reads, for slot 0:
 *       0x105CCC78 +0 +1 +2   as UNSIGNED bytes  -> the light colour
 *       0x105CCC80 +0 +1 +2   as SIGNED   bytes  -> the light direction
 *     and for slot 1:
 *       0x105CCC88 +0 +1 +2   as UNSIGNED bytes  -> the ambient colour
 *     i.e. +0 colour, +4 the duplicate colour (never read), +8 direction.
 *
 * The emitter agrees exactly.  0x1000A88B issues
 *     G_MOVEWORD numlight w1=0x80000040   -> (w1 >> 5) & 0xF == 2
 *     G_MOVEMEM 0x86 len 16 from 0x100A9FF8
 *     G_MOVEMEM 0x88 len 16 from 0x100A9FF0
 * and 0x100A9FF0 is one libultra `Lights1` -- an 8-byte Ambient followed by a
 * 16-byte Light -- whose bytes read
 *     ambient  33 33 40 00  33 33 40 00
 *     light    EE EE CC 00  EE EE CC 00  54 54 54 00
 * i.e. a warm white (238,238,204) directional light along (+84,+84,+84) with
 * a dim cool ambient (51,51,64), each colour stored twice.  That is `Light_t`
 * / `Ambient_t`, established from both ends rather than assumed.
 *
 * ONLY TWO SLOTS ARE USED.  0x10021C70 reads slot 0 and slot 1 at FIXED
 * addresses; `numlights` is only ever tested against zero.  A second
 * directional light would be silently ignored by this build. */
#define BR_DL_LIGHT_COL     0     /* +0..+2  unsigned colour              */
#define BR_DL_LIGHT_COLC    4     /* +4..+6  the copy; nothing reads it   */
#define BR_DL_LIGHT_DIR     8     /* +8..+10 signed direction             */
#define BR_DL_LIGHT_DIFFUSE 0     /* slot 0 -- G_MOVEMEM index 0x86       */
#define BR_DL_LIGHT_AMBIENT 1     /* slot 1 -- G_MOVEMEM index 0x88       */

/* 1/128 (0x100773A0), the scale BrVtxExpand applies to the Vtx's trailing
 * bytes -- so a lit model's normal arrives in [-1, 1).  0x10021C70 applies
 * the same factor to the light direction, but as a DIVIDE by 0x10077420 ==
 * 128.0f; br_dl.c writes that one out as a divide.  Exposed because a caller
 * building an expanded vertex record needs the same number. */
#define BR_DL_BYTE_SCALE  (1.0f / 128.0f)

/* 0x10077418 == 255.0f.  The Glide build's iterated colours are 0..255, so
 * the lit colour is clamped there.  The D3D build's counterpart 0x10022350
 * (slice2_16.c's BrGbiLightVertex) is the same 301 bytes with the limit at
 * 0x1008F3C4 == 1.0f and a 1/255 in its setup instead: the two builds differ
 * only in the colour convention.  Glide is the reference (CONVENTIONS.md), so
 * this port carries 0..255 and says so. */
#define BR_DL_COLOUR_MAX  255.0f

/* Clip outcode bits, in the order 0x10021A20 tests them. */
#define BR_DL_CLIP_W      0x01    /*  w     <= 0 */
#define BR_DL_CLIP_NEAR   0x02    /*  z + w <= 0 */
#define BR_DL_CLIP_FAR    0x04    /*  w - z <= 0 */
#define BR_DL_CLIP_LEFT   0x08    /*  x + w <= 0 */
#define BR_DL_CLIP_RIGHT  0x10    /*  w - x <= 0 */
#define BR_DL_CLIP_BOTTOM 0x20    /*  y + w <= 0 */
#define BR_DL_CLIP_TOP    0x40    /*  w - y <= 0 */

/* ---------------------------------------------------------------------
 * The combiner, enumerated
 * ---------------------------------------------------------------------
 * 0x1001E7A0 compares the SETCOMBINE payload against ten exact (w0,w1) pairs
 * and calls grColorCombine with a constant argument tuple for each.  The
 * Glide argument tuples are recorded in br_dl.c beside each pattern; what
 * matters to a portable backend is that the set is closed and tiny.
 *
 * BR_DL_CC_DECAL is special: matching it flips 0x100A9A68 between 0x10021C70
 * and 0x100221D0, so it selects a different code path, not merely a blend.
 *
 * ERRATUM, and it was the thing hiding the lighting: 0x100A9A68 is NOT a
 * "triangle-drawing function pointer", which is what this note used to call
 * it.  It is 0x100A9A58 + 4*4, i.e. DISPATCH-TABLE SLOT 0x04, and both
 * routines are G_VTX transforms -- the lit one and its decal variant.  See
 * PART 4 of br_dl.c.
 *
 * ERRATUM -- TWO OF THESE NAMES ARE THE WRONG WAY ROUND.  The argument
 * tuples this comment says are "recorded in br_dl.c" are not; they have since
 * been read off 0x1001E7A0 and written down in port/src/gfx/br_gfx3d.h, and
 * they say:
 *
 *   BR_DL_CC_SHADE (FCFFFFFF FFFCF87C) is grColorCombine(SCALE_OTHER, ONE,
 *      LOCAL_CONSTANT, OTHER_TEXTURE) -- i.e. `1.0 * TEXTURE`, and its N64
 *      words decode to (a-b)*c+d == TEXEL0.  It is the TEXTURE row.
 *   BR_DL_CC_TEX (FCFFFFFF FFFE793C) is grColorCombine(FUNCTION_LOCAL, ZERO,
 *      LOCAL_ITERATED, ...) -- i.e. the vertex colour, and its N64 words
 *      decode to SHADE.  It is the SHADE row.
 *
 * The names are LEFT AS THEY ARE so that no existing caller silently changes
 * meaning; consult br_gfx3d.h before using either.  Corroboration that the
 * decode and not the naming is right: BR_DL_CC_TEX_SHADE_CW's site calls
 * grConstantColorValue(-1) (white) and the same row's N64 d-input is the
 * literal `1`. */
typedef enum BrDlCombine {
    BR_DL_CC_DEFAULT = 0,   /* anything unrecognised                        */
    BR_DL_CC_SHADE,         /* FCFFFFFF FFFCF87C                            */
    BR_DL_CC_TEX,           /* FCFFFFFF FFFE793C                            */
    BR_DL_CC_TEX_SHADE_C1,  /* FC567EAC FFFFF3F9  + constant colour 0x000000FF */
    BR_DL_CC_TEX_SHADE_A,   /* FCFF97FF FF2DFEFF                            */
    BR_DL_CC_TEX_SHADE_B,   /* FCFFFFFF FFFDF2F9                            */
    BR_DL_CC_TEX_SHADE_CW,  /* FCFFFFFF FFFF73B9  + constant colour -1      */
    BR_DL_CC_ENVMAP,        /* FC127E08 F3FFF2F8  (conditional)             */
    BR_DL_CC_DECAL,         /* FC317E02 5FFEF3FA / 51FEF3FA                 */
    BR_DL_CC_TEX_SHADE_C0,  /* FC127FFF FFFFF838  + constant colour 0       */
    BR_DL_CC__COUNT
} BrDlCombine;

/* ---------------------------------------------------------------------
 * The backend seam
 * ---------------------------------------------------------------------
 * Everything the interpreter wants from a renderer.  Deliberately tiny and
 * free of platform types, in the spirit of br_gfx.h.  A NULL entry is simply
 * not called, so a consumer can take only what it needs. */
typedef struct BrDlSink {
    void *pUser;
    /* One triangle, three GrVertex-shaped vertices, already in screen space
     * with 1/w carried in `oow` and s/w, t/w in tmu0. */
    void (*pfnTri)(void *pUser, const BrDlVtx *a, const BrDlVtx *b,
                   const BrDlVtx *c);
    /* Combiner changed. `id` is the enumerated configuration; the raw words
     * are passed too so a backend can refuse to guess about DEFAULT. */
    void (*pfnCombine)(void *pUser, BrDlCombine id, uint32_t w0, uint32_t w1);
    /* SETOTHERMODE_L shift 3, i.e. the render mode word. */
    void (*pfnRenderMode)(void *pUser, uint32_t mode);
    /* 0xDC: bind the texture named by the low 24 bits of w0. */
    void (*pfnBindTexture)(void *pUser, uint32_t handle);
    /* 0xDD: re-aim that texture at `addr` (Glide's one-texture scheme). */
    void (*pfnRetarget)(void *pUser, uint32_t handle, uint32_t addr);
    /* A screen rectangle. `fTextured` distinguishes 0xE3/0xE4 from 0xE1/0xF6.
     * Corners are integer pixels in the COMMAND's top-down convention -- 10.2
     * payloads have already been divided, and 0xE1's are sign-extended.  The
     * bottom-up window the untextured handlers actually hand to 0x1001E380,
     * with its Y flip and its +1/-1, is in BrDl.rectMinX..rectMaxY; it is not
     * passed here because 0xE3/0xE4 reach a different emitter that flips
     * differently. */
    void (*pfnRect)(void *pUser, int fTextured, int tile,
                    int32_t ulx, int32_t uly, int32_t lrx, int32_t lry);
} BrDlSink;

/* ---------------------------------------------------------------------
 * Addressing, and why it needs a table
 * ---------------------------------------------------------------------
 * The original's patch pass rewrites the address words of G_VTX, G_SETTIMG
 * and friends into HOST POINTERS, and the handlers dereference them
 * directly.  That cannot be done here: a display-list word is 32 bits and a
 * host pointer is 64, and CONVENTIONS.md is explicit that byte offsets in
 * this corpus are 32-bit-only.
 *
 * So the port keeps the 32-bit address in the command -- which is what the
 * data on disc holds anyway -- and resolves it at USE time through a small
 * table of registered regions.  The arithmetic is BrSegFixup's, moved from
 * load time to draw time; the observable result is identical and no pointer
 * is ever truncated. */
#define BR_DL_REGIONS 4

typedef struct BrDlRegion {
    uint32_t       base;      /* first 32-bit address the region answers to */
    const uint8_t *pHost;
    size_t         cb;
} BrDlRegion;

/* ---------------------------------------------------------------------
 * State
 * --------------------------------------------------------------------- */
typedef struct BrDl {
    BrDlSink  sink;

    BrDlRegion aRegion[BR_DL_REGIONS];
    int        cRegions;

    BrMat4    proj;                          /* 0x105CCD00                 */
    BrMat4    aModel[BR_DL_MTX_STACK];       /* 0x105CCD10, stride 0x40    */
    int32_t   iModel;                        /* 0x100A9A50, 0 == none      */
    BrMat4    combined;                      /* 0x105D1760                 */
    /* 0x105D17D0 is NOT a "combined matrix stale" flag, which is what this
     * field used to be called.  It is the LIGHT CACHE flag, and it reads the
     * other way round: 0x10021C70 recomputes the derived light state when it
     * is zero and then sets it to 1.  Its three writers are exactly the three
     * things the derived state depends on -- 0x1002116E (any MODELVIEW
     * G_MTX; the two projection arms jump past it), 0x10023898 (G_MOVEMEM
     * light) and 0x10023A33/0x10023A6C (G_MOVEWORD LIGHTCOL) -- all of which
     * write 0. */
    int32_t   fLightCached;                  /* 0x105D17D0                 */

    /* viewport: screen = trans + scale * (clip/w) */
    float     vpScaleX, vpTransX;            /* 0x105CCD48, 0x105CD9F8     */
    float     vpScaleY, vpTransY;            /* 0x105CCFDC, 0x105CD9FC     */

    uint32_t  geoMode, geoModePrev;          /* 0x105D17C8, 0x105D17CC     */
    uint32_t  renderMode;                    /* 0x105CE2D4                 */
    uint32_t  combineW0, combineW1;          /* 0x105D17AC, 0x105D17B0     */
    BrDlCombine combine;
    int32_t   fDecal;                        /* 0x105CDA04                 */

    /* 0xFB, 0x1001E930.  Each byte is `fild`ed and then multiplied by
     * 0x10077400 == 1/255, so an ENV colour is 0..1. */
    float     env[4];                        /* 0x105CCD44/5CD9F4/5CCCF8/5CCC74 */
    /* 0xFA, 0x1001EA80.  Each byte is `fild`ed and stored WITH NO SCALE, so a
     * PRIM colour is 0..255 -- the Glide iterated-colour range, the same one
     * BR_DL_COLOUR_MAX names.  The two handlers sit 350 bytes apart and this
     * is the only thing that differs between them; a port that divides both
     * by 255 makes prim disagree with the fallback below and with the 255.0f
     * clamp in br_dl_light_vertex.
     *
     * THESE ARE THE "LIGHTS OFF" FALLBACK COLOUR.  0x10022AC0's numlights==0
     * arm (0x10022BCC) copies 0x105D17A4 / 0x105D17B4 / 0x105CE2D0 into the
     * vertex, and those are exactly this handler's R/G/B destinations -- so
     * `lightOff` and `prim[0..2]` were two host names for one set of original
     * globals, which is the aliased-storage hazard CONVENTIONS.md describes.
     * The `lightOff[3]` field that used to sit further down had NO writer at
     * all; br_dl_light_vertex now reads these three, which is the one
     * object. */
    float     prim[4];                       /* 0x105D17A4, 0x105D17B4,
                                              * 0x105CE2D0, 0x105CD9F0     */
    /* 0xF7, 0x1001E9F0.  The handler does NOT store the word: it expands the
     * LOW RGBA5551 half of w1 into four separate byte globals, which the
     * rectangle emitter 0x1001E380 reads at 0x1001E441.  R/G/B are the 5-bit
     * channels widened as `(v << 3) | (v >> 2)`; A is bit 0 spread to 0 or
     * 255 by `and 1 / neg / sbb / and 0xFF`. */
    uint8_t   fillR, fillG, fillB, fillA;    /* 0x105CCD40, 0x105CCFD8,
                                              * 0x105D17A0, 0x105CE208     */
    /* PORT BOOKKEEPING, not an original global: the raw w1 the expansion came
     * from, kept so a consumer can see the untouched payload. */
    uint32_t  fillColour;
    uint32_t  fogColour;                     /* raw w1 of 0xF8             */
    float     f0A9A54;                       /* 0xDE payload (0x100A9A54)  */
    float     f5D17C4;                       /* 0xDF payload (0x105D17C4)  */

    int32_t   uls, ult, lrs, lrt;            /* 0xF2, 10.2 sign-folded     */
    int32_t   tileW, tileH;                  /* derived, (lr-ul+4)>>2      */

    /* The grSstWinOpen dimensions.  0x100A7518 is the HEIGHT and is what both
     * scissor handlers and both untextured fill-rect handlers subtract Y
     * from; 0x100A7514 is the width and is read by the texture-rect helper
     * 0x100215C0.  In the shipped build they are 640 and 480. */
    int32_t   cxScreen;                      /* 0x100A7514                 */
    int32_t   cyScreen;                      /* 0x100A7518                 */

    /* 0xE2 (0x1001EBC0) and 0xED (0x1001EB50) -- ONE clip window in Glide's
     * BOTTOM-UP screen space, which is why these are min/max and not ul/lr.
     * Both handlers end in grClipWindow(minx, miny, maxx, maxy) through the
     * thunk 0x100729D2, and the Y flip is not optional bookkeeping: an F3D
     * scissor has uly ABOVE lry in a top-down screen, so
     *     minY = H - lry     maxY = H - uly
     * and the two corners SWAP ROLES.  Store them unflipped and the window
     * comes out inverted.  Which global is which is fixed by the consumer
     * 0x1001E380, whose four opening clamps run max, max, min, min against
     * 0x105D17BC, 0x105D17C0, 0x105D17B8, 0x105CCFE0 in that order. */
    int32_t   scisMinX;                      /* 0x105D17BC == ulx          */
    int32_t   scisMinY;                      /* 0x105D17C0 == H - lry      */
    int32_t   scisMaxX;                      /* 0x105D17B8 == lrx          */
    int32_t   scisMaxY;                      /* 0x105CCFE0 == H - uly      */

    /* The four arguments the two UNTEXTURED rect handlers hand to the
     * emitter 0x1001E380 -- 0xE1 at 0x1001E752..0x1001E75A and 0xF6 at
     * 0x1001E367..0x1001E370, the same four pushes in the same order:
     *     (ulx, H - lry - 1, lrx + 1, H - uly)
     * The `inc edi` on X and the `dec edx` on the flipped Y make both spans
     * (corner difference + 1) pixels wide.  Not an original global -- the
     * original passes them in registers to a function this port has not
     * transcribed -- but they are the handler's whole output, so they are
     * recorded here rather than dropped.  Untextured only: 0xE3/0xE4 reach
     * 0x100215C0 instead, which does its own flip in floating point. */
    int32_t   rectMinX, rectMinY, rectMaxX, rectMaxY;

    uint8_t   aLight[BR_DL_LIGHTS][16];      /* 0x105CCC78                 */
    int32_t   nLights;                       /* 0x105CCFD0                 */

    /* The derived light state 0x10021C70 builds once per change and every
     * lit vertex then reads.  Nine contiguous floats in the original, and the
     * D3D build has the same nine in the same roles at 0x104C15D0/DC/E8 --
     * slice2_16.h's BrGbiLightState, whose `scale`/`dir`/`ambient` are these
     * three triples under D3D addresses. */
    float     lightScale[3];                 /* 0x105CE210 .. light colour */
    float     lightDir[3];                   /* 0x105CE21C .. model space  */
    float     lightAmb[3];                   /* 0x105CE228 .. ambient      */
    /* The "no lights at all" fallback does NOT use lightAmb: it copies three
     * globals that are not even contiguous.  The D3D build has the same
     * oddity (slice2_16.h calls the triple `off`), which is why it is
     * reproduced rather than tidied.
     *
     * There is no field for it.  0x105D17A4 / 0x105D17B4 / 0x105CE2D0 are
     * `prim[0..2]` above -- 0xFA's own destinations -- and a second float
     * triple here would be one original object under two host names.  See the
     * note on `prim`. */

    const uint8_t *aStack[BR_DL_DL_STACK];   /* 0x105CE2E8                 */
    int32_t   sp;                            /* 0x105CCFE8                 */

    BrDlVtx   aVtx[BR_DL_VTX_COUNT];         /* 0x105CE318                 */

    uint32_t  hTexture;                      /* last 0xDC handle           */

    /* counters -- not in the original; the port's only way to assert. */
    uint32_t  cCommands, cUnhandled, cTriIn, cTriDrawn, cTriRejected;
    uint32_t  cTriClipped, cVtxLoads, cVtxTransformed, cRects;
    uint32_t  cDlCalls, cStackOverflow;
    /* clipper counters.  cTriClipped stays what it was -- triangles that
     * ENTERED the clipper -- so the existing accounting identity
     * cTriIn == cTriDrawn + cTriRejected + cTriClipped still holds; the
     * sub-triangles the clipper emits are counted separately. */
    uint32_t  cTriClipOut;      /* triangles the clipper handed to the sink */
    uint32_t  cTriClipKilled;   /* entered the clipper, came out with < 3   */
    uint32_t  cClipVtxMax;      /* widest polygon produced                  */
    uint32_t  cClipStarved;     /* pool empty: port drops, original faults  */
    uint32_t  cClipOverflow;    /* polygon wider than BR_DL_CLIP_MAX        */
    /* lighting counters */
    uint32_t  cLightSetup;      /* times the derived state was rebuilt      */
    uint32_t  cVtxLit;          /* vertices through a lighting transform    */
    uint32_t  cVtxLitAmbient;   /* ...of which n.L < 0, so ambient only     */
    uint32_t  cVtxLitOff;       /* ...of which numlights == 0               */
    uint32_t  cVtxTexGen;       /* would have used a TEXTURE_GEN routine    */
    uint32_t  cLightNoMtx;      /* setup with no modelview -- see br_dl.c   */
    /* Non-zero while the vertex array holds colours a LIGHTING transform
     * produced, i.e. 0..255 rather than the raw 1/128-scaled bytes.  Not in
     * the original -- there the whole rasteriser is swapped instead, which a
     * BrDlSink has no way to express.  A consumer that wants one number
     * should ask BrDlColourScale(). */
    int32_t   fVtxLit;
} BrDl;

/* What one unit of r/g/b means for the vertices currently in pDl->aVtx:
 * 1/255 after a lighting transform, 1/1 otherwise (where the slot carries the
 * signed 1/128-scaled source byte and is not a colour at all).  Exists so a
 * backend does not have to guess, and because the two ranges are the one
 * observable difference the Glide/D3D split leaves in this data. */
float BrDlColourScale(const BrDl *pDl);

/* 0x1001FD70's choice of G_VTX handler, returned as the ORIGINAL's address.
 * The interpreter does not branch per vertex: it rewrites dispatch-table slot
 * 0x04 (== 0x100A9A68) whenever the geometry mode changes, and 0x1001E8FB
 * swaps the two lit entries when the DECAL combiner row is selected.  Both
 * inputs are per-BrDl state, so evaluating the same function at G_VTX time is
 * exactly equivalent and is what the port does.
 *
 * One of 0x10021A20, 0x10021C70, 0x100221D0, 0x10022600, 0x10022BF0,
 * 0x10023360, 0x10023110.
 *
 * CAVEAT, stated because it is the one place the equivalence is not exact:
 * before the FIRST 0xB6/0xB7 the original's table still holds its link-time
 * contents, which are 0x10021A20 / 0x1001ECF0 / 0x1001FA30 -- the choice for
 * ZBUFFER|SHADING_SMOOTH -- while the geometry mode word is still zero and
 * this function therefore answers 0x10023110.  Both are unlit and both take
 * the same code path here, so nothing observable differs; the addresses do. */
uint32_t BrDlVtxRoutine(const BrDl *pDl);

/* Non-zero when BrDlVtxRoutine's answer is one of the five that light. */
int BrDlIsLit(const BrDl *pDl);

/* Screen size the fill/scissor handlers flip Y against (0x100A7518 is the
 * height, 0x100A7514 the width, both set from grSstWinOpen). */
void BrDlInit(BrDl *pDl, int32_t cxScreen, int32_t cyScreen);

/* Viewport as the transform uses it, in pixels. */
void BrDlSetViewport(BrDl *pDl, float scaleX, float transX,
                     float scaleY, float transY);

/* Register a block of host memory under a 32-bit address range. Returns 0 on
 * success, non-zero when the table is full. */
int BrDlAddRegion(BrDl *pDl, uint32_t base, const void *pHost, size_t cb);

/* Resolve a 32-bit display-list address, or NULL. `cbNeed` bytes must fit. */
const uint8_t *BrDlResolve(const BrDl *pDl, uint32_t addr, size_t cbNeed);

/* Which of the ten recognised combiner configurations is this pair?
 * 0x1001E7A0's compare chain, and the single most portable fact in the
 * renderer: the set is closed. */
BrDlCombine BrDlClassifyCombine(uint32_t w0, uint32_t w1);

/* 0x10023C90 -- run a list of host-order commands to G_ENDDL.
 * Returns the number of commands executed.  `cbMax` bounds the walk; the
 * original has no such bound and will run off the end of a malformed list.
 * DEVIATION, and the reason is that a test must not be able to crash. */
size_t BrDlRun(BrDl *pDl, const uint8_t *pList, size_t cbMax);

/* 0x10019040 -- the load-time pass.  Byte-swaps each command into host order
 * and rewrites the three address-bearing opcodes:
 *
 *   0x04 G_VTX     segment-fix w1              (0x10019210 -> 0x100189E0)
 *   0xBF G_TRI1    halve the three indices     (0x10019250)
 *   0xB1 G_TRI2    halve all six indices       (0x10019270)
 *   0xFD G_SETTIMG segment-fix w1              (0x100189E0)
 *   0xB8 G_ENDDL   stop
 *
 * Everything else is skipped eight bytes at a time.  Note what is NOT here:
 * the walk does not follow 0x06 G_DL, so a list is patched exactly once, by
 * whoever owns it.
 *
 * The Glide build additionally routes each G_VTX through the vertex cache
 * (0x10018E10 == slice1_05.c's BrVtxCacheResolve), replacing w1 with a
 * pointer to the expanded 8-float records.  Pass a non-NULL `pfnResolve` to
 * get that; pass NULL for the D3D shape, which leaves w1 as a raw pointer to
 * the 16-byte N64 Vtx array.
 *
 * Returns the number of commands touched. */
size_t BrDlPatch(const BrSegMap *pMap, uint8_t *pList, size_t cbMax,
                 void (*pfnResolve)(void *pUser, uint32_t *pw1, int nVerts),
                 void *pUser);

/* Which of the 28 opcodes is this?  Non-zero if the original's table has a
 * handler for it, zero if it falls to the skip stub.  Exposed because it is
 * the one fact a caller most often wants to assert. */
int BrDlIsHandled(unsigned op);

/* ---------------------------------------------------------------------
 * Reference rasteriser -- NOT in the original.
 * ---------------------------------------------------------------------
 * The consumer side, exactly as br_font.c part 3 is the consumer side of the
 * text emitter.  Flat/Gouraud, no texture, no z-buffer: enough to prove the
 * geometry is real, not enough to be a renderer. */
typedef struct BrDlRaster {
    uint8_t *pRgba;
    int32_t  cx, cy;
    uint32_t cCovered;      /* pixels written */
    /* Set by BrDlAttachRaster; read only to find out what the colour slots
     * currently mean (BrDlColourScale).  Callers do not fill this in. */
    const BrDl *pDl;
} BrDlRaster;

/* Install BrDlRaster as pDl's sink. */
void BrDlAttachRaster(BrDl *pDl, BrDlRaster *pRas);

#endif /* BR_DL_H */
