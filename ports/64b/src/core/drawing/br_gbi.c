/* br_gbi.c -- drawing: the display-list opcode handlers.
 *
 * RESPONSIBILITY: drawing/ -- turn geometry and images into pixels.
 *
 * Filed out of slice2_16.c, an address batch and not a module.  Each
 * function here is one slot of the game's drawing-command dispatch: set a
 * graphics register, turn geometry switches on or off, call into another
 * command list and come back, pop a matrix, or run the loop that walks the
 * list.  See slice2_16.h for the per-function notes and gotchas.
 *
 * slice2_16.c's preamble is carried over verbatim, renames included -- it is
 * what lets the matching bodies define the originals' no-state-pointer
 * signatures while every other translation unit keeps calling the port's.
 * An include set that looks redundant has already been shown elsewhere in
 * this module to move VC5's register allocation (see br_rdpmode.c), so
 * nothing here is trimmed on the grounds that it is unused.
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
#include "slice2_16.h"
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

/* 0x1001CD60 */
/* WHAT IT DOES: handles one command in the game's drawing-command list by
 * stashing the command's payload in a graphics setting, then moves on to the
 * next command. What that setting controls is not established -- nothing in
 * this packet reads it back -- so treat the effect on the picture as
 * unknown. */
/* @implements 0x1001CD60 d3d BrGbiSet0A79E8 */
/* @implements 0x1001EB10 glide BrGbiSet0A79E8 */
BrGfxWords *BrGbiSet0A79E8(BrGfxWords *pCmd)
{
    g_brGbi0A79E8 = pCmd->w1;
    return pCmd + 1;
}

/* 0x1001CD80 */
/* WHAT IT DOES: another one-line drawing-command handler that parks the
 * command's payload in a graphics setting and moves on. As with its
 * neighbour, what the setting is used for is not established. */
/* @implements 0x1001CD80 d3d BrGbiSet4C5174 */
/* @implements 0x1001EB30 glide BrGbiSet4C5174 */
BrGfxWords *BrGbiSet4C5174(BrGfxWords *pCmd)
{
    g_brGbi4C5174 = pCmd->w1;
    return pCmd + 1;
}

/* 0x1001E790 */
/* WHAT IT DOES: turns geometry features off. Drawing commands carry a set of
 * switches -- lighting, fog, backface culling and the like -- and this
 * clears the ones named in the command, remembering what they were before,
 * then tells the renderer the switches changed. */
/* @implements 0x1001FD40 glide BrGbiClearGeometryMode */
BrGfxWords *BrGbiClearGeometryMode(BrGfxWords *pCmd)
{
    /* The update is a compound assignment on the GLOBAL, not on a local copy
     * of it. `cur = g; g2 = cur; cur &= ~w1; g = cur;` says the same thing and
     * costs three bytes: VC5 accumulates into whichever register dies first,
     * which for a local copy is the mask, so it emits `and ecx,eax` and an
     * extra load. Reading the global once and updating it in place pins the
     * accumulator to the global's own value -- `and eax,ecx`. */
    DAT_105d17cc = DAT_105d17c8;
    DAT_105d17c8 &= ~(int)pCmd->w1;
    BrGbiGeoModeChanged();
    return pCmd + 1;
}

/* 0x10020F20 */
/* WHAT IT DOES: turns geometry features on: the mirror image of the clear
 * above. It sets the switches named in the command, remembers the previous
 * setting, and notifies the renderer. */
/* @implements 0x100211E0 glide BrGbiSetGeometryMode */
BrGfxWords *BrGbiSetGeometryMode(BrGfxWords *pCmd)
{
    /* In place on the global -- see BrGbiClearGeometryMode above. Here it is
     * also what folds the command word into the memory operand of the `or`. */
    DAT_105d17cc = DAT_105d17c8;
    DAT_105d17c8 |= (int)pCmd->w1;
    BrGbiGeoModeChanged();
    return pCmd + 1;
}

/* 0x10020D60 */
/* WHAT IT DOES: jumps the drawing-command reader into another list of
 * commands -- the equivalent of calling a subroutine while drawing. Unless
 * the command says otherwise it remembers where to come back to. The return-
 * address stack holds ten entries but the game complains one entry early,
 * and stores anyway. */
