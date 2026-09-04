/* slice2_16.c -- Boss Rally (BRD3D.dll), a later pass, 0x1001CD60..0x1002BC90.
 * See slice2_16.h for the per-function notes and gotchas. */

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

/* ------------------------------------------------------------------ */
/* Local helpers                                                      */
/* ------------------------------------------------------------------ */

/* 0x1007C8A0 __ftol: truncate toward zero, return the low dword of the
 * 64-bit result. DEVIATION: inputs outside the 64-bit range are undefined in
 * C, so they are turned into 0 rather than whatever the x87 indefinite
 * value would truncate to. */
static int32_t br16_ftol(double x)
{
    int64_t v;

    if (!(x > -9.2233720368547758e18 && x < 9.2233720368547758e18))
        return 0;
    v = (int64_t)x;
    return (int32_t)(uint32_t)((uint64_t)v & 0xFFFFFFFFu);
}

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

static uint32_t br16_bswap32(uint32_t v)
{
    return ((v & 0x000000FFu) << 24) | ((v & 0x0000FF00u) << 8) |
           ((v & 0x00FF0000u) >> 8)  | ((v & 0xFF000000u) >> 24);
}

/* Byte-swap the u32 that starts at p, byte-wise so the host's own endianness
 * never enters into it. */
static void br16_swap_u32_at(uint8_t *p)
{
    uint8_t t;
    t = p[0]; p[0] = p[3]; p[3] = t;
    t = p[1]; p[1] = p[2]; p[2] = t;
}

static void br16_swap_u16_at(uint8_t *p)
{
    uint8_t t = p[0];
    p[0] = p[1];
    p[1] = t;
}

/* Read/write a u32 byte-wise in the host's order. The .rca payload has
 * already been swapped by the time these are read back. */
static uint32_t br16_ld32(const uint8_t *p)
{
    uint32_t v;
    memcpy(&v, p, sizeof v);
    return v;
}

static void br16_st32(uint8_t *p, uint32_t v)
{
    memcpy(p, &v, sizeof v);
}

static uint16_t br16_ld16(const uint8_t *p)
{
    uint16_t v;
    memcpy(&v, p, sizeof v);
    return v;
}

/* Advance the fade emitter's cursor by one command and hand back the slot
 * that was reserved. Every emit site in 0x1002AF10 / 0x1002B340 is this. */
static BrGfxWords *br16_fade_alloc(BrFadeState *pSt)
{
    BrGfxWords *p = pSt->pCmd;
    pSt->pCmd = p + 1;
    return p;
}

/* ================================================================== */
/* 1. F3D command handlers                                            */
/* ================================================================== */

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
BrGfxWords *BrGbiSetTileSize(BrGbiState *pSt, BrGfxWords *pCmd)
{
    BrDlsTileSize t;

    BrDlsTileSizeDecode(pCmd->w0, pCmd->w1, &t);
    pSt->tile.uls   = t.uls;
    pSt->tile.ult   = t.ult;
    pSt->tile.lrs   = t.lrs;
    pSt->tile.lrt   = t.lrt;
    pSt->tile.tileW = t.tileW;
    pSt->tile.tileH = t.tileH;
    return pCmd + 1;
}

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

BrMat4 *BrGbiMtxProj(BrGbiMtxState *pSt)
{
    return (BrMat4 *)(void *)&pSt->aWords[0];
}

/* @n64 0x80242810 located */
BrMat4 *BrGbiMtxSlot(BrGbiMtxState *pSt, int index)
{
    return (BrMat4 *)(void *)&pSt->aWords[BR_GBI_MTX_STACK_OFF + index * 16];
}

/* The ring push used on both modelview paths: 10 wraps to 0 before the
 * increment, so `top` only ever takes 1..10. */
static void br16_mtx_push(BrGbiMtxState *pSt)
{
    if (pSt->top == 10)
        pSt->top = 0;
    pSt->top += 1;
}

/* top == 0 means "no modelview matrix"; the two sites that build a pointer
 * from it substitute NULL. */
static BrMat4 *br16_mtx_current(BrGbiMtxState *pSt)
{
    if (pSt->top == 0)
        return NULL;
    return BrGbiMtxSlot(pSt, pSt->top);
}

/* 0x10020DC0 */
/* WHAT IT DOES: installs a transform matrix -- either the camera's
 * projection, or a model's position and orientation. It can replace the
 * current matrix or combine with it, and optionally save the old one so it
 * can be restored later. It always finishes by recomputing the single
 * combined matrix the renderer actually uses. */
