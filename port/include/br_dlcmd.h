/* br_dlcmd.h -- nine display-list opcode handlers, at the ORIGINAL's level.
 *
 * RESPONSIBILITY: drawing/ -- turn geometry and images into pixels.  This
 * module is the *handler layer* of the display-list machine that br_dl.h
 * describes: one C function per entry of the 256-slot dispatch table at
 * BRGlide 0x100A9A58, transcribed with the original's calling convention
 * intact.
 *
 * WHY IT IS A SEPARATE MODULE FROM br_dl.c
 * ----------------------------------------------------------------------
 * br_dl.c models the interpreter as a portable machine: a BrDl state object
 * and a BrDlSink of backend callbacks.  That is the right shape for a Metal
 * backend, and it is deliberately NOT a transcription -- several handlers are
 * simplified there, and four of those simplifications are measurable
 * divergences from the Glide code (listed at the bottom of this header).
 *
 * This module is the other half: the nine handlers below are written from
 * the disassembly instruction by instruction, they keep the original's
 * globals as named fields, and every backend call the original makes is a
 * callback with the original's argument list.  The point is that the
 * NEXT-COMMAND POINTER and the exact argument arithmetic become directly
 * assertable, which is what the dispatch loop's correctness rests on.
 *
 * A SIBLING MODULE EXISTS.  port/include/br_dlglide.h does the same job for a
 * DISJOINT set of opcodes -- 0xDC, 0xDD, 0xDF, 0xE1, 0xE2, 0xED, 0xF2 -- with
 * its own state object (BrDlGl) and its own hook struct.  The two do not share
 * storage and neither claims an opcode the other does; between them they cover
 * sixteen of the table's twenty-eight.  0xE1 in particular lives THERE, which
 * is why 0xF6's fixed-point convention is argued from 0xF6 below rather than
 * inherited from its integer twin.
 *
 * THE CALLING CONVENTION, read off all nine
 * ----------------------------------------------------------------------
 * Each handler takes ONE argument -- a pointer to its own 8-byte command,
 * w0 at +0 and w1 at +4 -- and RETURNS THE ADDRESS OF THE NEXT COMMAND.  The
 * loop at 0x10023C90 is `while (p) p = table[p[3]](p);`, so a handler that
 * returns the wrong pointer desynchronises everything after it.
 *
 *   ALL NINE of these return `p + 8`, on EVERY path.  That includes the
 *   early-out paths, which are the ones worth checking:
 *     0x04  n == 0 skips the loop entirely and still returns p + 8
 *           (0x10021A49 `jle 0x10021C5F`, which lands ON the `lea eax,[esi+8]`)
 *     0xBF  trivial-reject and clipped both return p + 8, and they read the
 *           argument at TWO DIFFERENT [esp] displacements (0x24 after three
 *           pushes for the clipper call, 0x18 without them) -- same argument,
 *           different ESP.  Stack displacement is meaningless without the ESP
 *           it is relative to.
 *     0xB1  four exits, all `lea eax,[ebx+8]`, and rejecting or clipping the
 *           FIRST triangle does not skip the second.
 *
 * The variable-length handlers are elsewhere: 0xDC returns p + 8*w1, 0xE4
 * returns p + 0x18, 0x06 returns w1, 0xB8 returns the popped address.  None
 * of those four is in this module; do not generalise `p + 8` to them.
 *
 * THE OPCODES
 * ----------------------------------------------------------------------
 *   0x04  0x10021A20  584 B  G_VTX, the UNLIT vertex transform.  No D3D
 *                            counterpart in shared.csv (D3D's is 0x10021BD0).
 *                            This is the routine the dispatch table holds at
 *                            LINK time; 0x1001FD70 overwrites slot 0x04 with
 *                            one of six lit variants once a geometry mode
 *                            arrives.  See br_dl.h, BrDlVtxRoutine.
 *   0xB1  0x1001FA30  696 B  G_TRI2 -- two triangles, one command.
 *   0xBF  0x1001ECF0  378 B  G_TRI1 -- one triangle.
 *   0xF6  0x1001E320   96 B  G_FILLRECT, 10.2 FIXED-POINT corners.
 *                            Pairs with D3D 0x1001BE30 (SHARED body).
 *   0xF7  0x1001E9F0  110 B  G_SETFILLCOLOR -- expands the LOW RGBA5551 half
 *                            of w1 into four 8-bit channels.
 *                            *** Pairs with D3D 0x1001CC00.  GLIDE 0x1001CC00
 *                            IS RallyMain, which is ported.  A bare address is
 *                            not self-describing; the pairing is D3D-side
 *                            only.  See the warning below. ***
 *   0xF8  0x1001EA60   19 B  G_SETFOGCOLOR -- grFogColorValue(w1), nothing else.
 *   0xFA  0x1001EA80  138 B  G_SETPRIMCOLOR.
 *   0xFB  0x1001E930  183 B  G_SETENVCOLOR.  Pairs with D3D 0x1001CB40, but
 *                            crossdiff matched it by body-dup, so the partner
 *                            is one of a byte-identical pair and the evidence
 *                            does not say which.  Quoted for orientation only.
 *   0xFC  0x1001E770   36 B  G_SETCOMBINE.  Pairs with D3D 0x1001C7F0.
 *
 * THE 0xF7 / RallyMain TRAP
 * ----------------------------------------------------------------------
 * `tools/whereis.py 0x1001E9F0` answers "shared 0x1001CC00 (d3d) ... [read as
 * a GLIDE address]", and port/include/br_boot.h documents 0x1001CC00 as
 * RallyMain -- BRGlide's only export.  Both are true and they are different
 * functions: 0x1001CC00 names RallyMain in the GLIDE image and the fill-colour
 * handler in the D3D image.  tools/isported.py says so explicitly now (it was
 * corrected for exactly this case); before that it reported this handler as
 * "PORTED as BrAppArgs".  Never resolve a counterpart address without saying
 * which build you read it in.
 *
 * 0xF6 IS FIXED POINT -- ESTABLISHED, NOT INHERITED
 * ----------------------------------------------------------------------
 * CONVENTIONS.md records that 0xE1 is a second FILL RECTANGLE over the same
 * bit layout with plain INTEGER corners, so 0xF6's convention had to be read
 * off 0xF6 itself.  It is:
 *
 *      0x1001E320 (0xF6)   shl 0x14 / sar 0x16   and   shl 8 / sar 0x16
 *      0x1001E720 (0xE1)   shl 0x14 / sar 0x14   and   shl 8 / sar 0x14
 *
 * The two functions are otherwise the same 70-odd bytes.  `shl 20` puts a
 * 12-bit field's top bit in bit 31; `sar 22` then brings back its top TEN
 * bits, i.e. the field DIVIDED BY FOUR.  `sar 20` brings back all twelve, no
 * divide.  So 0xF6 reads 10.2 fixed point and 0xE1 reads 12-bit integers,
 * from identical fields.  0xF6 additionally masks each result to 10 bits
 * (`and 0x3FF`), which is what makes the sign extension unobservable and lets
 * the port write the extraction as a plain shift-and-mask.
 *
 * WHERE br_dl.c DIVERGES FROM THESE FUNCTIONS
 * ----------------------------------------------------------------------
 * Recorded here rather than fixed, because br_dl.c is shared ground.  Each
 * was found by transcribing the handler, and each is asserted in
 * port/tests/test_br_dlcmd.c against THIS module:
 *
 *  1. 0xF6 corner assignment.  br_dl_rect()'s untextured arm reads ul from w0
 *     and lr from w1.  0x1001E320 reads them the OTHER way round -- w0 is
 *     (lrx, lry) and w1 is (ulx, uly), which is stock G_FILLRECT and is also
 *     what br_dl_rect()'s own TEXTURED arm does.  Visible in the original as
 *     `sub ebx, edi` / `sub edx, ecx` -- the height flip is applied to the w1
 *     Y to make the MAX edge and to the w0 Y to make the MIN edge, which only
 *     makes sense with w1 = upper-left.
 *  2. 0xFA prim colour scale.  0x1001EA80 stores fild results with NO scale,
 *     i.e. 0..255 floats.  0x1001E930 (env) multiplies each by 0x10077400 ==
 *     1/255.  br_dl.c runs both through br_dl_unpack(), which divides.  The
 *     corroboration is br_dl.h's own `lightOff[3]` note: the "no lights"
 *     fallback reads 0x105D17A4 / 0x105D17B4 / 0x105CE2D0, which ARE the prim
 *     colour's r/g/b, and BR_DL_COLOUR_MAX is 255.
 *  3. 0xF7 is not a raw store.  br_dl.c keeps w1 verbatim; 0x1001E9F0 expands
 *     RGBA5551 into four bytes at 0x105CCD40 / 0x105CCFD8 / 0x105D17A0 /
 *     0x105CE208, and those four bytes are exactly what the rect drawer
 *     0x1001E380 reads at 0x1001E441 when the combiner is not the prim row.
 *     So 0xF7 feeds 0xF6, and the raw word is never used again.
 *  4. 0x04's quarter-pixel snap.  The original is `fmul 4.0 / fistp / fild /
 *     fmul 0.25`; fistp rounds to nearest with TIES TO EVEN.  br_dl.c adds
 *     +/-0.5 and truncates, which is ties-away-from-zero.  They differ on
 *     every exact half, e.g. 0.625 -> 2.5 quarters -> 0.50 here, 0.75 there.
 *
 * None of the four is a crash; all four are silent.  They are listed because
 * "the two models disagree" is the finding, not "the new one is right".
 *
 * FLOATING POINT
 * ----------------------------------------------------------------------
 * The original is x87 with MSVC's startup control word, i.e. 53-bit
 * (double) mantissa, and it rounds to `float` only where it stores.  This
 * module therefore accumulates in `double` and stores to `float` at exactly
 * the points the original has an `fstp dword`, including the intermediate
 * spills through `[ebp-4]` in 0x10021A20 which are real roundings, not
 * scratch.  Writing the chains in `float` throughout would round more often
 * than the original does.
 */