/* @implements 0x10021020 glide BrGbiDList */
BrGfxWords *BrGbiDList(BrGfxWords *pCmd)
{
    int n;

    if ((pCmd->w0 & 0x00FF0000u) == 0) {
        /* GOTCHA: the guard tests the value the counter is ABOUT to take and
         * then stores anyway, so slot 9 is written and reported both.
         * Orig calls exit() through the IAT, not a local helper.
         * `inc eax` not `lea ecx,[eax+1]`: increment the loaded counter. */
        /* Every use reads the COUNTER GLOBAL, and only the guard's `+ 1` is
         * named. Caching it in a local (`n = g; n++; ... n = g;`) makes VC5
         * hold the loaded value across the guard -- `lea ecx,[eax+1]` where
         * the original destroys it with `inc eax` and reloads after the
         * exit call, which the call forces anyway. */
        n = DAT_105ccfe8 + 1;
        if (n == 10)
            exit(1);
        DAT_105ce2e8[DAT_105ccfe8] = (int)(pCmd + 1);
        DAT_105ccfe8 = DAT_105ccfe8 + 1;
    }
    return (BrGfxWords *)(uintptr_t)pCmd->w1;
}

/* 0x10020DA0 -- takes no argument in the original. */
/* WHAT IT DOES: ends the current list of drawing commands and returns to
 * whoever jumped into it. If nothing jumped in, it reports that there is
 * nowhere to go back to, which is what stops the drawing-command reader
 * altogether. */
/* @implements 0x10021060 glide BrGbiEndDList */
BrGfxWords *BrGbiEndDList(void)
{
    int n = DAT_105ccfe8;

    /* WRAPPING IF, not an early-exit goto.  With `if (n == 0) goto empty;`
     * the zero-return becomes the FALL-THROUGH block, and VC5 -- which knows
     * eax is zero on that edge, having just tested it -- drops the `xor
     * eax,eax` entirely and returns whatever is in eax (26 -> 24 bytes).  The
     * original materialises the zero, so its return-0 is a SEPARATE trailing
     * block reached by `je`, which is what a wrapping if produces. */
    if (n != 0) {
        n -= 1;
        DAT_105ccfe8 = n;
        return (BrGfxWords *)DAT_105ce2e8[n];
    }
    return (BrGfxWords *)0;
}

/* 0x10020EF0 */
/* WHAT IT DOES: restores the previously saved model matrix, undoing one save
 * made by the matrix command above. If nothing was saved it does nothing. */
/* @implements 0x100211B0 glide BrGbiPopMatrix */
BrGfxWords *BrGbiPopMatrix(BrGfxWords *pCmd)
{
    int top = DAT_100a9a50;

    if (top != 0) {
        top -= 1;
        DAT_100a9a50 = top;
        if (top == 0)
            DAT_100a9a50 = 10;
    }
    return pCmd + 1;
}

/* 0x10020F80 */
/* WHAT IT DOES: handles a drawing command that parks the command's payload
 * in a graphics setting and then passes that same value to another routine
 * which acts on it. What the setting means is not established here. */
/* @implements 0x10020F80 d3d BrGbiSet4C1694 */
/* @implements 0x10021250 glide BrGbiSet4C1694 */
BrGfxWords *BrGbiSet4C1694(BrGfxWords *pCmd)
{
    /* w1 named once: the original loads it a single time and reuses that
     * register for both the store and the call argument. */
    uint32_t w1 = pCmd->w1;

    g_brGbi4C1694 = w1;
    BrGbiCall10020FA0(w1);
    return pCmd + 1;
}

/* 0x10020F50 */
/* WHAT IT DOES: a drawing command with a small selector byte in it: selector
 * 0 and selector 3 each go to a different handler, and anything else is
 * ignored and skipped. What the two arms do is described where they live. */
/* @implements 0x10021210 glide BrGbiDispatch10020F50 */
BrGfxWords *BrGbiDispatch10020F50(BrGfxWords *pCmd)
{
    int sel = ((int)pCmd->w0 << 16) >> 24;

    /* A SWITCH, not a chain of `if (sel == k) return f(pCmd);`. Both read the
     * same and both emit `je`, but the switch puts the default's `lea eax,
     * [ecx+8]; ret` between the tests and the two call arms, which is the
     * original's layout; the if-chain inlines each arm at its test and needs
     * a `jne` over it. The goto-and-labels spelling this replaced got the
     * arms to the bottom but not the default into the middle. */
    switch (sel) {
    case 0:
        return BrGbiCall100243D0(pCmd);
    case 3:
        return BrGbiSet4C1694(pCmd);
    default:
        return pCmd + 1;
    }
}

