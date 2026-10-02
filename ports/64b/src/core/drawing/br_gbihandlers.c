/* br_gbihandlers.c -- drawing: the F3D display-list command handlers, the
 * texture-load scanning pass, the texture upload thunks and the fade tint.
 *
 * RESPONSIBILITY: drawing/ -- turn geometry and images into pixels.
 *
 * What is here, in the original's file order:
 *  - F3D command handlers: tile size, the matrix stack (BrGbiMatrix), the
 *    textured-rectangle draws, vertex lighting (D3D only), clip codes and
 *    G_MOVEMEM (BrGbiMoveMem).
 *  - The texture-load hunt: the handlers that note the source image
 *    (BrGbiTexScanSetImg), copy a palette (BrGbiTexScanLoadTlut), stage a
 *    block of pixels (BrGbiTexScanLoadBlock) and watch the blend mode
 *    (BrGbiTexScanOtherModeL), plus the D3D-only walker.
 *  - Texture uploads: power-of-two rounding (BrGbiSizeShift) and the pass-
 *    through to the backend (BrGbiBlit).
 *  - BrFadeDrawSprite, the translucent full-screen transition tint.
 *
 * Filed out of the address batch slice2_16.c, whose preamble is carried over
 * verbatim, and the bodies keep their original order and neighbours: this
 * TU's codegen depends on everything that precedes each body.  Measured at
 * the refile (all five sweep variants): BrGbiTexScanLoadTlut/LoadBlock and
 * BrFadeDrawSprite are only byte-identical with the WHOLE of this file's
 * earlier content in front of them -- every subset probed moved them.
 * BrFadeRelease / BrFadeLatch are NOT in front of BrFadeDrawSprite (they are
 * in br_fadewipe.c); with them here BrFadeDrawSprite moves under /O2 /Op.
 * See slice2_16.h for the per-function notes and gotchas.
 */
/* The original binary is /MD: CRT calls resolve through the import table. */
#define _CRTIMP __declspec(dllimport)
/* Header prototype is the port's (table, pCmd).  The original takes only
 * pCmd; the table is the global at 0x100A79F0.  Rename the port prototype
 * in this TU so the matching body can use the original shape. */
#define BrGbiRun BrGbiRun_port
/* OtherMode H/0E and TexCreate: orig takes no state pointer: those fields
 * are standalone globals (0x10697A44 / 0x106B7AB0 / 0x118ED1C8). */
#define BrGbiTexScanOtherModeH   BrGbiTexScanOtherModeH_port
#define BrGbiTexScanOtherModeH0E BrGbiTexScanOtherModeH0E_port
#define BrGbiTexCreate           BrGbiTexCreate_port
#define BrGbiTexScanLoadTlut     BrGbiTexScanLoadTlut_port
#define BrGbiTexScanLoadBlock    BrGbiTexScanLoadBlock_port
#define BrGbiSolidTexBuild       BrGbiSolidTexBuild_port
#define BrGbiBlit                BrGbiBlit_port
#define BrFadeSetTarget          BrFadeSetTarget_port
#define BrFadeSetTargetA         BrFadeSetTargetA_port
#define BrFadeSetTargetB         BrFadeSetTargetB_port
#define BrFadeIsClosing          BrFadeIsClosing_port
#define BrFadeIsSettled          BrFadeIsSettled_port
#define BrFadeIsShut             BrFadeIsShut_port
#define BrRcaFixupArray          BrRcaFixupArray_port
/* GBI handlers: orig is `Gfx *(*)(Gfx *)` against standalone globals, not a
 * state pointer.  Same rename so the matching bodies can use that shape. */
#define BrGbiClearGeometryMode  BrGbiClearGeometryMode_port
#define BrGbiSetGeometryMode    BrGbiSetGeometryMode_port
#define BrGbiDList              BrGbiDList_port
#define BrGbiEndDList           BrGbiEndDList_port
#define BrGbiMatrix             BrGbiMatrix_port
#define BrGbiPopMatrix          BrGbiPopMatrix_port
#define BrGbiDispatch10020F50   BrGbiDispatch10020F50_port
#define BrGbiMoveMem            BrGbiMoveMem_port
#define BrGbiMoveWord           BrGbiMoveWord_port
#define BrGbiMoveMemMatrix      BrGbiMoveMemMatrix_port
/* Fade sprite: orig is (pRecs, alpha); rectIdx / otherModeH / cursor are
 * standalone globals, not a BrFadeState *. */
