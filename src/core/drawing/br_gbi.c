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
#ifdef BR_MATCHING_BUILD
/* The original binary is /MD: CRT calls resolve through the import table. */
#define _CRTIMP __declspec(dllimport)
/* Header prototype is the port's (table, pCmd).  The original takes only
 * pCmd; the table is the global at 0x100A79F0.  Rename the port prototype
 * in this TU so the matching body can use the original shape. */
#define BrGbiRun BrGbiRun_port
/* OtherMode H/0E and TexCreate: orig takes no state pointer — those fields
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
#endif
#include "slice2_16.h"
#ifdef BR_MATCHING_BUILD
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
void BrFadeDrawSprite(const uint32_t *pRecs, float alpha);
#undef BrFadeDrawBars
void BrFadeDrawBars(void);
/* Bodies live in br_gbitexscan.c; TexScanRun still calls them. */
void BrGbiTexScanOtherModeH(const BrGfxWords *pCmd);
void BrGbiTexScanOtherModeH0E(const BrGfxWords *pCmd);
void BrGbiTexScanLoadTlut(const BrGfxWords *pCmd);
void BrGbiTexScanLoadBlock(const BrGfxWords *pCmd);
void BrGbiSolidTexBuild(void);
#include <stdlib.h>
BrGfxWords *BrGbiClearGeometryMode(BrGfxWords *pCmd);
BrGfxWords *BrGbiSetGeometryMode(BrGfxWords *pCmd);
BrGfxWords *BrGbiDList(BrGfxWords *pCmd);
BrGfxWords *BrGbiEndDList(void);
BrGfxWords *BrGbiMatrix(BrGfxWords *pCmd);
BrGfxWords *BrGbiPopMatrix(BrGfxWords *pCmd);
BrGfxWords *BrGbiDispatch10020F50(BrGfxWords *pCmd);
BrGfxWords *BrGbiMoveMem(BrGfxWords *pCmd);
BrGfxWords *BrGbiMoveWord(BrGfxWords *pCmd);
BrGfxWords *BrGbiMoveMemMatrix(BrGfxWords *pCmd);
extern int DAT_105d17c8;   /* geo.cur      */
extern int DAT_105d17cc;   /* geo.prev     */
extern int DAT_105ccfe8;   /* DL stack n   */
extern int DAT_105ce2e8[]; /* DL stack     */
extern int DAT_100a9a50;   /* mtx top      */
extern int DAT_105ccd00;   /* projection   */
extern int DAT_105ccd10;   /* modelview[0] */
extern int DAT_105d17d0;   /* mtx.f5180    */
extern int DAT_105d1760;   /* combined     */
extern int DAT_105ce2d8;   /* lookat 0x82  */
extern int DAT_105ce2dc;   /* lookat 0x84  */
extern char DAT_105ccc78[]; /* lights      */
extern int DAT_105ccfd0;   /* numLights    */
extern BrGfxWords *DAT_106e7710;  /* DL write cursor */
extern int         DAT_106ec798;  /* fade rectIdx    */
extern int         DAT_106e7718;  /* otherModeH      */
#endif

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
#ifdef BR_MATCHING_BUILD
BrGfxWords *BrGbiSet0A79E8(BrGfxWords *pCmd)
{
    g_brGbi0A79E8 = pCmd->w1;
    return pCmd + 1;
}
#else
BrGfxWords *BrGbiSet0A79E8(BrGbiState *pSt, BrGfxWords *pCmd)
{
    pSt->f0A79E8 = pCmd->w1;
    return pCmd + 1;
}
#endif

/* 0x1001CD80 */
/* WHAT IT DOES: another one-line drawing-command handler that parks the
 * command's payload in a graphics setting and moves on. As with its
 * neighbour, what the setting is used for is not established. */
/* @implements 0x1001CD80 d3d BrGbiSet4C5174 */
/* @implements 0x1001EB30 glide BrGbiSet4C5174 */
#ifdef BR_MATCHING_BUILD
BrGfxWords *BrGbiSet4C5174(BrGfxWords *pCmd)
{
    g_brGbi4C5174 = pCmd->w1;
    return pCmd + 1;
}
#else
BrGfxWords *BrGbiSet4C5174(BrGbiState *pSt, BrGfxWords *pCmd)
{
    pSt->f4C5174 = pCmd->w1;
    return pCmd + 1;
}
#endif