#ifndef BR_DLCMD_H
#define BR_DLCMD_H

#include <stddef.h>
#include <stdint.h>

#include "br_dl.h"       /* BrDlVtx -- 0x105CE318's 0x68-byte record, and it
                          * IS the ABI: 0xBF/0xB1 hand &v[i] straight to
                          * grDrawTriangle.  Never redefine it here. */

/* ---------------------------------------------------------------------
 * The backend seam, one callback per call the nine handlers actually make.
 *
 * Deliberately NOT BrDlSink: this seam is shaped like the original's call
 * sites, arguments and all, so a test can assert the arithmetic that reaches
 * them.  BrDlSink is shaped like a renderer.  A NULL entry is not called.
 * --------------------------------------------------------------------- */
typedef struct BrDlCmdSink {
    void *pUser;

    /* 0x1001EE70 -- the Sutherland-Hodgman driver, reached when the three
     * outcodes are neither all-sharing nor all-zero.  Argument order is the
     * original's: 0x1001ED53 pushes c, b, a so cdecl delivers (a, b, c). */
    void (*pfnClipTri)(void *pUser, BrDlVtx *a, BrDlVtx *b, BrDlVtx *c);

    /* glide2x grDrawTriangle (thunk 0x100729EA), same argument order. */
    void (*pfnDrawTri)(void *pUser, BrDlVtx *a, BrDlVtx *b, BrDlVtx *c);

    /* 0x1001E380, the 914-byte rect drawer 0xF6 / 0xE1 / 0xE3 / 0xE4 share.
     * Its four arguments are a Glide CLIP WINDOW: (minx, miny, maxx, maxy),
     * confirmed by 0x1001E380 clamping arg1/arg2 upward against 0x105D17BC /
     * 0x105D17C0 and arg3/arg4 downward against 0x105D17B8 / 0x105CCFE0 and
     * then passing all four to grClipWindow.  0xF6 computes them from the
     * command; what 0x1001E380 does with them is not this module's business. */
    void (*pfnFillRect)(void *pUser, int32_t minx, int32_t miny,
                        int32_t maxx, int32_t maxy);

    /* 0x1001E7A0 -- the combiner's ten-way equality chain.  0xFC calls it
     * with (w0, w1) AFTER latching both into 0x105D17AC / 0x105D17B0. */
    void (*pfnCombine)(void *pUser, uint32_t w0, uint32_t w1);

    /* glide2x grConstantColorValue (thunk 0x10072996), 0xFA's tail. */
    void (*pfnConstantColor)(void *pUser, uint32_t colour);

    /* glide2x grFogColorValue (thunk 0x100729F6).  0xF8 is nothing else. */
    void (*pfnFogColor)(void *pUser, uint32_t colour);

    /* G_VTX's w1.  In the Glide build BrDlPatch has already replaced w1 with
     * a HOST POINTER to the expanded 8-float records, and 0x10021A20 walks it
     * with a 0x20 stride.  A 64-bit host cannot keep a pointer in a 32-bit
     * command word (CONVENTIONS.md), so the port resolves the address here,
     * exactly as br_dl.h's region table does.  Must return at least
     * `cbNeed` readable bytes, or NULL. */
    const uint8_t *(*pfnResolve)(void *pUser, uint32_t addr, size_t cbNeed);
} BrDlCmdSink;