#define BrFadeDrawSprite        BrFadeDrawSprite_port
/* Fade bars: orig takes NO argument at all -- eleven standalone globals. */
#define BrFadeDrawBars          BrFadeDrawBars_port
#include "br_coretypes.h"   /* br_globals: its objects */
#include "br_mat.h"   /* br_globals: its objects */
#include "slice1_05.h"   /* br_globals: its objects */
#include "slice2_16.h"
#include "br_addr32.h"
#undef BrGbiRun
#undef BrGbiTexScanOtherModeH
#undef BrGbiTexScanOtherModeH0E
#undef BrGbiTexCreate
#undef BrGbiTexScanLoadTlut
#undef BrGbiTexScanLoadBlock
#undef BrGbiSolidTexBuild
#undef BrGbiBlit
#undef BrFadeSetTarget
#undef BrFadeSetTargetA
#undef BrFadeSetTargetB
#undef BrFadeIsClosing
#undef BrFadeIsSettled
#undef BrFadeIsShut
#undef BrRcaFixupArray
#undef BrGbiClearGeometryMode
#undef BrGbiSetGeometryMode
#undef BrGbiDList
#undef BrGbiEndDList
#undef BrGbiMatrix
#undef BrGbiPopMatrix
#undef BrGbiDispatch10020F50
#undef BrGbiMoveMem
#undef BrGbiMoveWord
#undef BrGbiMoveMemMatrix
#undef BrFadeDrawSprite
/* BrFadeDrawSprite: prototype in br_funcs.h */
#undef BrFadeDrawBars
/* BrFadeDrawBars: prototype in br_funcs.h */
/* Bodies live in br_gbitexscan.c; TexScanRun still calls them. */
/* BrGbiTexScanOtherModeH: prototype in br_funcs.h */
/* BrGbiTexScanOtherModeH0E: prototype in br_funcs.h */
/* BrGbiTexScanLoadTlut: prototype in br_funcs.h */
/* BrGbiTexScanLoadBlock: prototype in br_funcs.h */
/* BrGbiSolidTexBuild: prototype in br_funcs.h */
#include <stdlib.h>
/* BrGbiClearGeometryMode: prototype in br_funcs.h */
/* BrGbiSetGeometryMode: prototype in br_funcs.h */
/* BrGbiDList: prototype in br_funcs.h */
/* BrGbiEndDList: prototype in br_funcs.h */
/* BrGbiMatrix: prototype in br_funcs.h */
/* BrGbiPopMatrix: prototype in br_funcs.h */
/* BrGbiDispatch10020F50: prototype in br_funcs.h */
/* BrGbiMoveMem: prototype in br_funcs.h */
/* BrGbiMoveWord: prototype in br_funcs.h */
/* BrGbiMoveMemMatrix: prototype in br_funcs.h */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* geo.cur      */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* geo.prev     */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* DL stack n   */
/* 64-bit core: declared once, in br_globals.h or its struct's header */ /* DL stack     */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* mtx top      */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* projection   */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* modelview[0] */
/* 64-bit core: declared once, in br_globals.h or its struct's header */  /* model stack, 1-based: [top - 1] */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* mtx.f5180    */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* combined     */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* lookat 0x82  */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* lookat 0x84  */
/* 64-bit core: declared once, in br_globals.h or its struct's header */ /* lights      */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* numLights    */
/* 64-bit core: declared once, in br_globals.h or its struct's header */  /* DL write cursor */
/* 64-bit core: declared once, in br_globals.h or its struct's header */  /* fade rectIdx    */
/* 64-bit core: declared once, in br_globals.h or its struct's header */  /* otherModeH      */

/* The routines this file and br_dl.c BOTH used to transcribe.  Same original
 * function, one host body -- see br_dlshared.h. */
#include "br_dlshared.h"

#include <string.h>

/* ------------------------------------------------------------------ */
/* Local helpers                                                      */
/* ------------------------------------------------------------------ */

/* 0x1007C8A0 __ftol: truncate toward zero, return the low dword of the
 * 64-bit result. DEVIATION: inputs outside the 64-bit range are undefined in
 * C, so they are turned into 0 rather than whatever the x87 indefinite
 * value would truncate to. */
/* (port-only br16_ftol removed) */


/* ------------------------------------------------------------------ *
 * x87 COMPARISON POLARITY, SPELLED ONCE
 * ------------------------------------------------------------------ *
 * `fcomp ST0, mem` then `fnstsw ax` puts C0 in bit 0 of ah and C3 in bit 6.
 * C0 is set for LESS-THAN, C3 for EQUAL -- and an UNORDERED compare (either
 * operand a NaN) sets C0, C2 and C3 all at once.  So each mask means:
 *
 *     test ah,1     nonzero <=> a <  b  OR unordered   ==   !(a >= b)
 *     test ah,1     ZERO    <=> a >= b  AND ordered    ==    (a >= b)
 *     test ah,0x40  nonzero <=> a == b  OR unordered   ==   BR16_FEQU(a, b)
 *     test ah,0x40  ZERO    <=> a != b  AND ordered    ==   BR16_FNEO(a, b)
 *     test ah,0x41  ZERO    <=> a >  b  AND ordered    ==    (a >  b)
 *     test ah,0x41  nonzero <=> a <= b  OR unordered   ==   !(a >  b)
 *
 * FOUR OF THE SIX ARE ORDINARY C, because C's relational operators are all
 * false for NaN: `a >= b`, `!(a >= b)`, `a > b` and `!(a > b)` are exact.
 *
 * THE OTHER TWO HAVE NO C OPERATOR AT ALL, and that is the trap this file
 * fell into repeatedly.  `a == b` is FALSE for NaN where C3 is SET, and
 * `a != b` is TRUE for NaN where !C3 is CLEAR -- so BOTH of C's equality
 * operators get the unordered case wrong, in opposite directions, and there
 * is no negation that rescues either.  These two macros spell them properly,
 * using only relational operators:
 *
 *   ordered and unequal  <=>  exactly one of (a < b), (a > b) holds
 *   equal or unordered   <=>  neither holds
 *
 * Use these rather than writing the disjunction out; eight sites in this file
 * need one or the other, and every one of them was wrong before this pass. */
#define BR16_FEQU(a, b)  (!((a) < (b) || (a) > (b)))  /* C3 set   */
#define BR16_FNEO(a, b)   ((a) < (b) || (a) > (b))    /* C3 clear */

/* The 12-bit two's-complement fold the tile-size fields use went to
 * br_dlshared.c with BrDlsTileSizeDecode, which is the only thing in this
 * file that wanted it. */

/* (port-only br16_bswap32 removed) */


/* Byte-swap the u32 that starts at p, byte-wise so the host's own endianness
 * never enters into it. */
/* (port-only br16_swap_u32_at removed) */


/* (port-only br16_swap_u16_at removed) */


/* Read/write a u32 byte-wise in the host's order. The .rca payload has
 * already been swapped by the time these are read back. */
/* (port-only br16_ld32 removed) */