/* 0x100242F0  G_MOVEWORD.
 *
 * Jump table recovered from the DLL (byte table 0x100243C0, targets
 * 0x100243AC): index 0x02 -> 0x1002431A, 0x0A -> 0x1002432D; 0x08 and 0x0E
 * reach the table but land on the default arm; everything outside 0x02..0x0E
 * is rejected by the range check. The index is the SIGN-EXTENDED low byte of
 * w0, so 0x80..0xFF go to the default too. */
/* WHAT IT DOES: the drawing command that pokes a single word of renderer
 * data. Only two pokes are recognised: setting how many lights are active,
 * and rewriting the colour or the direction bytes of one particular light.
 * Everything else is skipped. */
/* @implements 0x100239C0 glide BrGbiMoveWord */
BrGfxWords *BrGbiMoveWord(BrGfxWords *pCmd)
{
    unsigned w0  = pCmd->w0;
    int      sel = ((int)w0 << 24) >> 24;
    unsigned off;
    unsigned slot;

    switch (sel) {
    case 2:
        DAT_105ccfd0 = (int)((pCmd->w1 >> 5) & 0xFu);
        return pCmd + 1;
    case 8:
        return pCmd + 1;
    case 0xA:
        off = (w0 >> 8) & 0xFFFFu;
        /* test al,0xf is on `off` BEFORE the scale.  The scale is a
         * MULTIPLY, not a shift: `slot <<= 4` lets VC5 fold the pair into
         * `shr 1; and 0x7FFFFFF0`, while `slot = slot * 16` leaves the
         * original's `shr 5; shl 4`.  Same value, and the only two bytes
         * that were wrong.  Indexing a 16-byte element type instead
         * (`arr[slot].b[0]`) is much worse -- it hoists the scale out of
         * the arms.  Do not re-probe those. */
        if ((off & 0xFu) == 0) {
            slot = off >> 5;
            slot = slot * 16;
            DAT_105ccc78[slot]     = (char)(pCmd->w1 >> 24);
            DAT_105ccc78[slot + 1] = (char)(pCmd->w1 >> 16);
            DAT_105ccc78[slot + 2] = (char)(pCmd->w1 >> 8);
        } else {
            slot = off >> 5;
            slot = slot * 16;
            DAT_105ccc78[slot + 4] = (char)(pCmd->w1 >> 24);
            DAT_105ccc78[slot + 5] = (char)(pCmd->w1 >> 16);
            DAT_105ccc78[slot + 6] = (char)(pCmd->w1 >> 8);
        }
        DAT_105d17d0 = 0;
        return pCmd + 1;
    case 0xE:
        return pCmd + 1;
    default:
        return pCmd + 1;
    }
}

/* 0x10024A90 */
/* WHAT IT DOES: the drawing-command reader itself. It reads the command's
 * leading byte, calls the handler registered for it, and continues from
 * wherever that handler says the next command is -- looping until a handler
 * reports there is nothing left. This one loop is what draws every frame of
 * the game. */
/* @implements 0x10024A90 d3d BrGbiRun */
/* @implements 0x10023C90 glide BrGbiRun */
/* Original is cdecl, one argument: the table is the global at 0x100A79F0
 * and the opcode is byte 3 of the command in host order. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
void BrGbiRun(BrGfxWords *pCmd)
{
    while (pCmd != NULL)
        pCmd = g_brGbi0A79F0[((unsigned char *)pCmd)[3]](pCmd);
}

/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include "slice5_61.h"

#include <string.h>

#include "slice1_03.h"   /* BrTextState / BrTextGetState, BR_TEXT_ALIGN_* */
#include "slice5_63.h"   /* g_br4B035C -- raw text-align global */
#include "slice2_15.h"   /* BrRdpRegs / BrRdpGetRegs -- 0x104BBF08 etc.   */
#include "slice2_25.h"   /* the option globals, index tables and callees   */


/* ==========================================================================
 * 0x10023920 (Glide) / 0x10024260 (D3D)  G_MOVEMEM 0x80 -- load viewport
 * ========================================================================== */