/* @implements 0x10021080 glide BrGbiMatrix */
#ifdef BR_MATCHING_BUILD
BrGfxWords *BrGbiMatrix(BrGfxWords *pCmd)
{
    unsigned  w0 = pCmd->w0;
    void     *pIn = (void *)(uintptr_t)pCmd->w1;
    int       top;
    int       z = 0;
    void     *cur;
    BrMat4    tmp;

    if ((w0 & 0x10000u) != 0) {
        if ((w0 & 0x20000u) != 0)
            memcpy(&DAT_105ccd00, pIn, 64);
        else
            BrMat4Mul(pIn, &DAT_105ccd00, &DAT_105ccd00);
    } else if ((w0 & 0x20000u) != 0) {
        top = DAT_100a9a50;
        if ((w0 & 0x40000u) != 0) {
            if (top == 10)
                top = z;
            top += 1;
            DAT_100a9a50 = top;
        }
        memcpy((char *)&DAT_105ccd10 + (top << 6), pIn, 64);
        DAT_105d17d0 = z;
    } else {
        top = DAT_100a9a50;
        if (top == z)
            cur = (void *)z;
        else
            cur = (char *)&DAT_105ccd10 + (top << 6);
        BrMat4Mul(pIn, cur, &tmp);
        w0 = pCmd->w0;
        if ((w0 & 0x40000u) != 0) {
            top = DAT_100a9a50;
            if (top == 10)
                top = z;
            top += 1;
            DAT_100a9a50 = top;
        } else {
            top = DAT_100a9a50;
        }
        memcpy((char *)&DAT_105ccd10 + (top << 6), &tmp, 64);
        DAT_105d17d0 = z;
    }

    top = DAT_100a9a50;
    if (top == z)
        cur = (void *)z;
    else
        cur = (char *)&DAT_105ccd10 + (top << 6);
    BrMat4Mul(cur, &DAT_105ccd00, &DAT_105d1760);
    return pCmd + 1;
}
#else
BrGfxWords *BrGbiMatrix(BrGbiState *pSt, BrGfxWords *pCmd, const BrMat4 *pIn)
{
    BrGbiMtxState *pM = &pSt->mtx;
    uint32_t       w0 = pCmd->w0;

    if ((w0 & 0x10000u) != 0) {
        /* projection: G_MTX_PUSH is ignored on this path. */
        if ((w0 & 0x20000u) != 0)
            memcpy(BrGbiMtxProj(pM), pIn, 16 * sizeof(float));
        else
            BrMat4Mul(pIn, BrGbiMtxProj(pM), BrGbiMtxProj(pM));
    } else if ((w0 & 0x20000u) != 0) {
        /* modelview load */
        if ((w0 & 0x40000u) != 0)
            br16_mtx_push(pM);
        memcpy(BrGbiMtxSlot(pM, pM->top), pIn, 16 * sizeof(float));
        pM->f5180 = 0;
    } else {
        /* modelview multiply: tmp = new * current, then store. */
        BrMat4 tmp;
        BrMat4Mul(pIn, br16_mtx_current(pM), &tmp);
        if ((w0 & 0x40000u) != 0)
            br16_mtx_push(pM);
        memcpy(BrGbiMtxSlot(pM, pM->top), &tmp, 16 * sizeof(float));
        pM->f5180 = 0;
    }

    BrMat4Mul(br16_mtx_current(pM), BrGbiMtxProj(pM), &pM->combined);
    return pCmd + 1;
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

/* 0x10021510 */
/* WHAT IT DOES: draws a textured rectangle straight onto the screen -- the
 * command used for things like heads-up display panels rather than for 3D
 * geometry. It unpacks the corners and the tile number and hands them to the
 * rectangle drawer. Note it swallows three commands, not one, because the
 * rectangle's texture coordinates follow it in the list. */
/* The decode is br_dlshared.c's, which carries this address and BRGlide's
 * 0x10021570. */
BrGfxWords *BrGbiTileRect(BrGbiState *pSt, BrGfxWords *pCmd)
{
    BrDlsTileRect r;

    (void)pSt;
    BrDlsTileRectDecode(pCmd->w0, pCmd->w1, 0, &r);
    BrGbiCall10021560(r.ulx, r.uly, r.lrx, r.lry, r.tile);
    /* GOTCHA: three commands consumed, not one. */
    return pCmd + 3;
}

/* 0x10021B80 */
/* WHAT IT DOES: the same textured-rectangle draw as above but with the
 * corners given in whole screen pixels rather than quarter-pixels, so it
 * scales them up before handing them on, and it consumes only its own
 * command. */
/* Likewise 0x10021B80 / BRGlide 0x100219D0. */
BrGfxWords *BrGbiTileRectS(BrGbiState *pSt, BrGfxWords *pCmd)
{
    BrDlsTileRect r;

    (void)pSt;
    BrDlsTileRectDecode(pCmd->w0, pCmd->w1, 1, &r);
    BrGbiCall10021560(r.ulx, r.uly, r.lrx, r.lry, r.tile);
    return pCmd + 1;
}

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
void BrGbiLightVertex(const BrGbiLightState *pSt, const float *pSrc, float *pDst)
{
    float t;
    int   i;

    if (pSt->numLights == 0) {
        pDst[7] = pSt->off[0];
        pDst[8] = pSt->off[1];
        pDst[9] = pSt->off[2];
        return;
    }

    /* Add order as emitted: (src[5]*dir[0] + src[6]*dir[1]) + src[7]*dir[2],
     * where src[5] is the float at +0x14. */
    t = (pSrc[5] * pSt->dir[0] + pSrc[6] * pSt->dir[1]) + pSrc[7] * pSt->dir[2];

    /* 0x10022375: `fcomp [0x1008F3C8] / fnstsw ax / test ah,1 / jne` --
     * 0x1008F3C8 is 0.0f and bit 0 of ah is C0, which an UNORDERED compare
     * sets as well.  So a NaN dot takes the ambient-only arm.
     *
     * THIS WAS `t < 0.0f`, WHICH IS FALSE FOR NaN and sent an unordered dot
     * down the lit path instead. Same defect as 0x10022DC0's, in the same
     * file, found the same way -- see the note there. */
    if (!(t >= 0.0f)) {
        pDst[7] = pSt->ambient[0];
        pDst[8] = pSt->ambient[1];
        pDst[9] = pSt->ambient[2];
        return;
    }

    for (i = 0; i < 3; ++i) {
        float v = t * pSt->scale[i] + pSt->ambient[i];
        /* 0x100223A4 and its two repeats: `fcomp [0x1008F3C4] / test ah,0x41
         * / mov eax,0x3F800000 / je`.  The ceiling is substituted only when
         * the test is ZERO, i.e. C0 and C3 both clear, i.e. an ORDERED
         * GREATER-THAN.  An unordered compare sets both, so a NaN is NOT
         * clamped and passes through.
         *
         * THIS WAS `(v <= 1.0f) ? v : 1.0f`, which is false for NaN and
         * therefore clamped it to 1.0f. The positive form is the faithful one
         * here for the same reason it is in br_dl.c's copy: C's `v > 1.0f` is
         * also false for NaN.
         *
         * The literal substituted is 0x3F800000, and the limit compared
         * against at 0x1008F3C4 is also 1.0f. NOTE THAT BRGLIDE'S TWIN OF
         * THIS FUNCTION USES 255.0f FOR BOTH (0x10022B35 `mov eax,0x437F0000`
         * against 0x10077418 == 255.0f) -- these two are NOT the same
         * function, whatever shared.csv's `shape` match suggests. See the
         * banner above. */
        pDst[7 + i] = (v > 1.0f) ? 1.0f : v;
    }
}

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
int BrGbiClipCodes(const float *pVert)
{
    return (int)BrDlsClipCodes(pVert[1], pVert[2], pVert[3], pVert[6]);
}

/* 0x10024240 */
/* WHAT IT DOES: handles the drawing command that hands the renderer a ready-
 * made combined transform matrix outright, replacing whatever the matrix
 * commands had built up. */
/* port-only body; Glide match is src/core/generated/0x10023900.c */
#ifndef BR_MATCHING_BUILD
BrGfxWords *BrGbiMoveMemMatrix(BrGbiState *pSt, BrGfxWords *pCmd,
                               const void *pSrc)
{
    memcpy(&pSt->mtx.combined, pSrc, 16 * sizeof(float));
    return pCmd + 1;
}
#endif

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
#ifdef BR_MATCHING_BUILD
BrGfxWords *BrGbiMoveMem(BrGfxWords *pCmd)
{
    unsigned w0  = pCmd->w0;
    unsigned idx = (w0 >> 16) & 0xFFu;
    unsigned len;
    unsigned slot;

    switch (idx) {
    case 0x80:
        return BrGbiCall10024260(pCmd);
    case 0x82:
        DAT_105ce2d8 = (int)pCmd->w1;
        return pCmd + 1;
    case 0x84:
        DAT_105ce2dc = (int)pCmd->w1;
        return pCmd + 1;
    case 0x86:
    case 0x88:
    case 0x8A:
    case 0x8C:
    case 0x8E:
    case 0x90:
    case 0x92:
    case 0x94:
        /* idx extract is shr/and on a copy of w0 (esi stays w0 until
         * memcpy reuses it as the source pointer).  Length is w0's low
         * 16; dest is 0x105CCC78 + ((idx-0x86)>>1)*16. */
        len  = w0 & 0xFFFFu;
        slot = ((idx - 0x86u) >> 1) << 4;
        memcpy(DAT_105ccc78 + slot, (void *)(uintptr_t)pCmd->w1, len);
        DAT_105d17d0 = 0;
        return pCmd + 1;
    case 0x9E:
        return BrGbiMoveMemMatrix(pCmd);
    default:
        return pCmd + 1;
    }
}
#else
BrGfxWords *BrGbiMoveMem(BrGbiState *pSt, BrGfxWords *pCmd, const void *pSrc)
{
    uint32_t idx = (pCmd->w0 >> 16) & 0xFFu;

    if (idx < 0x80u || idx > 0x9Eu)
        return pCmd + 1;

    switch (idx) {
    case 0x80:
        return BrGbiCall10024260(pCmd);
    case 0x82:
        pSt->f1698 = pCmd->w1;
        return pCmd + 1;
    case 0x84:
        pSt->f169C = pCmd->w1;
        return pCmd + 1;
    case 0x9E:
        return BrGbiMoveMemMatrix(pSt, pCmd, pSrc);
    case 0x86: case 0x88: case 0x8A: case 0x8C:
    case 0x8E: case 0x90: case 0x92: case 0x94: {
        /* dst = 0x104BBE38 + ((idx - 0x86) >> 1) * 16, length = w0 & 0xFFFF.
         * DEVIATION: the length comes straight out of the command and the
         * original does not check it; it is clamped to the record here so a
         * malformed list cannot walk off the light array. */
        size_t   slot = (size_t)((idx - 0x86u) >> 1);
        size_t   len  = (size_t)(pCmd->w0 & 0xFFFFu);
        size_t   room = sizeof pSt->lights.aRaw - slot * BR_GBI_LIGHT_SIZE;
        if (len > room)
            len = room;
        memcpy(&pSt->lights.aRaw[slot * BR_GBI_LIGHT_SIZE], pSrc, len);
        pSt->mtx.f5180 = 0;
        return pCmd + 1;
    }
    default:
        return pCmd + 1;
    }
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

/* ================================================================== */
/* 2. Texture-load scanning pass                                      */
/* ================================================================== */

const void *BrGbiTexScanData(BrGbiTexScan *pSt, uint32_t addr)
{
    if (pSt->pfnData != NULL)
        return pSt->pfnData(pSt->pUser, addr);
    return (const void *)(uintptr_t)addr;
}

/* 0x10029E80  G_TEXTURE */
#ifdef BR_MATCHING_BUILD
void BrGbiTexScanTexture(const BrGfxWords *pCmd)
{
    g_brTexScan5553E8 = (int32_t)((pCmd->w0 >> 8)  & 7u);
    g_brTexScan5553E0 = (int32_t)((pCmd->w0 >> 11) & 7u);
}
#else
void BrGbiTexScanTexture(BrGbiTexScan *pSt, const BrGfxWords *pCmd)
{
    pSt->f5553E8 = (int32_t)((pCmd->w0 >> 8)  & 7u);
    pSt->f5553E0 = (int32_t)((pCmd->w0 >> 11) & 7u);
}
#endif

/* 0x10029EB0  G_SETTIMG */
/* WHAT IT DOES: during the texture-load hunt, notes the address and pixel
 * size of the image a load is about to read from, and -- if a run was not
 * already in progress -- marks this command as where the run begins. */
/* @implements 0x10029EB0 d3d BrGbiTexScanSetImg */
/* @implements 0x10029420 glide BrGbiTexScanSetImg */
#ifdef BR_MATCHING_BUILD
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
#else
void BrGbiTexScanSetImg(BrGbiTexScan *pSt, BrGfxWords *pCmd)
{
    int32_t s = pSt->state;

    if (s != 0 && s != 3 && s != 6)
        return;

    pSt->timgSiz  = (int32_t)((pCmd->w0 >> 19) & 3u);
    pSt->timgAddr = pCmd->w1;
    pSt->srcSeen  = 0;
    if (s == 0) {
        pSt->pRunStart = pCmd;
        pSt->pRunEnd   = NULL;
    }
    pSt->state = 1;
}
#endif

/* 0x10029F10  G_LOADTLUT */
/* WHAT IT DOES: during the texture-load hunt, copies a colour palette out of
 * the source image into the palette buffer. The number of bytes comes
 * straight from the command and is not checked, here or in the original. */
/* @implements 0x10029F10 d3d BrGbiTexScanLoadTlut */
/* @implements 0x10029480 glide BrGbiTexScanLoadTlut */
#ifdef BR_MATCHING_BUILD
extern uint8_t *DAT_100a9e58;          /* tlut dest, 0x100A9E58 */
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
#else
void BrGbiTexScanLoadTlut(BrGbiTexScan *pSt, const BrGfxWords *pCmd,
                          const void *pSrc)
{
    int32_t  ds, dt;
    uint32_t len;

    if (pSt->state != 1)
        return;

    ds = (int32_t)(pCmd->w1 & 0xFFFu) - (int32_t)(pCmd->w0 & 0xFFFu);
    dt = (int32_t)((pCmd->w1 >> 12) & 0xFFFu) -
         (int32_t)((pCmd->w0 >> 12) & 0xFFFu);
    len = (uint32_t)((ds + 1) * (dt + 1)) << 1;

    pSt->srcSeen = pSt->timgAddr;
    /* DEVIATION: the destination is a caller-owned buffer and the length is
     * data-driven; the original does not check it and neither does this. */
    memcpy(pSt->pTlutDst, pSrc, len);
    pSt->state = 7;
}
#endif

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
/* @implements 0x10029510 glide BrGbiTexScanLoadBlock */
#ifdef BR_MATCHING_BUILD
extern uint32_t DAT_105d17f0;          /* stageSrc, 0x105D17F0 */
extern int32_t  DAT_10697a54;          /* stageLen, 0x10697A54 */
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
#else
void BrGbiTexScanLoadBlock(BrGbiTexScan *pSt, const BrGfxWords *pCmd,
                           const void *pSrc)
{
    int32_t  d;
    uint32_t len, copy;

    if (pSt->state != 2)
        return;

    d = (int32_t)((pCmd->w1 >> 12) & 0xFFFu) -
        (int32_t)((pCmd->w0 >> 12) & 0xFFFu);
    len = (uint32_t)(d + d + 2);

    pSt->stageSrc = pSt->timgAddr;
    pSt->stageLen = len;              /* the unclamped request, as published */

    /* DEVIATION: clamped to the staging buffer. */
    copy = len;
    if (copy > (uint32_t)BR_GBI_STAGE_SIZE)
        copy = (uint32_t)BR_GBI_STAGE_SIZE;
    memcpy(pSt->aStage, pSrc, copy);
    pSt->state = 3;
}
#endif

/* 0x1002A040  G_SETTILE */
/* WHAT IT DOES: during the texture-load hunt, records everything one of the
 * eight texture slots is being configured with -- pixel format and size,
 * where it sits in texture memory, and how it wraps, mirrors or clamps in
 * each direction -- and notes the highest slot used. */
/* @implements 0x1002A040 d3d BrGbiTexScanSetTile */
/* @implements 0x100295B0 glide BrGbiTexScanSetTile */
#ifdef BR_MATCHING_BUILD
void BrGbiTexScanSetTile(const BrGfxWords *pCmd)
{
    int32_t tile = (int32_t)((pCmd->w1 >> 24) & 7u);

    /* Re-read w0/w1 each field — orig keeps pCmd in ecx and reloads. */
    g_brTexScanTiles[tile].fmt     = (int32_t)((pCmd->w0 >> 21) & 7u);
    g_brTexScanTiles[tile].siz     = (int32_t)((pCmd->w0 >> 19) & 3u);
    g_brTexScanTiles[tile].line    = (int32_t)(((pCmd->w0 >> 9) & 0x1FFu) << 3);
    g_brTexScanTiles[tile].tmem    = (int32_t)(pCmd->w0 & 0x1FFu);
    g_brTexScanTiles[tile].mirrorS = (int32_t)((pCmd->w1 >> 8)  & 1u);
    g_brTexScanTiles[tile].clampS  = (int32_t)((pCmd->w1 >> 9)  & 1u);
    g_brTexScanTiles[tile].mirrorT = (int32_t)((pCmd->w1 >> 18) & 1u);
    g_brTexScanTiles[tile].clampT  = (int32_t)((pCmd->w1 >> 19) & 1u);
    g_brTexScanTiles[tile].maskS   = (int32_t)((pCmd->w1 >> 4)  & 0xFu);
    g_brTexScanTiles[tile].maskT   = (int32_t)((pCmd->w1 >> 14) & 0xFu);
    g_brTexScanTiles[tile].shiftS  = (int32_t)(pCmd->w1 & 0xFu);
    g_brTexScanTiles[tile].shiftT  = (int32_t)((pCmd->w1 >> 10) & 0xFu);

    if (g_brTexScanState == 3 || g_brTexScanState == 4 ||
        g_brTexScanState == 7 || tile > g_brTexScanMaxTile)
        g_brTexScanMaxTile = tile;
    g_brTexScanState = 5;
}
#else
void BrGbiTexScanSetTile(BrGbiTexScan *pSt, const BrGfxWords *pCmd)
{
    uint32_t   w0 = pCmd->w0;
    uint32_t   w1 = pCmd->w1;
    int32_t    tile = (int32_t)((w1 >> 24) & 7u);
    BrGbiTile *p = &pSt->aTiles[tile];

    p->fmt     = (int32_t)((w0 >> 21) & 7u);
    p->siz     = (int32_t)((w0 >> 19) & 3u);
    p->line    = (int32_t)(((w0 >> 9) & 0x1FFu) << 3);
    p->tmem    = (int32_t)(w0 & 0x1FFu);
    p->mirrorS = (int32_t)((w1 >> 8)  & 1u);
    p->clampS  = (int32_t)((w1 >> 9)  & 1u);
    p->mirrorT = (int32_t)((w1 >> 18) & 1u);
    p->clampT  = (int32_t)((w1 >> 19) & 1u);
    p->maskS   = (int32_t)((w1 >> 4)  & 0xFu);
    p->maskT   = (int32_t)((w1 >> 14) & 0xFu);
    p->shiftS  = (int32_t)(w1 & 0xFu);
    p->shiftT  = (int32_t)((w1 >> 10) & 0xFu);

    /* maxTile is forced (not maximised) when a load is in flight. */
    if (pSt->state == 3 || pSt->state == 4 || pSt->state == 7 ||
        tile > pSt->maxTile)
        pSt->maxTile = tile;
    pSt->state = 5;
}
#endif

/* 0x1002A140  G_SETTILESIZE */
/* WHAT IT DOES: during the texture-load hunt, records which rectangle of the
 * image one of the eight texture slots covers. */
/* @implements 0x1002A140 d3d BrGbiTexScanSetTileSize */
/* @implements 0x100296B0 glide BrGbiTexScanSetTileSize */
#ifdef BR_MATCHING_BUILD
void BrGbiTexScanSetTileSize(const BrGfxWords *pCmd)
{
    int32_t tile = (int32_t)((pCmd->w1 >> 24) & 7u);

    g_brTexScanTiles[tile].uls = (int32_t)((pCmd->w0 >> 12) & 0xFFFu);
    g_brTexScanTiles[tile].ult = (int32_t)(pCmd->w0 & 0xFFFu);
    g_brTexScanTiles[tile].lrs = (int32_t)((pCmd->w1 >> 12) & 0xFFFu);
    g_brTexScanTiles[tile].lrt = (int32_t)(pCmd->w1 & 0xFFFu);
    /* LAST in the source, though the bytes put it between the lrs and lrt
     * stores: VC5 sinks the global store one tile store, so writing it
     * where the bytes show it lands one store too EARLY.  The `volatile`
     * that used to pin it here did not help -- /O2 reorders the ordinary
     * stores around a volatile one just the same. */
    g_brTexScanState = 6;
}
#else
void BrGbiTexScanSetTileSize(BrGbiTexScan *pSt, const BrGfxWords *pCmd)
{
    uint32_t   w0 = pCmd->w0;
    uint32_t   w1 = pCmd->w1;
    BrGbiTile *p = &pSt->aTiles[(w1 >> 24) & 7u];

    p->uls = (int32_t)((w0 >> 12) & 0xFFFu);
    p->ult = (int32_t)(w0 & 0xFFFu);
    pSt->state = 6;
    p->lrs = (int32_t)((w1 >> 12) & 0xFFFu);
    p->lrt = (int32_t)(w1 & 0xFFFu);
}
#endif

/* 0x1002A1A0  G_SETOTHERMODE_L */
/* WHAT IT DOES: during the texture-load hunt, watches for changes to the
 * blending mode and works out whether the material being set up is one that
 * needs special handling. A handful of specific blend settings, and anything
 * without two particular bits set, turn the flag off. */
/* @implements 0x1002A1A0 d3d BrGbiTexScanOtherModeL */
/* @implements 0x10029710 glide BrGbiTexScanOtherModeL */
#ifdef BR_MATCHING_BUILD
void BrGbiTexScanOtherModeL(const BrGfxWords *pCmd)
{
    uint32_t v;
    uint32_t z;

    if ((pCmd->w0 & 0xFF00u) != 0x300u)
        return;

    v = pCmd->w1;
    /* PARKED 2026-09-03, and the residue is ONE compiler decision, not a
     * source shape. Recomp 76 B / 22 insns against 107 / 29. The original
     * emits the three `mov [g],0; ret` blocks IN FULL, each with its own
     * `jne` skipping eleven bytes; VC5 cross-jumps ours into a single shared
     * block. That accounts for 33 of the 31 missing bytes on its own, and the
     * `xor ecx,ecx; cmp eax,ecx` / `mov [g],ecx` at the tail is a CONSEQUENCE
     * of the non-merge (with four zero stores alive VC5 puts the zero in a
     * register; with one it uses an immediate and `test`), not a second clue.
     *
     * DEAD PROBES, byte-identical output, do not re-run:
     *   - early-return per arm (below) and the nested single-exit
     *     if/else-if/else chain -- VC5 canonicalises the two.
     *   - a named zero local shared by the last compare and its store; VC5
     *     folds it to an immediate as long as the merge stands.
     * Nothing written in C stops the merge, because the three blocks really
     * are byte-identical -- that is exactly what cross-jumping looks for.
     * NARROWED 2026-09-03, second pass. The merge is NOT a flag: a standalone
     * harness of this exact shape gives 2 exits / 80 B under /O2, /O1, /Ox,
     * /O2 /Op, /O2 /Oy-, an explicit /Ot /Og /Oi /Oy /Ob1 /Gs and /Os /Og
     * alike. Four source shapes were probed in that harness and ALL give the
     * same 2 exits / 80 B: early-return per arm; the negated tail
     * (`v == 0 || no bits`) instead of the positive one; the fully nested
     * `if (v != K) { … }` chain; and a `switch` over the three constants.
     *
     * ‼ AND THE LEVER THAT SOLVES THE SAME SYMPTOM ELSEWHERE DOES NOT WORK
     * HERE. 0x10060CC0 BrCarPredictRemote had exactly this defect -- VC5
     * merging identical exits -- and writing its LAST test in positive form
     * (`if (ok) { work; return 1; }` then `return 0;`) split all four exits
     * apart and made it byte-exact. Applying the same flip here moves
     * nothing. The difference is what the merged blocks produce: there they
     * set a RETURN VALUE, here they STORE TO A GLOBAL and the function is
     * void. Treat the polarity lever as value-return-specific until a second
     * void case says otherwise.
     *
     * So the next idea has to be a mechanism outside these axes -- a compiler
     * patch level, or evidence that these handlers came from an original TU
     * built differently. Not another permutation. */
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
    z = 0;
    if (v == z || (v & 0x1800u) == 0) {
        g_brTexScan575414 = (int32_t)z;
        return;
    }
    g_brTexScan575414 = (int32_t)((v >> 16) & 1u);
}
#else
void BrGbiTexScanOtherModeL(BrGbiTexScan *pSt, const BrGfxWords *pCmd)
{
    uint32_t v;

    if ((pCmd->w0 & 0xFF00u) != 0x300u)
        return;

    v = pCmd->w1;
    if (v == 0x504F50u || v == 0xC184240u || v == 0x504240u || v == 0) {
        pSt->f575414 = 0;
        return;
    }
    /* `test ah,0x18` -- bits 11 and 12 of w1. */
    if ((v & 0x1800u) == 0) {
        pSt->f575414 = 0;
        return;
    }
    pSt->f575414 = (int32_t)((v >> 16) & 1u);
}
#endif

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
/* @implements 0x100290E0 d3d BrGbiTexScanRun */
/* @implements 0x10028820 glide BrGbiTexScanRun */
void BrGbiTexScanRun(BrGbiTexScan *pSt, BrGfxWords *pCmd)
{
    if (pCmd == NULL)
        return;

    pSt->f5544C    = 0;
    pSt->maxTile   = 0;
    pSt->f575448   = 0;
    pSt->state     = 0;
    pSt->pRunEnd   = NULL;

    for (;;) {
        uint32_t op = (pCmd->w0 >> 24) & 0xFFu;

        switch (op) {
        case 0xB8:                       /* G_ENDDL */
            return;

        case 0x04: case 0xB1: case 0xBF: /* G_VTX, G_TRI2, G_TRI1 */
            BrGbiTexScanFlush(pSt, pCmd);
            break;
        case 0xB9:                       /* G_SETOTHERMODE_L */
            BrGbiTexScanOtherModeL(pSt, pCmd);
            BrGbiTexScanMark(pSt, pCmd);
            break;
        case 0xBA:
#ifdef BR_MATCHING_BUILD
            BrGbiTexScanOtherModeH(pCmd);
#else
            BrGbiTexScanOtherModeH(pSt, pCmd);
#endif
            break;
        case 0xBB:
            BrGbiTexScanTexture(pSt, pCmd);
            break;
        case 0xE6:
            BrGbiTexScanLoadSync(pSt);
            break;
        case 0xE7:
            BrGbiTexScanPipeSync(pSt);
            break;
        case 0xE8:
            BrGbiTexScanTileSync(pSt);
            break;
        case 0xF0:
            BrGbiTexScanLoadTlut(pSt, pCmd,
                                 BrGbiTexScanData(pSt, pSt->timgAddr));
            break;
        case 0xF2:
            BrGbiTexScanSetTileSize(pSt, pCmd);
            break;
        case 0xF3:
            BrGbiTexScanLoadBlock(pSt, pCmd,
                                  BrGbiTexScanData(pSt, pSt->timgAddr));
            break;
        case 0xF5:
            BrGbiTexScanSetTile(pSt, pCmd);
            break;
        case 0xFD:
            BrGbiTexScanSetImg(pSt, pCmd);
            break;

        case 0xFA:                       /* inline block 0x100291FA */
            pSt->prim[0] = (uint8_t)(pCmd->w1 >> 24);
            pSt->prim[1] = (uint8_t)(pCmd->w1 >> 16);
            pSt->prim[2] = (uint8_t)(pCmd->w1 >> 8);
            pSt->prim[3] = (uint8_t)(pCmd->w1);
            pSt->f575444 = 1;
            break;
        case 0xFB:                       /* inline block 0x10029233 */
            pSt->env[0] = (uint8_t)(pCmd->w1 >> 24);
            pSt->env[1] = (uint8_t)(pCmd->w1 >> 16);
            pSt->env[2] = (uint8_t)(pCmd->w1 >> 8);
            pSt->env[3] = (uint8_t)(pCmd->w1);
            pSt->f575440 = 1;
            break;
        case 0xFC:                       /* inline block 0x1002926D */
            /* One specific G_SETCOMBINE is recognised; every other combine
             * clears the flag. */
            pSt->f575448 = (pCmd->w0 == 0xFC50FE04u &&
                            pCmd->w1 == 0x3FFDF3F8u) ? 1 : 0;
            break;

        default:
            BrGbiTexScanMark(pSt, pCmd);
            break;
        }
        pCmd += 1;
    }
}

/* ================================================================== */
/* 2b. Texture upload thunks                                          */
/* ================================================================== */

/* 0x10027C00 */
/* WHAT IT DOES: rounds a size up to the next power of two and reports the
 * answer as a shift count -- how many times you would double 1 to reach it.
 * Textures have to be powers of two, so this is how an odd width or height
 * gets rounded up. Anything above 128 is capped, and anything of 1 or less
 * gives zero. */
/* @implements 0x10027290 glide BrGbiSizeShift */
int BrGbiSizeShift(int n)
{
    int r;

    if (n <= 1)    return 0;
    if (n <= 2)    return 1;
    if (n <= 4)    return 2;
    if (n <= 8)    return 3;
    if (n <= 0x10) return 4;
    if (n <= 0x20) return 5;
    if (n <= 0x40) return 6;
    /* Last pair is one ret: cmp 0x80; mov 7; jle; mov 8.
     * Adjacent `return 7; return 8` lowers to setg+add (n in eax).
     * A named r gives the branchy form but n sits in ecx (eax-specific
     * `cmp eax,imm32` is 1 byte shorter). */
    r = 7;
    if (n > 0x80)
        r = 8;
    return r;
}

/* 0x10028C70 */
/* WHAT IT DOES: says how many texture pixels are packed into one machine
 * word for a given pixel-size code: 16 for the smallest, then 8, then 4, and
 * 2 for anything else. */
/* @implements 0x10028C70 d3d BrGbiTexelsPerWord */
/* @implements 0x10027F80 glide BrGbiTexelsPerWord */
int BrGbiTexelsPerWord(int siz)
{
    switch (siz) {
    case 0:  return 16;
    case 1:  return 8;
    case 2:  return 4;
    default: return 2;
    }
}

/* 0x10028BF0 */
/* WHAT IT DOES: passes a texture upload through to the graphics backend,
 * working out for it the one thing it does not get told -- how many bytes
 * one row of the texture occupies, given the width rounded up to a power of
 * two and the pixel size. */
/* @implements 0x10028BF0 d3d BrGbiBlit */
/* @implements 0x10027F00 glide BrGbiBlit */
#ifdef BR_MATCHING_BUILD
/* The original takes 14 args and calls through the import-pointer global
 * at 0x118ED1C4 (the slot before BrGbiTexCreate's 0x118ED1C8); the port's
 * pfn parameter is a port convenience. */
extern BrGbiBlitFn g_pfn18ED1C4;    /* 0x118ED1C4 */

void BrGbiBlit(uintptr_t a1, uintptr_t a2, uintptr_t a3, uintptr_t a4,
               uintptr_t a5, uintptr_t a6, uintptr_t a7, uintptr_t a8,
               uintptr_t a9, uintptr_t a10, uintptr_t a11, uintptr_t a12,
               uintptr_t a13, uintptr_t a14)
{
    int32_t   rounded = (int32_t)(1 << BrGbiSizeShift((int)a3));
    int32_t   pitch   = (rounded / BrGbiTexelsPerWord((int)a5)) * 8;

    g_pfn18ED1C4(a1, a2, a3, a4, (uintptr_t)(intptr_t)pitch,
                 a5, a6, a7, a8, a9, a10, a11, a12, a13, a14);
}
#else
void BrGbiBlit(BrGbiBlitFn pfn,
               uintptr_t a1, uintptr_t a2, uintptr_t a3, uintptr_t a4,
               uintptr_t a5, uintptr_t a6, uintptr_t a7, uintptr_t a8,
               uintptr_t a9, uintptr_t a10, uintptr_t a11, uintptr_t a12,
               uintptr_t a13, uintptr_t a14)
{
    int32_t   rounded = (int32_t)(1 << BrGbiSizeShift((int)a3));
    int32_t   pitch   = (rounded / BrGbiTexelsPerWord((int)a5)) * 8;

    pfn(a1, a2, a3, a4, (uintptr_t)(intptr_t)pitch,
        a5, a6, a7, a8, a9, a10, a11, a12, a13, a14);
}
#endif

/* 0x1002A280 */
/* WHAT IT DOES: builds the backend's version of a texture from a record the
 * game already holds, translating the record's flag bits into a pixel format
 * and size and rounding the dimensions up to powers of two. It refuses if
 * the record has no source pixels or if one particular flag bit is set, so
 * it never fills in a record that was empty. */
/* @implements 0x1002A280 d3d BrGbiTexCreate */
/* @implements 0x100297F0 glide BrGbiTexCreate */
#ifdef BR_MATCHING_BUILD
extern BrGbiTexCreateFn g_pfn18AA0B0;   /* 0x118ED1C8 */
void BrGbiTexCreate(BrGbiTexRec *pRec, uintptr_t a2)
{
    uint8_t *p = (uint8_t *)pRec;
    uint32_t flags, sel, fmt, siz;

    /* Glide record: pTex +0x00, f04 +0x04, w +0x0C, h +0x0E, flags +0x20.
     * The header's packed BrGbiTexRec is the 64-bit port layout. */
    if (*(uint32_t *)p == 0)
        return;
    flags = *(uint32_t *)(p + 0x20);
    if ((flags & 0x100000u) != 0)
        return;

    sel = flags & 0x0F000000u;
    if (sel == 0x01000000u) {
        fmt = 0; siz = 2;
    } else if (sel == 0x04000000u) {
        fmt = 1; siz = 4;
    } else {
        fmt = 2; siz = 0;
    }

    *(void **)p = g_pfn18AA0B0((void *)*(uint32_t *)p,
                     *(uint32_t *)(p + 4),
                     (uint32_t)(1 << BrGbiSizeShift((int)*(uint16_t *)(p + 0x0C))),
                     (uint32_t)(1 << BrGbiSizeShift((int)*(uint16_t *)(p + 0x0E))),
                     fmt, siz,
                     (flags >> 31) & 1u, (flags >> 30) & 1u,
                     (flags >> 29) & 1u, (flags >> 28) & 1u,
                     0u, 0u, 1u, a2);
}
#else
void BrGbiTexCreate(BrGbiTexCreateFn pfn, BrGbiTexRec *pRec, uintptr_t a2)
{
    uint32_t flags, sel, fmt, siz;

    if (pRec->pTex == NULL)
        return;
    flags = pRec->flags;
    if ((flags & 0x100000u) != 0)
        return;

    sel = flags & 0x0F000000u;
    if (sel == 0x01000000u) {
        fmt = 0; siz = 2;
    } else if (sel == 0x04000000u) {
        fmt = 1; siz = 4;
    } else {
        fmt = 2; siz = 0;
    }

    pRec->pTex = pfn(pRec->pTex, pRec->f04,
                     (uint32_t)(1 << BrGbiSizeShift((int)pRec->w)),
                     (uint32_t)(1 << BrGbiSizeShift((int)pRec->h)),
                     fmt, siz,
                     (flags >> 31) & 1u, (flags >> 30) & 1u,
                     (flags >> 29) & 1u, (flags >> 28) & 1u,
                     0u, 0u, 1u, a2);
}
#endif

/* 0x1002A740 */
/* WHAT IT DOES: makes the flat 4x4 placeholder texture used wherever a real
 * texture is not available, filling it with a dim value in two of the
 * display modes and a brighter one otherwise, then handing it to the backend
 * as a real texture. */
/* @implements 0x1002A740 d3d BrGbiSolidTexBuild */
/* @implements 0x10029C70 glide BrGbiSolidTexBuild */
#ifdef BR_MATCHING_BUILD
extern int     DAT_10226e80;           /* mode, 0x10226E80 */
extern uint8_t DAT_105e1810[];         /* 16 texels, 0x105E1810 */
extern int     DAT_10697a4c;           /* pTex out, 0x10697A4C */
void BrGbiSolidTexBuild(void)
{
    uint8_t  fill;
    uint8_t *p;

    fill = (DAT_10226e80 == 2 || DAT_10226e80 == 3) ? 0x20u : 0x80u;
    /* Orig: eax = &texels[1], write [eax-1]..[eax+2], add 4, jl &texels[17].
     * Pointer compare is unsigned (jb); orig is signed (jl). */
    p = DAT_105e1810 + 1;
    do {
        p[-1] = fill;
        p[0]  = fill;
        p[1]  = fill;
        p[2]  = fill;
        p += 4;
    } while ((int)p < (int)(DAT_105e1810 + 0x11));

    DAT_10697a4c = (int)g_pfn18AA0B0(DAT_105e1810, 0u, 4u, 4u, 1u, 4u,
                                    0u, 0u, 1u, 1u, 0u, 0u, 1u, 0u);
}
#else
void BrGbiSolidTexBuild(BrGbiTexCreateFn pfn, BrGbiSolidTex *pSt)
{
    uint8_t fill = (pSt->mode == 2 || pSt->mode == 3) ? 0x20u : 0x80u;
    int     i;

    for (i = 0; i < 16; ++i)
        pSt->aTexels[i] = fill;

    pSt->pTex = pfn(pSt->aTexels, 0u, 4u, 4u, 1u, 4u,
                    0u, 0u, 1u, 1u, 0u, 0u, 1u, 0u);
}
#endif

/* ================================================================== */
/* 3. Screen wipe / fade                                              */
/* ================================================================== */

/* 0x1002AEA0 */
/* WHAT IT DOES: releases one hold on the screen-transition effect and, when
 * the last hold goes, runs the teardown. Beware that the check is for
 * exactly zero after the decrement, so releasing one time too many drops the
 * count below zero and the teardown can never fire again. */
/* port-only body; Glide match is src/core/generated/0x10017F10.c */
int BrFadeRelease(BrFadeState *pSt)
{
    pSt->refCount -= 1;
    if (pSt->refCount == 0)
        pSt->pfnRelease();
    return 1;
}

/* 0x1002AEC0 */
/* WHAT IT DOES: resets the screen-transition wipe to its starting position
 * by copying two stored values into the live ones. */
/* port-only body; Glide match is src/core/generated/0x10017F30.c -- the
 * original takes no argument and addresses all four values absolutely, so the
 * BrFadeState view below cannot reproduce it.  Same split as BrFadeRelease. */
/* @n64 0x8026B434 located */
void BrFadeLatch(BrFadeState *pSt)
{
    pSt->pos      = pSt->srcC0;
    pSt->f5754FC  = pSt->srcC4;
}

/* The 0x3EB / 0x3E8 / 0 token soup both emit paths hand to
 * BrRdpSetCombineLERP; spelled out once so the two call sites stay readable
 * and identical to the original's push order. */
static void br16_combine(BrGfxWords *pOut, int t13, int t9, int t5, int t1)
{
    BrRdpSetCombineLERP(pOut,
                        0, 0, 0, t1,
                        0, 0, 0, t5,
                        0, 0, 0, t9,
                        0, 0, 0, t13);
}

/* 0x1002AF10 */
/* WHAT IT DOES: draws the translucent full-screen tint used during a
 * transition, at the requested opacity. It gives up entirely if the tint
 * would be too faint to see, and never lets it get more than about seven-
 * tenths opaque. The rectangle it covers comes from a table of screen
 * regions. */
/* @implements 0x1002AF10 d3d BrFadeDrawSprite */
/* @implements 0x10017F80 glide BrFadeDrawSprite */
#ifdef BR_MATCHING_BUILD
/* orig: cdecl (pRecs, alpha); cursor DAT_106e7710, rectIdx DAT_106ec798,
 * otherModeH DAT_106e7718. Combine is BrRdpSetCombineLERP(DAT++, 17 args)
 * with 0x3EB/0x3E8 immediates hoisted across the preceding emits.
 * (int)(alpha * 255.0f) is __ftol, not br16_ftol(double). */
void BrFadeDrawSprite(const uint32_t *pRecs, float alpha)
{
    BrGfxWords     *p;
    const uint32_t *pRec;
    uint32_t        lo, hi;
    int             idx;

    if (!(alpha >= 0.1f))
        return;
    if (alpha > 0.7f)
        alpha = 0.7f;

    p = DAT_106e7710++;
    p->w0 = 0xE7000000u;
    p->w1 = 0;

    p = DAT_106e7710++;
    p->w0 = 0xBA001402u;
    p->w1 = 0;

    p = DAT_106e7710++;
    p->w0 = 0xB900031Du;
    p->w1 = 0x00504340u;

    BrRdpSetCombineLERP(DAT_106e7710++,
                        0, 0, 0, 0x3EB,
                        0, 0, 0, 0x3EB,
                        0, 0, 0, 0x3EB,
                        0, 0, 0, 0x3EB);

    p = DAT_106e7710++;
    p->w0 = 0xFA000000u;
    p->w1 = (uint32_t)(int)(alpha * 255.0f) | 0xFFFFFF00u;

    p = DAT_106e7710++;
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
        p = DAT_106e7710++;
        idx = DAT_106ec798;
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

        idx = DAT_106ec798;
        pr = (const BrFadeRect *)recs + idx;
        p->w1 = ((pr->x0 & 0xFFFu) << 12) | (pr->y0 & 0xFFFu);
    }

    BrRdpSetCombineLERP(DAT_106e7710++,
                        0, 0, 0, 0x3EB,
                        0, 0, 0, 0x3EB,
                        0, 0, 0, 0x3E8,
                        0, 0, 0, 0x3E8);

    p = DAT_106e7710++;
    p->w0 = 0xE7000000u;
    p->w1 = 0;

    p = DAT_106e7710++;
    p->w0 = 0xBA000602u;
    p->w1 = (uint32_t)DAT_106e7718;
}
#else
void BrFadeDrawSprite(BrFadeState *pSt, const uint32_t *pRecs, float alpha)
{
    BrGfxWords     *p;
    BrGfxWords     *pE1;
    const uint32_t *pRec;
    uint32_t        lo, hi;

    /* NEGATED, and that is the whole point -- found by the equivalence audit.
     *
     *   1002AF14  fcomp dword ptr [0x1008F414]   ; 0.1f
     *   1002AF1A  fnstsw ax
     *   1002AF1C  test  ah, 1                    ; C0
     *   1002AF1F  jne   0x1002B120               ; -> ret
     *
     * `fcomp` sets C0 for LESS-THAN *and* for UNORDERED, so the original
     * returns on NaN and emits nothing. `alpha < 0.1f` is FALSE for NaN, so
     * the port fell through and emitted all ten display-list commands --
     * including a G_SETPRIMCOLOR whose alpha byte came from __ftol(NaN).
     *
     * NaN is reachable here: `over` is a duration and a zero `over` yields an
     * infinity that propagates into alpha (see slice2_16.h).
     *
     * This idiom was already understood in this very file -- BrFadeSetTarget
     * below uses `!(to < pSt->value)` for exactly this reason. It simply was
     * not applied here.
     *
     * The SECOND compare needs no negation: `test ah,0x41 / jne` fires only
     * when C0 and C3 are both clear, i.e. strictly greater AND ordered, and
     * `alpha > 0.7f` is already false for NaN. */
    if (!(alpha >= 0.1f))         /* 0x1008F414 */
        return;
    if (alpha > 0.7f)             /* 0x1008F418; the original overwrites the
                                   * incoming argument slot */
        alpha = 0.7f;

    p = br16_fade_alloc(pSt); p->w0 = 0xE7000000u; p->w1 = 0;
    p = br16_fade_alloc(pSt); p->w0 = 0xBA001402u; p->w1 = 0;
    p = br16_fade_alloc(pSt); p->w0 = 0xB900031Du; p->w1 = 0x00504340u;
    br16_combine(br16_fade_alloc(pSt), 0x3EB, 0x3EB, 0x3EB, 0x3EB);

    p = br16_fade_alloc(pSt);
    p->w0 = 0xFA000000u;                      /* G_SETPRIMCOLOR */
    p->w1 = (uint32_t)br16_ftol((double)(alpha * 255.0f)) | 0xFFFFFF00u;

    p = br16_fade_alloc(pSt); p->w0 = 0xBA000602u; p->w1 = 0x000000C0u;

    pE1  = br16_fade_alloc(pSt);
    pRec = pRecs + (size_t)pSt->rectIdx * BR_FADE_RECT_DWORDS;

    /* RESIDUE, and it is the whole function: TWO bytes, at 1002B0BA/1002B0BE
     * (glide 0x10017F80 + 0x137).  The original loads pRec[3] into esi and
     * pRec[1] into edi before `add esi, edi`; the recompile pairs them the
     * other way round.  Size, instruction count and both multisets are exact
     * (RAW and REGNORM 0+0), so this is register pairing, nothing else.
     * Probed and ruled out, do not re-run: swapping the two addends, naming
     * each operand as a block-scoped temp, splitting the mask into a separate
     * statement, and computing hi before lo -- all four are byte-identical to
     * what is here. T3a.
     *
     * Two more dead, 2026-09-03, both from levers proven elsewhere this week:
     * naming ONLY the addend that must load first (the "name the pointer"
     * lever that flipped a store's load order on 0x1006CFC0), and moving the
     * w1 store ahead of the w0 computation so pRec[1]'s other use comes
     * first. Both byte-identical. VC5 canonicalises the operand order of an
     * INTEGER sum the same way it does a float one, so the register pairing
     * is not reachable by rewriting this expression. Six probes now. */
    lo = (pRec[3] + pRec[1]) & 0xFFFu;
    hi = ((pRec[2] + pRec[0]) << 12) & 0xFFF000u;
    pE1->w0 = 0xE1000000u | hi | lo;
    pE1->w1 = ((pRec[0] & 0xFFFu) << 12) | (pRec[1] & 0xFFFu);

    br16_combine(br16_fade_alloc(pSt), 0x3E8, 0x3E8, 0x3EB, 0x3EB);

    p = br16_fade_alloc(pSt); p->w0 = 0xE7000000u; p->w1 = 0;
    p = br16_fade_alloc(pSt); p->w0 = 0xBA000602u; p->w1 = pSt->otherModeH;
}
#endif

/* 0x1002B130 */
/* WHAT IT DOES: aims the screen wipe at a new position, to be reached over
 * the given time. Going forward it simply sets the target and speed; going
 * backward it may instead flag a bounce, so a wipe already on its way out
 * reverses when it lands rather than restarting. Note the time is a duration
 * and is divided into, so asking for zero time gives an infinite speed. */
/* @implements 0x1002B130 d3d BrFadeSetTarget */
/* @implements 0x100181A0 glide BrFadeSetTarget */
#ifndef BR_MATCHING_BUILD
void BrFadeSetTarget(BrFadeState *pSt, float to, float over)
{
    pSt->kick = 1;

    /* 0x1002B134 `fcomp [value] / test cl,ah` with cl==1 -- an `ah,1` test
     * spelled with a register -- then `jne 0x1002B176`, i.e. the C0 case
     * jumps AWAY from this arm.  So this arm needs C0 CLEAR: ordered and
     * to >= value, which is exactly C's `>=`.
     *
     * THIS WAS `!(to < pSt->value)`, and it was held up in the note over
     * BrFadeDrawSprite as the idiom to copy.  It is the right idiom for the
     * OPPOSITE branch sense: negation is faithful when the original jumps INTO
     * the arm on C0, and wrong when it jumps out of it, because NaN belongs on
     * the C0 side either way.  Reading "there is a negation, so NaN was
     * considered" is not the same as checking which side it lands on.
     *
     * Second compare, 0x1002B14F: `test ah,0x40 / jne` away -- C3 (equal or
     * unordered) leaves, so this arm needs ordered-and-unequal against 0.0f
     * (0x1008F410).  `to != 0.0f` is true for NaN and was wrong; it is also
     * unreachable with a NaN `to` once the first test is right, because `&&`
     * short-circuits exactly as the original's branch does. */
    if (to >= pSt->value && BR16_FNEO(to, 0.0f)) {   /* 0x1008F410 == 0.0f */
        pSt->target = to;
        pSt->rate   = 1.0f / over;                   /* 0x1008F420 == 1.0f */
        return;
    }

    /* GOTCHA: the guard is value != 1.0f, not == 1.0f -- the `jne` after
     * `test ah,0x40` (0x1002B184) leaves the bounce path only when the two
     * are UNequal.  0x1008F420 == 1.0f, 0x1008F428 == 0.0 (a double).
     *
     * Both halves were NaN-wrong.  `value != 1.0f` is true for an unordered
     * compare where the original's C3 sends it to the tail; and 0x1002B197's
     * `test ah,0x41 / jne` away means the bounce needs ORDERED GREATER, which
     * is plain `> 0.0` -- `!(rate <= 0.0)` is true for NaN and was not. */
    if (BR16_FNEO(pSt->value, 1.0f) && pSt->rate > 0.0) {
        pSt->bounce = 1;
        return;
    }
    pSt->target = to;
    pSt->rate   = -1.0f / over;               /* 0x1008F430 == -1.0f */
}
#endif

/* 0x1002B1C0 */
/* WHAT IT DOES: aims one of the two independent brightness ramps at a new
 * value over the given time, choosing to climb or fall depending on which
 * side of the target it is currently on. As with the wipe, a zero duration
 * gives an infinite rate. */
/* @implements 0x1002B1C0 d3d BrFadeSetTargetA */
/* @implements 0x10018230 glide BrFadeSetTargetA */
#ifndef BR_MATCHING_BUILD
void BrFadeSetTargetA(BrFadeState *pSt, float to, float over)
{
    pSt->kickA = 1;
    pSt->tgtA  = to;

    /* 0x1002B1DD `fcomp [0.0f] / test ah,1 / jne 0x1002B20C` on (to - curA),
     * and 0x1002B1EE `fcomp [0.0f] / test ah,0x40 / jne 0x1002B20C` on `to`.
     * BOTH jumps go to the NEGATIVE-rate tail, so the positive arm needs both
     * flags CLEAR: ordered `>= 0` and ordered `!= 0`.  The old
     * `!(to - curA < 0.0f) && to != 0.0f` was true for NaN on both halves and
     * therefore sent an unordered input to the positive arm where the
     * original sends it to the negative one.
     *
     * MUTATION SURVIVOR, and legitimately so: replacing the BR16_FNEO with a
     * plain `to != 0.0f` changes nothing for ANY input.  The two differ only
     * when `to` is a NaN, and a NaN `to` makes `to - curA` a NaN too, so the
     * first operand is already false and `&&` never evaluates the second --
     * which is exactly what the original's first `jne` does.  The faithful
     * spelling is kept because it records the flag mask, not because a test
     * can reach it. */
    if (to - pSt->curA >= 0.0f && BR16_FNEO(to, 0.0f))
        pSt->rateA = 1.0f / over;
    else
        pSt->rateA = -1.0f / over;
}
#endif

/* 0x1002B220 */
/* WHAT IT DOES: the same as the ramp above, for the second of the two
 * independent brightness ramps. */
/* @implements 0x1002B220 d3d BrFadeSetTargetB */
/* @implements 0x10018290 glide BrFadeSetTargetB */
#ifndef BR_MATCHING_BUILD
void BrFadeSetTargetB(BrFadeState *pSt, float to, float over)
{
    pSt->kickB = 1;
    pSt->tgtB  = to;

    /* 0x1002B23D / 0x1002B24E -- instruction for instruction the same as
     * BrFadeSetTargetA; see the note there. */
    if (to - pSt->curB >= 0.0f && BR16_FNEO(to, 0.0f))
        pSt->rateB = 1.0f / over;
    else
        pSt->rateB = -1.0f / over;
}
#endif

/* 0x1002B2A0 */
/* WHAT IT DOES: reports whether the screen transition is on its way closed
 * -- either currently moving backward, or flagged to reverse when it lands. */
#ifdef BR_MATCHING_BUILD
/* The fade originals read the loose globals directly: value 0x104B16C0,
 * target 0x104B16B8, rate 0x104B16BC, bounce 0x104B16D8, the A/B channel
 * rates 0x104B16C8/C4, latch flags 0x104B16CC/D0/D4, and the A/B pair
 * shadows at 0x100A75xx. Constants: 0x10077370 = 0.0f, 0x10077380 /
 * 0x10077390 = the +/- rate numerators, 0x10077388 a double threshold. */
extern float  DAT_104b16b8, DAT_104b16bc, DAT_104b16c0;
extern float  DAT_104b16c4, DAT_104b16c8;
extern int    DAT_104b16cc, DAT_104b16d0, DAT_104b16d4, DAT_104b16d8;
extern float  DAT_100a7500, DAT_100a7504, DAT_100a7508, DAT_100a750c;
extern float  DAT_10077370, DAT_10077380, DAT_10077390;
extern double DAT_10077388;

void BrFadeSetTarget(float v, float dur)
{
    DAT_104b16cc = 1;
    if (!(v < DAT_104b16c0) && v != DAT_10077370) {
        DAT_104b16b8 = v;
        DAT_104b16bc = DAT_10077380 / dur;
        return;
    }
    if (DAT_104b16c0 != DAT_10077380 && DAT_104b16bc > DAT_10077388) {
        DAT_104b16d8 = 1;
        return;
    }
    DAT_104b16b8 = v;
    DAT_104b16bc = DAT_10077390 / dur;
}

void BrFadeSetTargetA(float v, float dur)
{
    DAT_104b16d4 = 1;
    DAT_100a7508 = v;
    if (v - DAT_100a750c < DAT_10077370 || v == DAT_10077370)
        DAT_104b16c8 = DAT_10077390 / dur;
    else
        DAT_104b16c8 = DAT_10077380 / dur;
}

void BrFadeSetTargetB(float v, float dur)
{
    DAT_104b16d0 = 1;
    DAT_100a7500 = v;
    if (v - DAT_100a7504 < DAT_10077370 || v == DAT_10077370)
        DAT_104b16c4 = DAT_10077390 / dur;
    else
        DAT_104b16c4 = DAT_10077380 / dur;
}

int BrFadeIsClosing(void)
{
    if (!(DAT_104b16bc >= DAT_10077370))
        goto yes;
    if (DAT_104b16d8 == 0)
        return 0;
yes:
    return 1;
}

int BrFadeIsSettled(void)
{
    if (DAT_104b16c0 != DAT_104b16b8)
        goto no;
    if (DAT_104b16d8 == 0)
        return 1;
no:
    return 0;
}

int BrFadeIsShut(void)
{
    if (DAT_104b16bc >= DAT_10077370)
        goto no;
    if (DAT_104b16c0 != DAT_10077370)
        goto no;
    if (DAT_104b16d8 == 0)
        return 1;
no:
    return 0;
}
#endif

/* WHAT IT DOES: reports whether the screen transition is currently heading
 * TOWARDS covering the screen -- fading down rather than up -- or is set to
 * bounce back and do so. Callers use it to hold off on anything that would be
 * hidden a moment later.
 *
 * The negated comparison is load-bearing: an unordered (NaN) rate answers
 * yes, matching the original's `test ah,1` on the compare flags. */
/* @implements 0x1002B2A0 d3d BrFadeIsClosing */
/* @implements 0x10018310 glide BrFadeIsClosing */
#ifndef BR_MATCHING_BUILD
int BrFadeIsClosing(const BrFadeState *pSt)
{
    /* 0x1002B2AE `test ah,1 / jne 0x1002B2BF`, and 0x1002B2BF is
     * `mov eax,1 / ret`.  C0 is set for unordered too, so an unordered rate
     * returns 1 here.  `rate < 0.0f` is false for NaN and returned 0. */
    if (!(pSt->rate >= 0.0f))
        return 1;
    return (pSt->bounce != 0) ? 1 : 0;
}
#endif

/* 0x1002B2D0 */
/* WHAT IT DOES: reports whether the screen transition has finished moving
 * and has no reversal pending, which is how the game knows it can proceed to
 * whatever the transition was covering. */
/* @implements 0x1002B2D0 d3d BrFadeIsSettled */
/* @implements 0x10018340 glide BrFadeIsSettled */
#ifndef BR_MATCHING_BUILD
int BrFadeIsSettled(const BrFadeState *pSt)
{
    /* 0x1002B2DE `test ah,0x40 / je 0x1002B2F2`, and 0x1002B2F2 is
     * `xor eax,eax / ret`.  The zero-flag case is C3 CLEAR -- ordered AND
     * unequal -- so an unordered pair falls through and is reported SETTLED
     * (subject to the bounce flag).  `value != target` is true for NaN and
     * returned 0. */
    if (BR16_FNEO(pSt->value, pSt->target))
        return 0;
    return (pSt->bounce != 0) ? 0 : 1;
}
#endif

/* 0x1002B300 */
/* WHAT IT DOES: reports whether the screen transition is fully closed:
 * moving backward, arrived at zero, and with no reversal pending. */
/* @implements 0x1002B300 d3d BrFadeIsShut */
/* @implements 0x10018370 glide BrFadeIsShut */
#ifndef BR_MATCHING_BUILD
int BrFadeIsShut(const BrFadeState *pSt)
{
    /* BOTH tests leave by `je 0x1002B335`, which is `xor eax,eax / ret`.
     *
     *   0x1002B30E  test ah,1     je -> return 0   ==>  continue on C0
     *   0x1002B321  test ah,0x40  je -> return 0   ==>  continue on C3
     *
     * C0 and C3 are BOTH set by an unordered compare, so a NaN rate or a NaN
     * value makes the original continue and report SHUT.  The port had
     * `!(rate < 0.0f)` (true for NaN -> returned 0) and `value != 0.0f`
     * (also true for NaN -> returned 0), so it returned the opposite answer
     * on both.  This one is LIVE: BrFadeDrawBars calls it and sets
     * `bars = 3` from the result. */
    if (pSt->rate >= 0.0f)
        return 0;
    if (BR16_FNEO(pSt->value, 0.0f))
        return 0;
    return (pSt->bounce != 0) ? 0 : 1;
}
#endif

/* The 0xE1 command both bar-emitting arms build. */
static uint32_t br16_bar_w0(int32_t top, int32_t width, int32_t shift)
{
    uint32_t a = (uint32_t)((uint32_t)top << shift);
    uint32_t b = (uint32_t)((uint32_t)width << shift);

    a = (uint32_t)(a + 0xFFFFFu);
    a = (a << 12) & 0xFFF000u;
    b = (b - 1u) & 0xFFFu;
    return 0xE1000000u | a | b;
}

/* 0x1002B340 */
/* WHAT IT DOES: draws the wipe bars -- the solid blocks that sweep across
 * the screen during a transition -- by emitting the filled-rectangle
 * commands for them. It draws nothing while the wipe is fully open, and only
 * a limited number of frames' worth of bars once the wipe has run out of
 * travel. */
/* @implements 0x1002B340 d3d BrFadeDrawBars */
/* @implements 0x100183B0 glide BrFadeDrawBars */
#ifdef BR_MATCHING_BUILD
/* The original takes NO ARGUMENT: it reads eleven standalone globals, exactly
 * as BrFadeDrawSprite above does, and its very first instruction is
 * `fld [0x104b16c0]` (the fade value).  The port's BrFadeState * costs a
 * field load per access and, worse, turns every display-list allocation into
 * a `call br16_fade_alloc` -- the original has FIVE calls in total (two to
 * the 17-argument combine emitter, two to __ftol and one to BrFadeIsShut),
 * where the port arm has twenty-one.
 *
 * The eleven globals were read out of the original's own operands and matched
 * to the port's fields through the two address bases this file already
 * establishes (BrFadeDrawSprite's cursor, and BrFadeSetTarget's wipe block):
 *   0x106e7710 pCmd     0x106e7714 span     0x106e9a2c width
 *   0x106ed674 shift    0x106ed67c parity   0x104b1698 aPos2[2]
 *   0x104b16a8 pos      0x104b16b0 pos2     0x104b16c0 value
 *   0x100a7510 bars
 *
 * MACROS, not statics: MSVC5 does not inline a static with more than one
 * caller, so br16_fade_alloc (twelve sites) and br16_bar_w0 (three) would each
 * be a call the original does not have.  br16_ftol stays a call -- the
 * original really does call it twice.
 *
 * RESIDUE 800 vs 803 bytes, 216 vs 222 instructions, register-blind 3+9
 * (from 592 / 186 / 77+113 as a BrFadeState * body).  Two items, both walls:
 *
 *  - FOUR instructions in the `value == 0` arm's w1.  The original genuinely
 *    computes `((0 << shift) & 0xFFF) << 12` -- `xor edx,edx / shl edx,cl /
 *    and edx,0xfff / shl edx,0xc` -- four instructions to produce a zero it
 *    already has.  PROBED AND DEAD, all three fold to a plain `mov [eax+4],0`
 *    and none changes the instruction count: the shift written out with a
 *    `0u` literal, with an `(uint32_t)0` cast, and through a zero-initialised
 *    local.  VC5 folds a shift of a known zero, so whatever the original's
 *    source had there, it was not a constant the compiler could see.
 *  - TWO instructions in the three `!= 0` / `<` tests on globals: we emit
 *    `cmp dword ptr [g], reg`, the original loads the global into a register
 *    first and compares register to register.  Allocation; the prologue shows
 *    the same thing (the original pushes ebx early and keeps its zero there,
 *    we defer the push to first use and zero ebp instead). */
extern int32_t     DAT_106e7714;      /* span   */
extern int32_t     DAT_106e9a2c;      /* width  */
extern int32_t     DAT_106ed674;      /* shift  */
extern int32_t     DAT_106ed67c;      /* parity */
extern int32_t     DAT_104b1698[2];   /* aPos2, indexed by parity */
extern int32_t     DAT_104b16a8;      /* pos    */
extern int32_t     DAT_104b16b0;      /* pos2   */
extern float       DAT_104b16c0;      /* value  */
extern int32_t     DAT_100a7510;      /* bars   */
int BrFadeIsShut(void);               /* orig: no argument, reads globals */

#define BR16_ALLOC()  (DAT_106e7710++)
#define BR16_BAR_W0(top_, width_, shift_)                                  \
    (0xE1000000u                                                           \
     | ((((((uint32_t)(top_) << (shift_)) + 0xFFFFFu)) << 12) & 0xFFF000u)  \
     | (((((uint32_t)(width_) << (shift_))) - 1u) & 0xFFFu))

void BrFadeDrawBars(void)
{
    BrGfxWords *p;
    /* The original's dead store really is a store: it keeps the aPos2 read
     * in a stack slot nothing reads.  VC5 deletes a plain local here, so the
     * slot is modelled with `volatile` -- a codegen device, not a claim about
     * the original's source. */
    volatile int32_t dead;

    /* ONE fcomp: the original is `fcomp 1.0f / fnstsw / test ah,0x40 / jne`,
     * which is exactly what VC5 emits for a plain `==` on floats -- C3 set,
     * i.e. equal OR unordered, so a NaN value returns here too.  The
     * BR16_FEQU macro spells that out as `!(a < b || a > b)` and costs a
     * SECOND fcomp; it is the right model for the reader and the wrong one
     * for the bytes. */
    if (DAT_104b16c0 == 1.0f)
        return;

    p = BR16_ALLOC(); p->w0 = 0xE7000000u; p->w1 = 0;
    p = BR16_ALLOC(); p->w0 = 0xBA001402u; p->w1 = 0;
    p = BR16_ALLOC(); p->w0 = 0xB900031Du; p->w1 = 0x0F0A4000u;
    BrRdpSetCombineLERP(BR16_ALLOC(),
                        0, 0, 0, 0x3EB, 0, 0, 0, 0x3EB,
                        0, 0, 0, 0x3EB, 0, 0, 0, 0x3EB);

    p = BR16_ALLOC();
    p->w0 = 0xE2000000u;
    {
        /* `fild` straight into `call __ftol` -- a round trip the original's
         * source left in, so this is just the shifted integer.  It must go
         * through a named double: a helper taking a double PUSHES it on the C
         * stack (sub esp / fstp qword / add esp), and a bare
         * `(int)(double)intexpr` is folded away entirely. */
        double   dx = (double)(DAT_106e9a2c << DAT_106ed674);
        double   dy = (double)(DAT_106e7714 << DAT_106ed674);
        uint32_t x  = (uint32_t)(int32_t)dx & 0xFFFu;
        uint32_t y  = (uint32_t)(int32_t)dy & 0xFFFu;
        p->w1 = x | (y << 12);
    }

    p = BR16_ALLOC(); p->w0 = 0xFA00FFFFu; p->w1 = 0;

    if (DAT_104b16b0 != 0) {
        /* Dead store in the original: the aPos2 entry for the INVERTED parity
         * goes to a stack local nothing ever reads. */
        dead = DAT_104b1698[DAT_106ed67c ^ 1];

        if (BrFadeIsShut())
            DAT_100a7510 = 3;

        p = BR16_ALLOC();
        p->w0 = 0xB900031Du; p->w1 = 0x00504340u;
        BrRdpSetCombineLERP(BR16_ALLOC(),
                            0, 0, 0, 0x3EB, 0, 0, 0, 0x3EB,
                            0, 0, 0, 0x3EB, 0, 0, 0, 0x3EB);

        p = BR16_ALLOC(); p->w0 = 0xFA000000u; p->w1 = 0xFFu;

        p = BR16_ALLOC();
        p->w1 = 0;
        p->w0 = BR16_BAR_W0(DAT_104b16b0, DAT_106e9a2c, DAT_106ed674);
    }

    if (DAT_104b16a8 < DAT_106e7714) {
        if (DAT_100a7510 != 0) {
            p = BR16_ALLOC();
            DAT_100a7510 -= 1;
            p->w0 = BR16_BAR_W0(DAT_106e7714, DAT_106e9a2c, DAT_106ed674);
            p->w1 = (uint32_t)(((uint32_t)DAT_104b16a8 << DAT_106ed674)
                               & 0xFFFu) << 12;
        }
    } else if (DAT_104b16c0 == 0.0f && DAT_100a7510 != 0) {
        p = BR16_ALLOC();
        DAT_100a7510 -= 1;
        p->w0 = BR16_BAR_W0(DAT_106e7714, DAT_106e9a2c, DAT_106ed674);
        /* The original shifts a zero and masks it: always 0. */
        p->w1 = 0;
    }

    p = BR16_ALLOC(); p->w1 = 0; p->w0 = 0xE7000000u;
}
#undef BR16_ALLOC
#undef BR16_BAR_W0
#else
void BrFadeDrawBars(BrFadeState *pSt)
{
    BrGfxWords *p;

    /* 0x1002B34F `test ah,0x40 / jne 0x1002B661` (return).  C3 is set for
     * EQUAL and for UNORDERED, so a NaN value emits nothing.  `value == 1.0f`
     * is false for NaN, so the port emitted the whole twelve-command bar
     * sequence where the original emits none. */
    if (BR16_FEQU(pSt->value, 1.0f))   /* 0x1008F420 */
        return;

    p = br16_fade_alloc(pSt); p->w0 = 0xE7000000u; p->w1 = 0;
    p = br16_fade_alloc(pSt); p->w0 = 0xBA001402u; p->w1 = 0;
    p = br16_fade_alloc(pSt); p->w0 = 0xB900031Du; p->w1 = 0x0F0A4000u;
    br16_combine(br16_fade_alloc(pSt), 0x3EB, 0x3EB, 0x3EB, 0x3EB);

    p = br16_fade_alloc(pSt);
    p->w0 = 0xE2000000u;
    {
        /* fild followed straight by __ftol: a round trip the compiler left
         * in, so this really is just the shifted integer. */
        uint32_t x = (uint32_t)br16_ftol((double)(pSt->width << pSt->shift))
                     & 0xFFFu;
        uint32_t y = (uint32_t)br16_ftol((double)(pSt->span  << pSt->shift))
                     & 0xFFFu;
        p->w1 = x | (y << 12);
    }

    p = br16_fade_alloc(pSt); p->w0 = 0xFA00FFFFu; p->w1 = 0;

    if (pSt->pos2 != 0) {
        /* Dead store in the original: the value read out of aPos2 with the
         * parity inverted is written to a stack local nothing reads. */
        (void)pSt->aPos2[pSt->parity ^ 1];

        if (BrFadeIsShut(pSt))
            pSt->bars = 3;

        p = br16_fade_alloc(pSt);
        p->w0 = 0xB900031Du; p->w1 = 0x00504340u;
        br16_combine(br16_fade_alloc(pSt), 0x3EB, 0x3EB, 0x3EB, 0x3EB);

        p = br16_fade_alloc(pSt); p->w0 = 0xFA000000u; p->w1 = 0xFFu;

        p = br16_fade_alloc(pSt);
        p->w1 = 0;
        p->w0 = br16_bar_w0(pSt->pos2, pSt->width, pSt->shift);
    }

    if (pSt->pos < pSt->span) {
        if (pSt->bars != 0) {
            p = br16_fade_alloc(pSt);
            pSt->bars -= 1;
            p->w0 = br16_bar_w0(pSt->span, pSt->width, pSt->shift);
            p->w1 = (uint32_t)(((uint32_t)pSt->pos << pSt->shift) & 0xFFFu)
                    << 12;
        }
        /* 0x1002B5D5 `test ah,0x40 / je 0x1002B643` -- the arm is taken on
         * C3, i.e. equal OR UNORDERED.  `value == 0.0f` skipped it on NaN.
         *
         * MUTATION SURVIVOR, legitimately: the guard at the top of this
         * function already returns on a NaN `value`, and nothing between here
         * and there writes it (BrFadeIsShut takes a const pointer), so this
         * site can never see one.  Spelled faithfully anyway -- it is free,
         * and the next edit to that top guard would otherwise silently make
         * this one wrong. */
    } else if (BR16_FEQU(pSt->value, 0.0f) && pSt->bars != 0) {  /* 0x1008F410 */
        p = br16_fade_alloc(pSt);
        pSt->bars -= 1;
        p->w0 = br16_bar_w0(pSt->span, pSt->width, pSt->shift);
        /* The original shifts a zero and masks it: always 0. */
        p->w1 = 0;
    }

    p = br16_fade_alloc(pSt); p->w1 = 0; p->w0 = 0xE7000000u;
}
#endif

/* One ramp step. Shared by the two ramp arms of 0x1002B670, which are
 * identical instruction for instruction: ramp A at 0x1002B7F2..0x1002B864 and
 * ramp B at 0x1002B88B onward.
 *
 * GOTCHA: unlike the wipe, a ramp that lands exactly on its target is left
 * alone -- the forward arm tests `ah,0x41` (ordered GREATER) where the wipe
 * tests `ah,1` (ordered greater-or-EQUAL).
 *
 * ALL FOUR COMPARISONS HERE WERE NaN-WRONG, addresses from ramp A:
 *
 *   0x1002B800  test ah,0x40 / jne skip   -- the guard is C3 CLEAR, ordered
 *               and unequal.  `*pCur != tgt` is true for NaN and stepped.
 *   0x1002B827  test ah,1, read by the `je` at 0x1002B83E (`fnstsw` does not
 *               touch EFLAGS, so the zero flag survives the intervening
 *               fstp/fld/fcomp).  Forward is C0 CLEAR: ordered rate >= 0.
 *               `!(rate < 0.0f)` made an unordered rate go FORWARD.
 *   0x1002B856  test ah,0x41 / jne skip   -- clamp only on ordered greater.
 *               `!(*pCur <= tgt)` clamped a NaN; the original does not.
 *   0x1002B843  test ah,1 / je skip       -- clamp on C0, so unordered DOES
 *               clamp.  `*pCur < tgt` is false for NaN and did not. */
static void br16_ramp_step(float *pCur, float tgt, float rate, float dt,
                           int32_t *pKick, uint8_t *pOut)
{
    if (*pKick != 0) {
        *pKick = 0;
    } else if (BR16_FNEO(*pCur, tgt)) {
        int forward = (rate >= 0.0f);
        *pCur = rate * dt + *pCur;
        if (forward) {
            if (*pCur > tgt)
                *pCur = tgt;
        } else {
            if (!(*pCur >= tgt))
                *pCur = tgt;
        }
    }
    *pOut = (uint8_t)br16_ftol((double)*pCur * 255.0);  /* 0x1008F438 */
}

/* 0x100186E0 */
/* Matching-model globals for the fade/wipe module (glide addresses).  The
 * struct-based BrFadeState helpers above are the PORT's model of the same
 * storage; nothing constructs both at once today, but if a BrFadeState is
 * ever pointed at the shipped addresses these must be the storage, not a
 * second copy (aliased-storage rule, slice6_78.h banner). */
int32_t g_brFadeKick;      /* 0x104B16CC */
int32_t g_brFadeBounce;    /* 0x104B16D8 */
int32_t g_brFadeKickA;     /* 0x104B16D4 */
int32_t g_brFadeKickB;     /* 0x104B16D0 */
float   g_brFadeValue;     /* 0x104B16C0 */
float   g_brFadeTarget;    /* 0x104B16B8 */
float   g_brFadeRate;      /* 0x104B16BC */
float   g_brFadeRateA;     /* 0x104B16C8 */
float   g_brFadeRateB;     /* 0x104B16C4 */
float   g_brFadeCurA;      /* 0x100A750C */
float   g_brFadeTgtA;      /* 0x100A7508 */
float   g_brFadeCurB;      /* 0x100A7504 */
float   g_brFadeTgtB;      /* 0x100A7500 */
float   g_brFadeDt;        /* 0x106E9D8C */
int32_t g_brFadeParity;    /* 0x106ED67C */
int32_t g_brFadePos;       /* 0x104B16B0 */
int32_t g_brFadePos2;      /* 0x104B16A8 */
int32_t g_brFadeWidth;     /* 0x106E9A2C */
int32_t g_brFadeSpan;      /* 0x106E7714 */
int32_t g_brFadeB4;        /* 0x104B16B4 */
int32_t g_brFadeA4;        /* 0x104B16A4 */
int32_t g_brFadePosHist[2];  /* 0x104B1698 */
int32_t g_brFadePos2Hist[2]; /* 0x104B1690 */
uint8_t g_brFadeOutA;      /* 0x100BB2EC */
uint8_t g_brFadeOutB;      /* 0x100BB2E4 */

/* WHAT IT DOES: advances the screen transition by one frame: moves the wipe
 * toward its target, reverses it if a bounce was pending and it just
 * arrived, recomputes where the bars now sit, and steps both brightness
 * ramps, publishing each as a 0-255 value. One quirk worth knowing: the wipe
 * treats landing exactly on its target as an overshoot and the ramps do not.
 * The original INLINES both ramp steps (no helper) and works entirely in
 * globals; the NaN notes from the struct-based pass still describe the
 * comparison senses.
 *
 * NOT MATCHING by 4 bytes: one esi/edi role toggle in the backward-wipe
 * arm (sub eax,esi vs edi and the paired adds/cmp).  Statement-order probes
 * move the toggle between the load window and the arithmetic window but
 * never clear both -- allocator-residue class. */
/* @implements 0x100186E0 glide BrFadeTick */
void BrFadeTick(void)
{
    if (g_brFadeKick == 0) {
        if (g_brFadeValue != g_brFadeTarget) {
        g_brFadeValue = g_brFadeRate * g_brFadeDt + g_brFadeValue;
        if (g_brFadeRate < 0.0f) {
            if (!(g_brFadeValue >= g_brFadeTarget))
                g_brFadeValue = g_brFadeTarget;
        } else {
            /* overshoot INCLUDES equality: this is what lets the bounce
             * fire when the wipe lands exactly on its target */
            if (g_brFadeValue >= g_brFadeTarget) {
                g_brFadeValue = g_brFadeTarget;
                if (g_brFadeBounce != 0) {
                    g_brFadeRate   = -g_brFadeRate;
                    g_brFadeTarget = 0.0f;
                    g_brFadeBounce = 0;
                }
            }
        }
        }
    } else {
        g_brFadeKick = 0;
    }

    /* RESIDUE (glide 0x100186E0, 4 masked diffs, T3a): the original homes
     * Pos in esi / Pos2 in edi (loads esi-first, stores PosHist-first);
     * every probed spelling gets one axis wrong -- statement swap flips
     * the load regs (8 diffs), a Pos temp survives the aliasing Hist
     * store but binds edi (6), a Pos2 temp either dissolves (4) or flips
     * the store order (6).
     *
     * CORRECTION (2026-09-03): "register pairing only" was wrong. Two of
     * the four diffs are a SHAPE difference -- the original relocates both
     * stores against ONE base and gives the second a literal +8:
     *     mov [eax*4 + 0x104B1698], esi     ; PosHist  = Pos2Hist + 8
     *     mov [eax*4 + 0x104B1690], edi
     * so in the original these are ONE array of four ints at 0x104B1690,
     * not the two arrays declared above. Reproducing that by indexing
     * Pos2Hist past its end does fix the displacement (2 shape diffs -> 1)
     * but perturbs an earlier READ of the same arrays and ends up worse
     * (4 diffs -> 15/18). The real fix is to declare one array here and
     * give the two names views of it, which means touching slice2_16.h --
     * a serialised header edit, so it is left for a session that owns
     * that header. */
    g_brFadePos2Hist[g_brFadeParity] = g_brFadePos2;
    g_brFadePosHist[g_brFadeParity]  = g_brFadePos;
    g_brFadeB4 = 0;
    g_brFadeA4 = g_brFadeWidth;

    if (g_brFadeRate > 0.0f) {
        int32_t v = (int32_t)(g_brFadeSpan * g_brFadeValue);
        g_brFadePos  = 0;
        g_brFadePos2 = (v + 3) & ~3;
    } else if (!(g_brFadeRate >= 0.0f)) {
        int32_t v = (int32_t)(g_brFadeSpan * g_brFadeValue);
        int32_t step = ((g_brFadeSpan - v - g_brFadePos) + 3) & ~3;
        g_brFadePos2 += step;
        g_brFadePos  += step;
        if (g_brFadePos2 > g_brFadeSpan)
            g_brFadePos2 = g_brFadeSpan;
    } else {
        g_brFadePos  = 0;
        g_brFadePos2 = g_brFadeSpan;
    }

    if (g_brFadeKickA == 0) {
        if (g_brFadeCurA != g_brFadeTgtA) {
        g_brFadeCurA = g_brFadeRateA * g_brFadeDt + g_brFadeCurA;
        if (g_brFadeRateA < 0.0f) {
            if (!(g_brFadeCurA >= g_brFadeTgtA))
                g_brFadeCurA = g_brFadeTgtA;
        } else {
            /* forward clamp is STRICT greater (test ah,0x41) */
            if (g_brFadeCurA > g_brFadeTgtA)
                g_brFadeCurA = g_brFadeTgtA;
        }
        }
    } else {
        g_brFadeKickA = 0;
    }
    g_brFadeOutA = (uint8_t)(int32_t)((double)g_brFadeCurA * 255.0);

    if (g_brFadeKickB == 0) {
        if (g_brFadeCurB != g_brFadeTgtB) {
        g_brFadeCurB = g_brFadeRateB * g_brFadeDt + g_brFadeCurB;
        if (g_brFadeRateB < 0.0f) {
            if (!(g_brFadeCurB >= g_brFadeTgtB))
                g_brFadeCurB = g_brFadeTgtB;
        } else {
            /* forward clamp is STRICT greater (test ah,0x41) */
            if (g_brFadeCurB > g_brFadeTgtB)
                g_brFadeCurB = g_brFadeTgtB;
        }
        }
    } else {
        g_brFadeKickB = 0;
    }
    g_brFadeOutB = (uint8_t)(int32_t)((double)g_brFadeCurB * 255.0);
}


/* 0x1002B9C0 */
/* WHAT IT DOES: empties the vertex cache and the pointer list, so the next
 * batch of loaded geometry starts from nothing. */
/* @implements 0x1002B9C0 d3d BrRcaResetCounts */
/* @implements 0x10018A30 glide BrRcaResetCounts */
#ifdef BR_MATCHING_BUILD
void BrRcaResetCounts(void)
{
    g_brRca67B54C = 0;
    g_brRca67B548 = 0;
}
#else
void BrRcaResetCounts(BrVtxCache *pCache, BrPtrList *pList)
{
    pCache->nEntries = 0;
    pList->n = 0;
}
#endif

/* 0x1002B9E0 has moved to br_bits.c, which carries both builds' addresses:
 * br_track.c had transcribed the same function as `swap_u16_run` under
 * BRGlide's 0x10018A50.  br_bits is a leaf both can link. */

/* 0x1002BC90 */
/* WHAT IT DOES: turns a loaded mesh header the right way round: its entry
 * count, one following word, and then three words for each entry. Note the
 * entry count is re-read from the header on every pass of the loop, exactly
 * as the original does, so swapping it can change how many entries get
 * processed. */
/* @implements 0x10018D50 glide BrRcaSwapMesh */
#ifdef BR_MATCHING_BUILD
void BrRcaSwapMesh(void *pv)
{
    uint8_t *p = (uint8_t *)pv;
    int      i;
    uint32_t v;
    uint8_t *e;

    if (p == NULL)
        return;

    *(uint16_t *)(p + 2) = (uint16_t)(p[3] | (p[2] << 8));
    v = (((((uint32_t)p[4] << 8) | p[5]) << 8) | p[6]) << 8 | p[7];
    *(uint32_t *)(p + 4) = v;
    if (*(unsigned short *)(p + 2) <= 0)
        return;

    e = p + 0xA;
    i = 0;
    do {
        v = (((((uint32_t)e[-2] << 8) | e[-1]) << 8) | e[0]) << 8 | e[1];
        *(uint32_t *)(e - 2) = v;
        v = (((((uint32_t)e[2] << 8) | e[3]) << 8) | e[4]) << 8 | e[5];
        *(uint32_t *)(e + 2) = v;
        v = (((((uint32_t)e[6] << 8) | e[7]) << 8) | e[8]) << 8 | e[9];
        *(uint32_t *)(e + 6) = v;
        e += 0xC;
        i += 1;
    } while (i < (int)*(unsigned short *)(p + 2));
}
#else
void BrRcaSwapMesh(void *pv)
{
    uint8_t *p = (uint8_t *)pv;
    uint32_t i;

    if (p == NULL)
        return;

    br16_swap_u16_at(p + 2);
    br16_swap_u32_at(p + 4);

    /* The count is re-read from +0x02 on every iteration, exactly as the
     * original does. */
    for (i = 0; i < (uint32_t)br16_ld16(p + 2); ++i) {
        uint8_t *e = p + 8 + i * 12;
        br16_swap_u32_at(e + 0);
        br16_swap_u32_at(e + 4);
        br16_swap_u32_at(e + 8);
    }
}
#endif

int32_t  g_brSegN64Base;   /* 0x104B16E4 */
int32_t  g_brSegHostBase;  /* 0x104B16E0 */
uint8_t *g_brRcaBlob;      /* 0x106B7C7C */

/* WHAT IT DOES: prepares one texture record loaded from an .rca data file
 * for use: turns its fields the right way round, rebases the two addresses
 * it carries onto real memory, and then pulls in the actual pixel data --
 * either through an attached mesh header that says where in the file blob
 * the pixels live, or through a plain index into that blob. When the copying
 * is switched off it just does the byte order and lets the record go. */
/* @implements 0x10018B60 glide BrRcaFixupRecord */
void BrRcaFixupRecord(void *pRec)
{
    uint8_t *r = (uint8_t *)pRec;
    uint32_t tmp;
    uint32_t flags;
    uint8_t  a, b;

    /* +0x00 and +0x04: 4-byte reversal, then the in-place rebase.  The
     * three loads before any store are the original's order. */
    {
        uint8_t t0 = r[0], t3 = r[3], t1 = r[1];
        r[3] = t0; r[0] = t3;
        t3 = r[2];
        r[2] = t1; r[1] = t3;
    }
    BrSegPtrFixup((uint32_t *)(void *)r);

    {
        uint8_t t0 = r[4], t3 = r[7], t1 = r[5];
        r[7] = t0; r[4] = t3;
        t3 = r[6];
        r[6] = t1; r[5] = t3;
    }
    BrSegPtrFixup((uint32_t *)(void *)(r + 4));

    {
        uint8_t t0 = r[8], t3 = r[11], t1 = r[9];
        r[11] = t0; r[8] = t3;
        t3 = r[10];
        r[10] = t1; r[9] = t3;
    }

    *(uint16_t *)(r + 0x0C) = (uint16_t)(r[0x0D] | (r[0x0C] << 8));
    *(uint16_t *)(r + 0x0E) = (uint16_t)(r[0x0F] | (r[0x0E] << 8));
    *(uint16_t *)(r + 0x10) = (uint16_t)(r[0x11] | (r[0x10] << 8));
    *(uint16_t *)(r + 0x12) = (uint16_t)(r[0x13] | (r[0x12] << 8));
    *(uint16_t *)(r + 0x14) = (uint16_t)(r[0x15] | (r[0x14] << 8));
    *(uint16_t *)(r + 0x16) = (uint16_t)(r[0x17] | (r[0x16] << 8));
    /* 0x18..0x1F are deliberately left alone. */

    /* +0x20 is staged in a stack slot and reassembled from its bytes --
     * Horner form, matching the original's shl/or chain. */
    tmp = *(uint32_t *)(r + 0x20);
    flags = ((((((uint32_t)(uint8_t)tmp << 8)
             | ((const uint8_t *)&tmp)[1]) << 8)
             | ((const uint8_t *)&tmp)[2]) << 8)
             | ((const uint8_t *)&tmp)[3];
    *(uint32_t *)(r + 0x20) = flags;

    if (g_br675540 != 0) {
        if ((uint8_t)(flags >> 20) & 1) {
            uint8_t *mesh;
            uint32_t off0, off1;
            int      entry = 0;

            BrSegPtrFixup((uint32_t *)(void *)(r + 8));
            BrRcaSwapMesh(*(void **)(void *)(r + 8));
            mesh = *(uint8_t **)(void *)(r + 8);
            if (*(uint16_t *)(mesh + 2) == 2
                && *(uint32_t *)(mesh + 8) == 0xFFFFFFFFu)
                entry = 1;

            off1 = *(uint32_t *)(mesh + entry * 12 + 0x10);
            off0 = *(uint32_t *)(mesh + (entry + 1) * 12);

            if (entry == 0) {
                uint32_t n = *(uint32_t *)(r + 0x20) & 0x0003FFFFu;
                if (n != 0 && off0 != 0xFFFFFFFFu)
                    memcpy(*(void **)(void *)r, g_brRcaBlob + off0, n);
            }
            if (*(void **)(void *)(r + 4) != NULL
                && off1 != 0xFFFFFFFFu) {
                uint32_t len =
                    ((*(uint32_t *)(r + 0x20) & 0x0F000000u) == 0x01000000u)
                        ? 0x20u : 0x200u;
                memcpy(*(void **)(void *)(r + 4), g_brRcaBlob + off1, len);
            }
        } else {
            /* +0x08 is a 12-bit index scaled by 32 rather than a pointer. */
            uint32_t src = (*(uint32_t *)(r + 8) & 0xFFFu) << 5;
            if (*(void **)(void *)(r + 4) != NULL) {
                uint32_t len = ((flags & 0x0F000000u) == 0x01000000u)
                                   ? 0x20u : 0x200u;
                memcpy(*(void **)(void *)(r + 4), g_brRcaBlob + src, len);
            }
        }
    }
    BrGbiCall10075330(*(void **)(void *)(r + 4));
}