/* (port-only br16_st32 removed) */


/* (port-only br16_ld16 removed) */


/* Advance the fade emitter's cursor by one command and hand back the slot
 * that was reserved. Every emit site in 0x1002AF10 / 0x1002B340 is this. */
/* (port-only br16_fade_alloc removed) */


/* ================================================================== */
/* 1. F3D command handlers                                            */
/* ================================================================== */

/* 0x1001CF30 -- G_SETTILESIZE, opcode 0xF2.  The name was G_SETSCISSOR until
 * the opcode audit; BRD3D's dispatch table at 0x100A79F0 holds this address in
 * slot 0xF2 and holds 0x1001CE70 / 0x1001CDA0 in the two scissor slots, so the
 * arithmetic was never wrong -- only the name.  See slice2_16.h.
 *
 * ONE BODY: the decode is br_dlshared.c's and carries both builds' addresses.
 * br_dl.c transcribed the same 178 bytes as br_dl_settilesize. */
/* WHAT IT DOES: tells the renderer which rectangle of a texture the next
 * drawings will use, and works out that rectangle's width and height in
 * texture pixels. The coordinates arrive as fractions of a pixel and are
 * unpacked and sign-corrected here. Despite an earlier name of "set
 * scissor", this is the tile setter -- the real scissor commands are two
 * other, longer functions. */
/* (port-only BrGbiSetTileSize removed) */



/* (port-only BrGbiMtxProj removed) */


/* @n64 0x80242810 located */
/* (port-only BrGbiMtxSlot removed) */


/* The ring push used on both modelview paths: 10 wraps to 0 before the
 * increment, so `top` only ever takes 1..10. */
/* (port-only br16_mtx_push removed) */


/* top == 0 means "no modelview matrix"; the two sites that build a pointer
 * from it substitute NULL. */
/* (port-only br16_mtx_current removed) */


/* 0x10020DC0 */
/* WHAT IT DOES: installs a transform matrix -- either the camera's
 * projection, or a model's position and orientation. It can replace the
 * current matrix or combine with it, and optionally save the old one so it
 * can be restored later. It always finishes by recomputing the single
 * combined matrix the renderer actually uses. */
/* Byte-exact (2026-09-27).  Three source facts:
 * - Each push/no-push arm ends with its own copy and `DAT_105d17d0 = 0`
 *   (VC5 tail-merges the four into the shared `rep movsd; mov [..],ebp`).
 *   Four zero stores are what make VC5 hold 0 in ebp for the whole body.
 * - The copy goes through a `dst` pointer, so VC5 cannot prove the copy
 *   misses the stack top and re-reads it after every copy.
 * - The stack is indexed as the 1-based array DAT_105ccd50[top - 1], like
 *   the vertex routines: an element address is `lea edi,[eax+sym-0x40]`.
 * The unread w0 local is TU state, not code: without a sixth local symbol
 * here BrFadeDrawSprite, later in this file, swaps esi/edi (2 B). */
/* @implements 0x10021080 glide BrGbiMatrix */
BrGfxWords *BrGbiMatrix(BrGfxWords *pCmd)
{
    unsigned w0 = pCmd->w0;     /* unread; see below */
    void *pIn = (void *)(uintptr_t)pCmd->w1;
    int top;
    BrMat4 *cur;
    BrMat4 *dst;
    BrMat4 tmp;

    if ((pCmd->w0 & 0x10000u) != 0) {
        if ((pCmd->w0 & 0x20000u) != 0)
            memcpy(&DAT_105ccd00, pIn, 64);
        else
            BrMat4Mul(pIn, &DAT_105ccd00, &DAT_105ccd00);
    } else if ((pCmd->w0 & 0x20000u) != 0) {
        if ((pCmd->w0 & 0x40000u) != 0) {
            top = DAT_100a9a50;
            if (top == 10)
                top = 0;
            DAT_100a9a50 = ++top;
            dst = &(*(BrMat4 (*)[])&DAT_105ccd50)[top - 1];
            memcpy(dst, pIn, 64);
            DAT_105d17d0 = 0;
        } else {
            dst = &(*(BrMat4 (*)[])&DAT_105ccd50)[DAT_100a9a50 - 1];
            memcpy(dst, pIn, 64);
            DAT_105d17d0 = 0;
        }
    } else {
        cur = DAT_100a9a50 != 0 ? &(*(BrMat4 (*)[])&DAT_105ccd50)[DAT_100a9a50 - 1] : NULL;
        BrMat4Mul(pIn, cur, &tmp);
        if ((pCmd->w0 & 0x40000u) != 0) {
            top = DAT_100a9a50;
            if (top == 10)
                top = 0;
            DAT_100a9a50 = ++top;
            dst = &(*(BrMat4 (*)[])&DAT_105ccd50)[top - 1];
            memcpy(dst, &tmp, 64);
            DAT_105d17d0 = 0;
        } else {
            dst = &(*(BrMat4 (*)[])&DAT_105ccd50)[DAT_100a9a50 - 1];
            memcpy(dst, &tmp, 64);
            DAT_105d17d0 = 0;
        }
    }
    cur = DAT_100a9a50 != 0 ? &(*(BrMat4 (*)[])&DAT_105ccd50)[DAT_100a9a50 - 1] : NULL;
    BrMat4Mul(cur, &DAT_105ccd00, &(*(BrMat4 *)&DAT_105d1760));
    return pCmd + 1;
}

/* 0x10021510 */
/* WHAT IT DOES: draws a textured rectangle straight onto the screen -- the
 * command used for things like heads-up display panels rather than for 3D
 * geometry. It unpacks the corners and the tile number and hands them to the
 * rectangle drawer. Note it swallows three commands, not one, because the
 * rectangle's texture coordinates follow it in the list. */