/* 0x1001E790 */
/* WHAT IT DOES: turns geometry features off. Drawing commands carry a set of
 * switches -- lighting, fog, backface culling and the like -- and this
 * clears the ones named in the command, remembering what they were before,
 * then tells the renderer the switches changed. */
/* @implements 0x1001FD40 glide BrGbiClearGeometryMode */
#ifdef BR_MATCHING_BUILD
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
#else
BrGfxWords *BrGbiClearGeometryMode(BrGbiState *pSt, BrGfxWords *pCmd)
{
    pSt->geo.prev = pSt->geo.cur;
    pSt->geo.cur  = pSt->geo.cur & ~pCmd->w1;
    BrGbiGeoModeChanged();
    return pCmd + 1;
}
#endif

/* 0x10020F20 */
/* WHAT IT DOES: turns geometry features on: the mirror image of the clear
 * above. It sets the switches named in the command, remembers the previous
 * setting, and notifies the renderer. */
/* @implements 0x100211E0 glide BrGbiSetGeometryMode */
#ifdef BR_MATCHING_BUILD
BrGfxWords *BrGbiSetGeometryMode(BrGfxWords *pCmd)
{
    /* In place on the global -- see BrGbiClearGeometryMode above. Here it is
     * also what folds the command word into the memory operand of the `or`. */
    DAT_105d17cc = DAT_105d17c8;
    DAT_105d17c8 |= (int)pCmd->w1;
    BrGbiGeoModeChanged();
    return pCmd + 1;
}
#else
BrGfxWords *BrGbiSetGeometryMode(BrGbiState *pSt, BrGfxWords *pCmd)
{
    pSt->geo.prev = pSt->geo.cur;
    pSt->geo.cur  = pSt->geo.cur | pCmd->w1;
    BrGbiGeoModeChanged();
    return pCmd + 1;
}
#endif

/* 0x10020D60 */
/* WHAT IT DOES: jumps the drawing-command reader into another list of
 * commands -- the equivalent of calling a subroutine while drawing. Unless
 * the command says otherwise it remembers where to come back to. The return-
 * address stack holds ten entries but the game complains one entry early,
 * and stores anyway. */
/* @implements 0x10021020 glide BrGbiDList */
#ifdef BR_MATCHING_BUILD
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
#else
BrGfxWords *BrGbiDList(BrGbiState *pSt, BrGfxWords *pCmd)
{
    BrGbiDLStack *p = &pSt->dl;

    if ((pCmd->w0 & 0x00FF0000u) == 0) {
        /* GOTCHA: the guard tests the value the counter is ABOUT to take and
         * then stores anyway, so slot 9 is written and reported both. */
        if (p->n + 1 == BR_GBI_DL_STACK_MAX)
            BrGbiStackOverflow(1);
        p->ap[p->n] = pCmd + 1;
        p->n += 1;
    }
    /* DEVIATION: 32-bit branch target reinterpreted as a pointer. */
    return (BrGfxWords *)(uintptr_t)pCmd->w1;
}
#endif

/* 0x10020DA0 -- takes no argument in the original. */
/* WHAT IT DOES: ends the current list of drawing commands and returns to
 * whoever jumped into it. If nothing jumped in, it reports that there is
 * nowhere to go back to, which is what stops the drawing-command reader
 * altogether. */
/* @implements 0x10021060 glide BrGbiEndDList */
#ifdef BR_MATCHING_BUILD
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
#else
BrGfxWords *BrGbiEndDList(BrGbiState *pSt)
{
    BrGbiDLStack *p = &pSt->dl;

    if (p->n == 0)
        return NULL;
    p->n -= 1;
    return p->ap[p->n];
}
#endif

/* 0x10020EF0 */
/* WHAT IT DOES: restores the previously saved model matrix, undoing one save
 * made by the matrix command above. If nothing was saved it does nothing. */
/* @implements 0x100211B0 glide BrGbiPopMatrix */
#ifdef BR_MATCHING_BUILD
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
#else
BrGfxWords *BrGbiPopMatrix(BrGbiState *pSt, BrGfxWords *pCmd)
{
    BrGbiMtxState *pM = &pSt->mtx;

    if (pM->top != 0) {
        pM->top -= 1;
        if (pM->top == 0)
            pM->top = 10;
    }
    return pCmd + 1;
}
#endif

/* 0x10020F80 */
/* WHAT IT DOES: handles a drawing command that parks the command's payload
 * in a graphics setting and then passes that same value to another routine
 * which acts on it. What the setting means is not established here. */