/* ---------------------------------------------------------------------
 * State -- the Glide globals these nine handlers touch, and only those.
 * Every field carries the address it stands for.
 * --------------------------------------------------------------------- */
typedef struct BrDlCmd {
    BrDlCmdSink sink;

    /* --- 0x04 reads these ------------------------------------------- */
    BrMat4   combined;                    /* 0x105D1760, 16 floats        */
    float    vpScaleX, vpTransX;          /* 0x105CCD48, 0x105CD9F8       */
    float    vpScaleY, vpTransY;          /* 0x105CCFDC, 0x105CD9FC       */
    /* The quarter-pixel snap's fistp/fild destination is a GLOBAL, not a
     * stack slot, so it is observable after the call and is modelled. */
    int32_t  snapScratch;                 /* 0x105CE310                   */

    /* --- 0x04 / 0xBF / 0xB1 share this ------------------------------ */
    /* THE 32 IS FINE FOR THIS BUILD, and that is a measurement rather than an
     * inheritance -- worth writing down because br_f3d.h currently leaves it
     * open in the other direction.
     *
     * br_f3d.h documents stock F3DEX's G_VTX as carrying `v0 + n` in bits
     * 7:1, and records that field "observed as high as 63", concluding the
     * vertex cache may be 64 entries.  THIS BUILD NEVER READS THAT FIELD.
     * 0x10021A20 spills w0 and takes `cl` from [ebp-2] -- byte 2, bits 23:16
     * -- and nothing else in the handler touches bits 7:1.  Nor does the
     * loader supply it: 0x10019210, the patch pass's G_VTX arm, rewrites only
     * w1 (segment fixup, then the vertex cache) and leaves w0 untouched, so
     * whatever is on disc is what the handler sees.
     *
     * On disc, byte 2 is ZERO.  Scanning testdata/bb.rca and ce.rca for
     * 8-aligned commands with opcode 0x04 and 1 <= n <= 32 gives 191 hits; the
     * 8 with a non-zero byte 2 all have n == 1 and junk w1, i.e. they are
     * payload bytes the raw scan mistook for commands.  Every plausible G_VTX
     * has byte 2 == 0 and n <= 32, so the destination range is 0..n-1 and 32
     * slots hold it exactly.  The bits-7:1 field IS populated (values up to
     * 127) -- the emitter still writes stock F3DEX; the interpreter just
     * ignores it.
     *
     * For completeness on the storage itself: the next absolutely-addressed
     * global above 0x105CE318 in BRGlide's .text is 0x105D1718, a gap of
     * 13,312 bytes == 128 x 0x68 exactly.  So the array is not tight at 32;
     * it simply is not filled past it.  BrDlCmdVtx still clamps and counts
     * (cVtxClamped), because none of these handlers bounds the index and a
     * malformed list must not be able to write past the array. */
    BrDlVtx  aVtx[BR_DL_VTX_COUNT];       /* 0x105CE318, stride 0x68      */

    /* --- 0xBF / 0xB1 read these ------------------------------------- */
    /* The tile's texel-to-Glide-unit factors, written by the texture binder
     * (0x100284E0), held at 1.0 by br_dl.c for the same reason. */
    float    texScaleS, texScaleT;        /* 0x118ED1A4, 0x118ED1A8       */

    /* --- 0xF6 reads this -------------------------------------------- */
    int32_t  cyScreen;                    /* 0x100A7518, from grSstWinOpen*/

    /* --- 0xF7 writes these, 0x1001E380 reads them ------------------- */
    uint8_t  fillR;                       /* 0x105CCD40                   */
    uint8_t  fillG;                       /* 0x105CCFD8                   */
    uint8_t  fillB;                       /* 0x105D17A0                   */
    uint8_t  fillA;                       /* 0x105CE208                   */

    /* --- 0xFA writes these.  0..255, NOT 0..1 -- see the header note. */
    float    primR;                       /* 0x105D17A4                   */
    float    primG;                       /* 0x105D17B4                   */
    float    primB;                       /* 0x105CE2D0                   */
    float    primA;                       /* 0x105CD9F0                   */

    /* --- 0xFB writes these.  0..1, scaled by 0x10077400 == 1/255. ---- */
    float    envR;                        /* 0x105CCD44                   */
    float    envG;                        /* 0x105CD9F4                   */
    float    envB;                        /* 0x105CCCF8                   */
    float    envA;                        /* 0x105CCC74                   */

    /* --- 0xFC writes these before calling 0x1001E7A0 ---------------- */
    uint32_t combineW0;                   /* 0x105D17AC                   */
    uint32_t combineW1;                   /* 0x105D17B0                   */

    /* Counters.  NOT in the original -- the port's only way to assert what
     * a handler decided, since most of these handlers decide by falling
     * through to a different exit rather than by storing anything. */
    uint32_t cVtxLoads;       /* 0x04 entered                             */
    uint32_t cVtxTransformed; /* vertices whose outcode came out zero      */
    uint32_t cVtxWritten;     /* vertices written into aVtx at all         */
    uint32_t cVtxUnresolved;  /* 0x04 with w1 the sink would not resolve   */
    uint32_t cVtxClamped;     /* v0 + n would have run past aVtx -- see .c */
    uint32_t cTriIn;          /* triangles considered (0xBF: 1, 0xB1: 2)   */
    uint32_t cTriDrawn;       /* reached grDrawTriangle                    */
    uint32_t cTriRejected;    /* all three outcodes shared a bit           */
    uint32_t cTriClipped;     /* handed to 0x1001EE70                      */
    uint32_t cRects;          /* 0xF6 entered                              */
} BrDlCmd;