/* The decode is br_dlshared.c's, which carries this address and BRGlide's
 * 0x10021570. */
/* (port-only BrGbiTileRect removed) */


/* 0x10021B80 */
/* WHAT IT DOES: the same textured-rectangle draw as above but with the
 * corners given in whole screen pixels rather than quarter-pixels, so it
 * scales them up before handing them on, and it consumes only its own
 * command. */
/* Likewise 0x10021B80 / BRGlide 0x100219D0. */
/* (port-only BrGbiTileRectS removed) */


/* 0x10022350 -- AND IT IS *NOT* A DUPLICATE OF br_dl.c's
 * br_dl_light_vertex, whatever the pairing table says.
 *
 * config/shared.csv pairs this with BRGlide 0x10022AC0 as `shared`, matched
 * by `shape` -- the weakest class it has, a similarity rather than a byte
 * match. Compared instruction by instruction the two are the same routine
 * with ONE constant changed, three times over:
 *
 *     0x10022B35 (Glide)  mov eax, 0x437F0000   == 255.0f
 *     0x10022385 (D3D)    mov eax, 0x3F800000   ==   1.0f
 *
 * and the limit each compares against matches its own ceiling (0x10077418 is
 * 255.0f, 0x1008F3C4 is 1.0f). Glide's iterated colour runs 0..255 and D3D's
 * runs 0..1, so this is a real behavioural divergence between the builds and
 * the row should be classed `renderer` -- "same slot, different body" -- not
 * `shared`. Two host bodies is correct here; they are two functions.
 *
 * (The x87 scheduling also differs at the top -- Glide loads +0x1C first and
 * D3D loads +0x18 -- but the constant-to-component pairing is identical in
 * both, so the dot product is the same sum in the same order.) */
/* WHAT IT DOES: works out how bright one vertex of a model should be. If
 * lighting is switched off it just copies a fixed colour. Otherwise it
 * measures how squarely the surface faces the light: surfaces facing away
 * get plain ambient light, and the rest get ambient plus a share of the
 * light's colour, capped so nothing goes brighter than white. */
/* @d3donly 0x10022350 BrGbiLightVertex -- glide twin 0x10022AC0 claimed by br_dl.c:br_dl_light_vertex */
/* (port-only BrGbiLightVertex removed) */


/* 0x10022DC0 -- ONE BODY, in br_dlshared.c, which carries both builds'
 * addresses.  This is now only the float-array layout: +0x04 x, +0x08 y,
 * +0x0C z, +0x18 w.
 *
 * KEPT AS A WARNING: the two copies of this function -- here and
 * br_dl_outcode in br_dl.c -- disagreed about NaN for as long as both
 * existed, and this was the wrong one: it wrote `x < 0.0f`, which is false
 * for NaN, so an unordered vertex was reported INSIDE and fed through the
 * clipper. The original's `test ah,1` reads C0, which an unordered compare
 * sets, so a NaN is REJECTED. Making the two agree was not the fix; deleting
 * one of them was. */
/* (port-only BrGbiClipCodes removed) */


/* 0x10024240 */
/* WHAT IT DOES: handles the drawing command that hands the renderer a ready-
 * made combined transform matrix outright, replacing whatever the matrix
 * commands had built up. */
/* port-only body; Glide match is src/core/generated/0x10023900.c */

/* 0x10024150  G_MOVEMEM.
 *
 * Jump table recovered from the DLL (byte table 0x1002421C, targets
 * 0x100241E8):  0x80 -> 0x10024179, 0x82 -> 0x10024185,
 * 0x84 -> 0x10024193, 0x86/88/8A/8C/8E/90/92/94 -> 0x100241AE,
 * 0x9E -> 0x100241A2, everything else in 0x80..0x9E -> 0x100241E2. */
/* WHAT IT DOES: the drawing command that loads a block of renderer data from
 * memory: the viewport, one of the two look-at vectors, one of the eight
 * lights, or the combined transform matrix, chosen by an index byte. Indexes
 * outside the known set are ignored. The port clamps a light copy to the
 * light array, which the original did not. */
/* @implements 0x10023810 glide BrGbiMoveMem */
BrGfxWords *BrGbiMoveMem(BrGfxWords *pCmd)
{
    /* Hand-transcribed from the asm.  The index is a CAST to unsigned char:
     * that keeps w0 in esi and extracts with `shr eax,0x10; and eax,0xff`,
     * where `& 0xFF` makes VC5 spill w0 and re-read one byte of it.  Case
     * blocks follow source order, so the matrix case sits before the light
     * copies, as in the original. */
    unsigned w0  = pCmd->w0;
    unsigned idx = (unsigned char)(w0 >> 16);

    switch (idx) {
    case 0x80:
        return BrGbiCall10024260(pCmd);
    case 0x82:
        DAT_105ce2d8 = br_ptr32(pCmd->w1);
        return pCmd + 1;
    case 0x84:
        DAT_105ce2dc = br_ptr32(pCmd->w1);
        return pCmd + 1;
    case 0x9E:
        return BrGbiMoveMemMatrix(pCmd);
    case 0x86:
    case 0x88:
    case 0x8A:
    case 0x8C:
    case 0x8E:
    case 0x90:
    case 0x92:
    case 0x94:
        /* dest is 0x105CCC78 + ((idx-0x86)>>1)*16, length w0's low 16 */
        memcpy(DAT_105ccc78 + ((idx - 0x86) >> 1) * 16,
               (void *)(uintptr_t)pCmd->w1, w0 & 0xFFFF);
        DAT_105d17d0 = 0;
        break;
    }
    return pCmd + 1;
}