/* @implements 0x10020F80 d3d BrGbiSet4C1694 */
/* @implements 0x10021250 glide BrGbiSet4C1694 */
#ifdef BR_MATCHING_BUILD
BrGfxWords *BrGbiSet4C1694(BrGfxWords *pCmd)
{
    /* w1 named once: the original loads it a single time and reuses that
     * register for both the store and the call argument. */
    uint32_t w1 = pCmd->w1;

    g_brGbi4C1694 = w1;
    BrGbiCall10020FA0(w1);
    return pCmd + 1;
}
#else
BrGfxWords *BrGbiSet4C1694(BrGbiState *pSt, BrGfxWords *pCmd)
{
    pSt->f1694 = pCmd->w1;
    BrGbiCall10020FA0(pCmd->w1);
    return pCmd + 1;
}
#endif

/* 0x10020F50 */
/* WHAT IT DOES: a drawing command with a small selector byte in it: selector
 * 0 and selector 3 each go to a different handler, and anything else is
 * ignored and skipped. What the two arms do is described where they live. */
/* @implements 0x10021210 glide BrGbiDispatch10020F50 */
#ifdef BR_MATCHING_BUILD
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
#else
BrGfxWords *BrGbiDispatch10020F50(BrGbiState *pSt, BrGfxWords *pCmd)
{
    int sel = (int8_t)((pCmd->w0 >> 16) & 0xFFu);

    if (sel == 0)
        return BrGbiCall100243D0(pCmd);
    if (sel == 3)
        return BrGbiSet4C1694(pSt, pCmd);
    return pCmd + 1;
}
#endif

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
#ifdef BR_MATCHING_BUILD
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
#else
BrGfxWords *BrGbiMoveWord(BrGbiState *pSt, BrGfxWords *pCmd)
{
    int      sel = (int8_t)(pCmd->w0 & 0xFFu);
    uint32_t off;
    size_t   slot;
    uint8_t *p;

    if ((uint32_t)(sel - 2) > 0xCu)
        return pCmd + 1;

    if (sel == 0x02) {
        pSt->light.numLights = (int32_t)((pCmd->w1 >> 5) & 0xFu);
        return pCmd + 1;
    }
    if (sel != 0x0A)
        return pCmd + 1;

    off  = (pCmd->w0 >> 8) & 0xFFFFu;
    slot = (size_t)(off >> 5);
    /* DEVIATION: `off` is a full 16 bits and the original scales it straight
     * into the light array with no bound. Out-of-range slots are dropped. */
    if (slot >= BR_GBI_LIGHT_SLOTS)
        return pCmd + 1;

    p = &pSt->lights.aRaw[slot * BR_GBI_LIGHT_SIZE];
    /* Low nibble 0 writes the record's first three bytes, anything else its
     * bytes 4..6. The original tests `off & 0xF` -- not `== 4`. */
    if ((off & 0xFu) == 0) {
        p[0] = (uint8_t)(pCmd->w1 >> 24);
        p[1] = (uint8_t)(pCmd->w1 >> 16);
        p[2] = (uint8_t)(pCmd->w1 >> 8);
    } else {
        p[4] = (uint8_t)(pCmd->w1 >> 24);
        p[5] = (uint8_t)(pCmd->w1 >> 16);
        p[6] = (uint8_t)(pCmd->w1 >> 8);
    }
    pSt->mtx.f5180 = 0;
    return pCmd + 1;
}
#endif

/* 0x10024A90 */
/* WHAT IT DOES: the drawing-command reader itself. It reads the command's
 * leading byte, calls the handler registered for it, and continues from
 * wherever that handler says the next command is -- looping until a handler
 * reports there is nothing left. This one loop is what draws every frame of
 * the game. */
/* @implements 0x10024A90 d3d BrGbiRun */
/* @implements 0x10023C90 glide BrGbiRun */
#ifdef BR_MATCHING_BUILD
/* Original is cdecl, one argument: the table is the global at 0x100A79F0
 * and the opcode is byte 3 of the command in host order. */
extern BrGbiHandler g_brGbi0A79F0[];
void BrGbiRun(BrGfxWords *pCmd)
{
    while (pCmd != NULL)
        pCmd = g_brGbi0A79F0[((unsigned char *)pCmd)[3]](pCmd);
}
#else
void BrGbiRun(const BrGbiHandler *apTable, BrGfxWords *pCmd)
{
    while (pCmd != NULL)
        pCmd = apTable[(pCmd->w0 >> 24) & 0xFFu](pCmd);
}
#endif