/* The handler signature, and it is the original's: one command pointer in,
 * the NEXT command pointer out. */
typedef const uint8_t *(*BrDlCmdFn)(BrDlCmd *pS, const uint8_t *p);

/* Zero the state, identity matrix, unit texel scales, `cyScreen` as given.
 * `cyScreen` is 0x100A7518, which grSstWinOpen sets and which 0xF6 flips its
 * Y coordinates against. */
void BrDlCmdInit(BrDlCmd *pS, int32_t cyScreen);

/* ---------------------------------------------------------------------
 * The nine.  Each is `p` in, next command out; each returns p + 8.
 * --------------------------------------------------------------------- */

/* 0x04  0x10021A20 -- G_VTX, unlit.  Transforms n vertices from the record
 * array w1 points at into aVtx[v0..v0+n), computes the seven clip outcodes,
 * and for a wholly-inside vertex does the perspective divide, the viewport
 * map and the quarter-pixel snap.  n == 0 is a no-op. */
const uint8_t *BrDlCmdVtx(BrDlCmd *pS, const uint8_t *p);

/* 0xBF  0x1001ECF0 -- G_TRI1.  One triangle from bytes 6, 5, 4 of the
 * command (the load-time pass 0x10019250 has already halved them).  Trivially
 * rejects, clips, or completes the three vertices' texture coordinates and
 * draws. */