/* The original does `fild [slot] / fstp dword [slot] / fld dword [slot]`
 * around every conversion, i.e. it ROUNDS THE INTEGER TO FLOAT before the
 * multiply. Every value it converts is a sign-extended int16, which is exact
 * in a float, so the round trip is a no-op and is not reproduced literally.
 *
 * BUILD DIVERGENCE -- HOW Y IS FLIPPED, and the port had the D3D answer.
 *
 * The two builds differ in exactly two places, and BOTH are on the Y axis:
 *
 *   Y SCALE.  D3D 0x1002429D multiplies by [0x1008F3F0] == -0.25f; every
 *   other multiply in either build is [0x1008F3EC] / [0x1007740C] == +0.25f.
 *   Glide 0x1002396A uses +0.25f here too -- BRGlide's .rdata does not carry
 *   a -0.25f for this function at all.
 *
 *   Y TRANSLATE.  Glide opens with `fild dword [0x100A7518]` (0x10023925),
 *   parks it in a stack slot, and closes the fourth value with
 *   `fsubr dword [esp]` at 0x100239B0 -- `mem - ST0`, i.e.
 *
 *       vtransY = (float)screenHeight - 0.25f * v3
 *
 *   D3D 0x100242DD just stores 0.25f * v3 and has no such preamble; its
 *   whole prologue is two instructions where Glide's is five.
 *
 * So the two builds flip the vertical axis by DIFFERENT MEANS -- D3D negates
 * the scale and leaves the origin at the top, Glide keeps the scale positive
 * and reflects the translate through the screen height.  They are not two
 * spellings of one thing: taking D3D's negation without D3D's translate, or
 * vice versa, gives geometry that is upside down about the wrong axis.
 *
 * THE COMMENT WAS PART OF THE DEFECT.  This banner previously read "the
 * vertical scale comes out negated, because the drawing list counts down the
 * screen and the renderer counts up", stated as a fact about the game.  It
 * was a fact about BRD3D.dll, and it is the reason nobody re-derived the
 * line under it.  CONVENTIONS.md: derive expected values from the
 * disassembly, never from a comment.
 *
 * 0x100A7518 is Glide's grSstWinOpen height (480 in the shipped image, read
 * with `fild` because it is an int32).  config/globals_shared.csv pairs it
 * with D3D 0x100A81C4 at 9 votes, which is exactly BrScreenInfo::cy -- so the
 * port already models it and no new global is needed. */
/* WHAT IT DOES: handles the drawing command that loads the viewport -- the
 * mapping from the renderer's own coordinates onto the screen. It unpacks
 * four fixed-point numbers from the payload and scales them into the four
 * viewport values the projection uses. The vertical one is measured down from
 * the bottom of the screen rather than the top, which is how the drawing
 * list's top-down coordinates become the renderer's bottom-up ones. */
/* Orig filds 0x100A7518 and fstp's 0x105CCD48 / 0x105CD9F8 / 0x105CD9FC
 * as absolute globals, not through BrScreenGet / BrRdpGetRegs. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* WHAT IT DOES: handle the display-list command that sets the viewport:
 * reads the packed scale and offset values, converts them to floats, and
 * flips the vertical translate because the display list's Y axis runs the
 * other way from the screen's. */
/* @implements 0x10023920 glide BrGbiCall10024260 */
BrGfxWords *BrGbiCall10024260(BrGfxWords *pCmd)
{
    /* Orig: movsx from w1 as int16*, fild height first, /Op fstp;fld on
     * each i16->float, fsubr height for Y translate, add eax,8 early. */
    const int16_t *pVp;
    float          cyScreen;
    float         *pH;

    pVp = (const int16_t *)pCmd->w1;
    pCmd++;
    /* Address-taken so height occupies its own slot (`push ecx` / [esp])
     * and the last scale is `fsubr [esp]`, not `fsubp st(1)`. */
    pH = &cyScreen;
    *pH = (float)DAT_100a7518;
    DAT_105ccd48 = (float)pVp[0] * 0.25f;
    g_br4BC198   = (float)pVp[1] * 0.25f;
    DAT_105cd9f8 = (float)pVp[4] * 0.25f;
    DAT_105cd9fc = *pH - (float)pVp[5] * 0.25f;
    return pCmd;
}