/* WHAT IT DOES: handle the display-list command that loads a matrix, by
 * copying its 64 bytes from wherever the list points into the renderer's
 * current-matrix slot. Returns the pointer to the next command. */
/* @implements 0x10023900 glide BrGbiMoveMemMatrix */
BrGfxWords *BrGbiMoveMemMatrix(BrGfxWords *pCmd)
{
    memcpy(&(*(BrMat4 *)&DAT_105d1760), (const void *)pCmd->w1, 64);
    return pCmd + 1;
}

/* ================================================================== */
/* 2. Texture-load scanning pass                                      */
/* ================================================================== */

/* (port-only BrGbiTexScanData removed) */


/* 0x10029E80  G_TEXTURE */
/* (port-only BrGbiTexScanTexture removed) */


/* 0x10029EB0  G_SETTIMG */
/* WHAT IT DOES: during the texture-load hunt, notes the address and pixel
 * size of the image a load is about to read from, and -- if a run was not
 * already in progress -- marks this command as where the run begins. */
/* @implements 0x10029EB0 d3d BrGbiTexScanSetImg */
/* @t4-pass 0x10029420 1 2026-09-07 probes 53 bytes 83 insns 24 regions 2 rows 0 census yes  (tools/crank.py) */
/* @t4-pass 0x10029420 2 2026-09-07 probes 53 bytes 83 insns 24 regions 2 rows 0 census yes  (tools/crank.py) */
/* @t3 0x10029420 2026-09-09 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 83/83 insns 24/24 rows 0+0 regions 2 oracle UNCLASSIFIED
 * @t3-effort passes 2 zero-movement 1 2
 * residue is register colouring only: identical register-blind instruction
 * multiset (rows 0+0), 2 masked regions;
 * every row pairs under t3.py's canonical classes.  Effort: 2 counted
 * @t4-pass passes (ledger lines above, zero movement on passes 1 and 2);
 * crank candidates and scores in build/match/crank.log, dead probes in the
 * comment block above.  Do not reopen before the end-grind. */
/* @implements 0x10029420 glide BrGbiTexScanSetImg */
void BrGbiTexScanSetImg(BrGfxWords *pCmd)
{
    int32_t s = g_brTexScanState;
    int32_t z = 0;

    if (s != z && s != 3 && s != 6)
        return;

    g_brTexScanTimgSiz  = (int32_t)((pCmd->w0 >> 19) & 3u);
    g_brTexScanTimgAddr = pCmd->w1;
    g_brTexScanSrcSeen  = (uint32_t)z;
    if (s == z) {
        g_brTexScanRunStart = pCmd;
        g_brTexScanRunEnd   = (BrGfxWords *)z;
    }
    g_brTexScanState = 1;
}

/* 0x10029F10  G_LOADTLUT */
/* WHAT IT DOES: during the texture-load hunt, copies a colour palette out of
 * the source image into the palette buffer. The number of bytes comes
 * straight from the command and is not checked, here or in the original. */
/* @implements 0x10029F10 d3d BrGbiTexScanLoadTlut */
/* @t4-pass 0x10029480 1 2026-09-07 probes 25 bytes 109 insns 34 regions 1 rows 0 census yes  (tools/crank.py) */
/* @t4-pass 0x10029480 2 2026-09-07 probes 62 bytes 109 insns 34 regions 1 rows 0 census yes  (tools/crank.py) */
/* @t4-pass 0x10029480 3 2026-09-07 probes 62 bytes 109 insns 34 regions 1 rows 0 census yes  (tools/crank.py) */
/* @t3 0x10029480 2026-09-07 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 109/109 insns 34/34 rows 0+0 regions 1 oracle UNCLASSIFIED
 * @t3-effort passes 3 zero-movement 2 3
 * residue after tools/crank.py: 62 compiles this pass, levers accepted: none;
 * every candidate and score is in build/match/crank.log.
 * Do not reopen before the end-grind. */
/* @implements 0x10029480 glide BrGbiTexScanLoadTlut */
/* 64-bit core: declared once, in br_globals.h or its struct's header */          /* tlut dest, 0x100A9E58 */
void BrGbiTexScanLoadTlut(const BrGfxWords *pCmd)
{
    int32_t  ds, dt;
    uint32_t len;
    uint8_t *src;

    if (g_brTexScanState != 1)
        return;

    /* dt BEFORE ds: the original shifts w0 before w1 in each pair, which only
     * comes out of computing the shifted difference first (19 -> 4 diffs).
     * RESIDUE 4: inside `ds` the original still copies and masks w0's half
     * before w1's, and nothing in the source moves that -- a w0 temp, and a
     * negated subtraction, both leave it. */
    dt = (int32_t)((pCmd->w1 >> 12) & 0xFFFu) -
         (int32_t)((pCmd->w0 >> 12) & 0xFFFu);
    ds = (int32_t)(pCmd->w1 & 0xFFFu) - (int32_t)(pCmd->w0 & 0xFFFu);
    len = (uint32_t)((ds + 1) * (dt + 1)) << 1;

    /* The source pointer is read HERE, not at the top: the original loads the
     * destination global first and the timg pointer only when the length is
     * done, and that order is what puts the copy's src/dst in the original's
     * registers. */
    src = (uint8_t *)g_brTexScanTimgAddr;
    g_brTexScanSrcSeen = (uint32_t)src;
    memcpy(DAT_100a9e58, src, len);
    g_brTexScanState = 7;
}

/* 0x10029FA0  G_LOADBLOCK */
/* WHAT IT DOES: during the texture-load hunt, copies the texture's pixels
 * into a staging buffer so the texture cache can be offered them later. It
 * records the size the command asked for even though the copy itself is
 * clamped to the buffer, which the original was not. */