const uint8_t *BrDlCmdTri1(BrDlCmd *pS, const uint8_t *p);

/* 0xB1  0x1001FA30 -- G_TRI2.  Bytes 2,1,0 then bytes 6,5,4, each triangle
 * taking the same three-way decision independently: the first triangle being
 * rejected or clipped does NOT skip the second. */
const uint8_t *BrDlCmdTri2(BrDlCmd *pS, const uint8_t *p);

/* 0xF6  0x1001E320 -- G_FILLRECT with 10.2 fixed-point corners.  w0 is the
 * lower-right corner, w1 the upper-left; each word holds X in bits 23:12 and
 * Y in bits 11:0.  Y is flipped against cyScreen and both maxima are made
 * exclusive.  Calls 0x1001E380 with (ulx, cy-lry-1, lrx+1, cy-uly). */
const uint8_t *BrDlCmdFillRect(BrDlCmd *pS, const uint8_t *p);

/* 0xF7  0x1001E9F0 -- G_SETFILLCOLOR.  Expands w1's low RGBA5551 halfword to
 * four 8-bit channels by the usual 5->8 replicate, alpha from bit 0 to 0 or
 * 255.  See the RallyMain warning at the top of this header. */
const uint8_t *BrDlCmdFillColour(BrDlCmd *pS, const uint8_t *p);