/* RESIDUE (46 masked diffs, T3a, REGNORM 0+0, one byte SHORT): the two
 * command words are homed in each other's registers, and ours puts the
 * one that gets the short `and eax,imm32` encoding on the other side.
 * Every instruction is the original's. */
/* @implements 0x10029FA0 d3d BrGbiTexScanLoadBlock */
/* @t4-pass 0x10029510 1 2026-09-07 probes 67 bytes 93 insns 27 regions 1 rows 0 census yes  (tools/crank.py) */
/* @t4-pass 0x10029510 2 2026-09-07 probes 67 bytes 93 insns 27 regions 1 rows 0 census yes  (tools/crank.py) */
/* @t3 0x10029510 2026-09-09 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 93/94 insns 27/27 rows 0+0 regions 1 oracle UNCLASSIFIED
 * @t3-effort passes 2 zero-movement 1 2
 * residue is register colouring only: identical register-blind instruction
 * multiset (rows 0+0), 1 masked region, 1 B short on encoding;
 * every row pairs under t3.py's canonical classes.  Effort: 2 counted
 * @t4-pass passes (ledger lines above, zero movement on passes 1 and 2);
 * crank candidates and scores in build/match/crank.log, dead probes in the
 * comment block above.  Do not reopen before the end-grind. */
/* @implements 0x10029510 glide BrGbiTexScanLoadBlock */
/* 64-bit core: declared once, in br_globals.h or its struct's header */          /* stageSrc, 0x105D17F0 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */          /* stageLen, 0x10697A54 */
void BrGbiTexScanLoadBlock(const BrGfxWords *pCmd)
{
    int32_t  d;
    uint32_t len;
    uint8_t *src;

    if (g_brTexScanState != 2)
        return;

    src = (uint8_t *)g_brTexScanTimgAddr;
    d = (int32_t)((pCmd->w1 >> 12) & 0xFFFu) -
        (int32_t)((pCmd->w0 >> 12) & 0xFFFu);
    DAT_105d17f0 = (uint32_t)src;
    len = (uint32_t)(d + d + 2);
    DAT_10697a54 = (int32_t)len;
    memcpy(g_brTexScanStage, src, len);
    g_brTexScanState = 3;
}

/* 0x1002A1A0  G_SETOTHERMODE_L */
/* WHAT IT DOES: during the texture-load hunt, watches for changes to the
 * blending mode and works out whether the material being set up is one that
 * needs special handling. A handful of specific blend settings, and anything
 * without two particular bits set, turn the flag off. */
/* @implements 0x1002A1A0 d3d BrGbiTexScanOtherModeL */
/* @implements 0x10029710 glide BrGbiTexScanOtherModeL */
void BrGbiTexScanOtherModeL(const BrGfxWords *pCmd)
{
    uint32_t v;
    uint32_t z;

    if ((pCmd->w0 & 0xFF00u) != 0x300u)
        return;

    v = pCmd->w1;
    if (v == 0x504F50u) {
        g_brTexScan575414 = 0;
        return;
    }
    if (v == 0xC184240u) {
        g_brTexScan575414 = 0;
        return;
    }
    if (v == 0x504240u) {
        g_brTexScan575414 = 0;
        return;
    }
    /* The zero lives in a local and the bit test is written in POSITIVE
     * form with the zero store last.  That puts `xor ecx,ecx` in a register
     * shared by the compare and the final store, so the final exit block
     * (`mov [g],ecx`) differs from the three immediate-zero exits above and
     * VC5 does not cross-jump them into one. */
    z = 0;
    if (v == z) {
        g_brTexScan575414 = (int32_t)z;
        return;
    }
    if (v & 0x1800u) {
        g_brTexScan575414 = (int32_t)((v >> 16) & 1u);
        return;
    }
    g_brTexScan575414 = (int32_t)z;
}

/* BrGbiTexScanOtherModeH / OtherModeH0E filed to drawing/br_gbitexscan.c. */

/* 0x100290E0
 *
 * Opcode table recovered from the DLL (byte table 0x10029308, targets
 * 0x100292BC). The mapping is exactly F3D:
 *   0x04, 0xB1, 0xBF -> flush     0xB8 -> stop
 *   0xB9 -> othermode_l then mark 0xBA -> othermode_h
 *   0xBB -> texture               0xE6 -> loadsync
 *   0xE7 -> pipesync              0xE8 -> tilesync
 *   0xF0 -> loadtlut              0xF2 -> settilesize
 *   0xF3 -> loadblock             0xF5 -> settile
 *   0xFA -> prim colour           0xFB -> env colour
 *   0xFC -> combine probe         everything else -> mark
 *
 * DEVIATION: G_LOADTLUT and G_LOADBLOCK need the bytes their w1 points at.
 * The original dereferences the already-segment-fixed address; here the
 * walker goes through BrGbiTexScanData so a 64-bit host can supply them. */
/* WHAT IT DOES: walks a whole list of drawing commands without drawing
 * anything, looking for the multi-command sequences the N64 used to load a
 * texture so they can be collapsed into a single command for the PC
 * renderer. Along the way it also picks up the colours, texture slots and
 * render modes in force. It stops at the end-of-list command. */
/* @d3donly 0x100290E0 BrGbiTexScanRun -- glide twin 0x10028820 is the Glide arm in drawing/br_gbitexscan.c */
/* (the Glide arm is at the END of this file -- see the note there) */

/* ================================================================== */
/* 2b. Texture upload thunks                                          */
/* ================================================================== */

/* 0x10027C00 */
/* WHAT IT DOES: rounds a size up to the next power of two and reports the
 * answer as a shift count -- how many times you would double 1 to reach it.
 * Textures have to be powers of two, so this is how an odd width or height
 * gets rounded up. Anything above 128 is capped, and anything of 1 or less
 * gives zero. */
/* @t4-pass 0x10027290 1 2026-09-07 probes 50 bytes 97 insns 34 regions 8 rows 0 census yes  (tools/crank.py) */
/* @t4-pass 0x10027290 2 2026-09-07 probes 50 bytes 97 insns 34 regions 8 rows 0 census yes  (tools/crank.py) */
/* DEAD 2026-09-09: branchy-tail respellings (returns adjacent/reversed,
 * r=8-first, r initialised at declaration, ternary, reversed compare, a
 * copy local for the whole chain, unsigned param with per-site casts, K&R
 * declaration) -- the setg lowering (-2 B) or n in ecx (+1 B) every time;
 * every slot in the TU (51 of 64 compile).
 * @t4-pass 0x10027290 3 2026-09-09 probes 10 bytes 97 insns 34 regions 1 rows 0 census yes  (hand, fn.py variants)
 * @t4-pass 0x10027290 4 2026-09-09 probes 51 bytes 97 insns 34 regions 1 rows 0 census yes  (position sweep) */
/* @t3 0x10027290 2026-09-19 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 94/96 insns 35/34 rows 3+4 regions 1 oracle EQUIVALENT
 * @t3-effort passes 6 zero-movement 5 6
 * 2026-09-19 respell (94 B, FITS the 96 B image slot): reassigning the
 * PARAMETER for the final pair keeps operand and result in eax (the old
 * named-r/volatile spellings cost the byte), at the price of the setg
 * lowering in the tail (the 7 rows / +1 insn vs the original's branchy
 * cmp/mov/jle).  A5 oracle EQUIVALENT on 64 inputs; behavioural verdict
 * outranks the byte residue.  Dead probes: the comment block above.
 * Do not reopen before the end-grind. */
/* @t4-pass 0x10027290 5 2026-09-19 probes 26 bytes 94 insns 35 regions 1 rows 7 census yes  (tools/crank.py) */
/* @t4-pass 0x10027290 6 2026-09-19 probes 26 bytes 94 insns 35 regions 1 rows 7 census yes  (tools/crank.py) */
/* @implements 0x10027290 glide BrGbiSizeShift */
int BrGbiSizeShift(int n)
{
    if (n <= 1)    return 0;
    if (n <= 2)    return 1;
    if (n <= 4)    return 2;
    if (n <= 8)    return 3;
    if (n <= 0x10) return 4;
    if (n <= 0x20) return 5;
    if (n <= 0x40) return 6;
    /* Last pair is one ret: cmp 0x80; mov 7; jle; mov 8.  Adjacent
     * `return 7; return 8` lowers to setg+add; a named r gives the branchy
     * form but claims eax, pushing n into ecx -- and the eax-specific
     * `cmp eax,imm32` is 1 byte shorter, which is exactly the byte that
     * decides whether the body FITS its slot.  Reassigning the PARAMETER
     * keeps result and operand in one register. */
    if (n > 0x80)
        n = 8;
    else
        n = 7;
    return n;
}

/* 0x10028BF0 */
/* WHAT IT DOES: passes a texture upload through to the graphics backend,
 * working out for it the one thing it does not get told -- how many bytes
 * one row of the texture occupies, given the width rounded up to a power of
 * two and the pixel size -- and hands back the texture handle the backend
 * returns (the font, shadow and panel textures are kept by that handle). */
/* @t3 0x10027F00 2026-09-09 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 124/124 insns 52/52 rows 0+0 regions 2 oracle UNCLASSIFIED
 * @t3-effort passes 4 zero-movement 3 4
 * residue is register colouring only: identical register-blind instruction
 * multiset (rows 0+0), 2 masked regions;
 * every row pairs under t3.py's canonical classes.  Effort: 4 counted
 * @t4-pass passes (ledger lines above, zero movement on passes 3 and 4);
 * crank candidates and scores in build/match/crank.log, dead probes in the
 * comment block above.  Do not reopen before the end-grind. */
/* @implements 0x10028BF0 d3d BrGbiBlit */
/* @t4-pass 0x10027F00 1 2026-09-07 probes 18 bytes 124 insns 52 regions 3 rows 0 census yes  (tools/crank.py) */
/* @t4-pass 0x10027F00 2 2026-09-07 probes 18 bytes 124 insns 52 regions 3 rows 0 census yes  (tools/crank.py) */
/* DEAD 2026-09-09 (all at 124 B RAW 6+6 REGNORM 0+0 -- a three-register
 * role rotation esi/edi/ebx): named locals for a3, a5, the shift or the
 * texels-per-word result; 8* on the left; pitch declared first; both
 * declared then assigned; unsigned rounded; &31 on the shift (+3);
 * dropping the intptr_t cast; every slot in the TU (14 of 64 compile).
 * @t4-pass 0x10027F00 3 2026-09-09 probes 10 bytes 124 insns 52 regions 2 rows 0 census yes  (hand, fn.py variants)
 * @t4-pass 0x10027F00 4 2026-09-09 probes 14 bytes 124 insns 52 regions 2 rows 0 census yes  (position sweep; 14 of 64 slots compile for this block) */
/* @implements 0x10027F00 glide BrGbiBlit */
/* The original takes 14 args and calls through the import-pointer global
 * at 0x118ED1C4 (the slot before BrGbiTexCreate's 0x118ED1C8); the port's
 * pfn parameter is a port convenience. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* 0x118ED1C4 */