/* 0xF8  0x1001EA60 -- G_SETFOGCOLOR.  grFogColorValue(w1).  Stores nothing:
 * nineteen bytes, one call, `lea eax,[esi+8]`. */
const uint8_t *BrDlCmdFogColour(BrDlCmd *pS, const uint8_t *p);

/* 0xFA  0x1001EA80 -- G_SETPRIMCOLOR.  The four bytes of w1 become four
 * floats in 0..255 (no scale), then grConstantColorValue(w1). */
const uint8_t *BrDlCmdPrimColour(BrDlCmd *pS, const uint8_t *p);

/* 0xFB  0x1001E930 -- G_SETENVCOLOR.  The four bytes of w1 become four floats
 * in 0..1, each multiplied by 0x10077400 == 1/255.  No backend call. */
const uint8_t *BrDlCmdEnvColour(BrDlCmd *pS, const uint8_t *p);

/* 0xFC  0x1001E770 -- G_SETCOMBINE.  Latches w0 and w1 into 0x105D17AC and
 * 0x105D17B0 and then calls 0x1001E7A0 with both.  Both stores happen before
 * the call, which matters: 0x1001E380 reads the pair. */
const uint8_t *BrDlCmdSetCombine(BrDlCmd *pS, const uint8_t *p);

/* Which of the nine owns this opcode, or NULL.  Exists so a test can drive
 * the dispatch by command byte -- the way 0x10023C90 does -- rather than by
 * naming a function, since "the right handler for the byte" is itself part of
 * what the table asserts. */
BrDlCmdFn BrDlCmdLookup(unsigned op);

#endif /* BR_DLCMD_H */