uintptr_t BrGbiBlit(uintptr_t a1, uintptr_t a2, uintptr_t a3, uintptr_t a4,
                    uintptr_t a5, uintptr_t a6, uintptr_t a7, uintptr_t a8,
                    uintptr_t a9, uintptr_t a10, uintptr_t a11, uintptr_t a12,
                    uintptr_t a13, uintptr_t a14)
{
    int32_t   rounded = (int32_t)(1 << BrGbiSizeShift((int)a3));
    int32_t   pitch   = (rounded / BrGbiTexelsPerWord((int)a5)) * 8;

    return g_pfn18ED1C4(a1, a2, a3, a4, (uintptr_t)(intptr_t)pitch,
                        a5, a6, a7, a8, a9, a10, a11, a12, a13, a14);
}

/* The 0x3EB / 0x3E8 / 0 token soup both emit paths hand to
 * BrRdpSetCombineLERP; spelled out once so the two call sites stay readable
 * and identical to the original's push order. */
/* (port-only br16_combine removed) */


/* 0x1002AF10 */
/* WHAT IT DOES: draws the translucent full-screen tint used during a
 * transition, at the requested opacity. It gives up entirely if the tint
 * would be too faint to see, and never lets it get more than about seven-
 * tenths opaque. The rectangle it covers comes from a table of screen
 * regions. */
/* @implements 0x1002AF10 d3d BrFadeDrawSprite */
/* @implements 0x10017F80 glide BrFadeDrawSprite */
/* orig: cdecl (pRecs, alpha); cursor DAT_106e7710, rectIdx DAT_106ec798,
 * otherModeH DAT_106e7718. Combine is BrRdpSetCombineLERP(DAT++, 17 args)
 * with 0x3EB/0x3E8 immediates hoisted across the preceding emits.
 * (int)(alpha * 255.0f) is __ftol, not br16_ftol(double). */
void BrFadeDrawSprite(const uint32_t *pRecs, float alpha)
{
    BrGfxWords     *p;
    uint32_t        lo, hi;     /* unused, but load-bearing: removing them
                                 * changes this body's codegen (verified) */
    int             idx;

    if (!(alpha >= 0.1f))
        return;
    if (alpha > 0.7f)
        alpha = 0.7f;

    p = (*(BrGfxWords * *)&g_BrGfxPtr)++;
    p->w0 = 0xE7000000u;
    p->w1 = 0;

    p = (*(BrGfxWords * *)&g_BrGfxPtr)++;
    p->w0 = 0xBA001402u;
    p->w1 = 0;

    p = (*(BrGfxWords * *)&g_BrGfxPtr)++;
    p->w0 = 0xB900031Du;
    p->w1 = 0x00504340u;

    BrRdpSetCombineLERP((*(BrGfxWords * *)&g_BrGfxPtr)++,
                        0, 0, 0, 0x3EB,
                        0, 0, 0, 0x3EB,
                        0, 0, 0, 0x3EB,
                        0, 0, 0, 0x3EB);

    p = (*(BrGfxWords * *)&g_BrGfxPtr)++;
    p->w0 = 0xFA000000u;
    p->w1 = (uint32_t)(int)(alpha * 255.0f) | 0xFFFFFF00u;

    p = (*(BrGfxWords * *)&g_BrGfxPtr)++;
    p->w0 = 0xBA000602u;
    p->w1 = 0xC0u;

    {
        /* 88-byte records: idx*11 then [base+eax*8+disp]. orig loads
         * +0xC / +4 / +8 into esi/edi/ebx, lea-base, then [eax] for +0. */
        typedef struct BrFadeRect {
            uint32_t x0, y0, x1, y1;
            uint32_t pad[18];
        } BrFadeRect;
        const BrFadeRect *pr;
        const uint32_t *recs;
        uint32_t s, d, b, a;

        recs = pRecs;
        p = (*(BrGfxWords * *)&g_BrGfxPtr)++;
        idx = (*(int *)&g_BrEnvSection);
        pr = (const BrFadeRect *)recs + idx;
        /* RESIDUE, T3a, 2 bytes, DO NOT RE-PROBE. /O2 canonicalizes y1+y0 to
         * load y0 first; orig loads +0xC then +4, so its accumulator is y1 and
         * ours is y0 -- FIRSTDIV +0x13a, DIFFS 2, RAW and REGNORM both 0+0.
         * Dead probes, all four byte-identical to what is here: swapping which
         * field goes into which temp; dropping `d` (`s = pr->y1; s += pr->y0`);
         * the whole w0 as one expression with no temps; leading the `|` chain
         * with the y-term instead of the constant. Volatile access order blew
         * the scaled [edx+eax*8] form. The x1+x0 pair below matches the
         * original and differs only in reaching its second operand base-only,
         * which is what makes this allocation and not source -- see the
         * "INTEGER adds of two fields of the SAME struct" entry in
         * docs/VC5-IDIOMS.md. */
        s = pr->y1;
        d = pr->y0;
        b = pr->x1;
        s += d;
        a = pr->x0;
        b += a;
        p->w0 = 0xE1000000u | ((b << 12) & 0xFFF000u) | (s & 0xFFFu);

        idx = (*(int *)&g_BrEnvSection);
        pr = (const BrFadeRect *)recs + idx;
        p->w1 = ((pr->x0 & 0xFFFu) << 12) | (pr->y0 & 0xFFFu);
    }

    BrRdpSetCombineLERP((*(BrGfxWords * *)&g_BrGfxPtr)++,
                        0, 0, 0, 0x3EB,
                        0, 0, 0, 0x3EB,
                        0, 0, 0, 0x3E8,
                        0, 0, 0, 0x3E8);

    p = (*(BrGfxWords * *)&g_BrGfxPtr)++;
    p->w0 = 0xE7000000u;
    p->w1 = 0;

    p = (*(BrGfxWords * *)&g_BrGfxPtr)++;
    p->w0 = 0xBA000602u;
    p->w1 = (uint32_t)(*(int *)&BrG_6C0688);
}
