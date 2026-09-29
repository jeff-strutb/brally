/* br_fade.c -- drawing: the screen fade.
 *
 * RESPONSIBILITY: drawing/ -- turn geometry and images into pixels.
 *
 * Filed out of the address batches, which are not modules.  The fade is a
 * single level driven towards a target: this file collects the step that
 * walks it down, the three calls that set a target, and the three tests that
 * ask whether the fade is moving, settled or fully closed.
 *
 * slice2_16.c's preamble is carried over verbatim.  It looks far larger than
 * this file needs, and it is kept anyway: an include set that looks
 * redundant has already been shown elsewhere in this module to move VC5's
 * register allocation (see br_rdpmode.c), and the fade renames in it are
 * what let the matching bodies define the original's no-pointer signatures
 * while other translation units keep calling the port's.
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

/* The routines this file and br_dl.c BOTH used to transcribe.  Same original
 * function, one host body -- see br_dlshared.h. */
#include "br_dlshared.h"

#include <string.h>
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



extern int DAT_104abb20;
extern float DAT_106e9d8c;
extern float _DAT_104abb24;
extern float kF300_S_S537;

/* WHAT IT DOES: fade a level down by one step per call and, once it reaches
 * the floor, snap it to zero and drop the pointer that was driving it. The
 * tail end of a fade-out. */
/* @implements 0x10014CB0 glide FUN_10014cb0 */
/* @n64 0x8021F1A4 located */
/* auto-filed from ghidra --refine; transforms: kF300_S_S537:float */

void FUN_10014cb0(void)

{
  if ((_DAT_104abb24 != kF300_S_S537) &&
     (_DAT_104abb24 = _DAT_104abb24 - DAT_106e9d8c, _DAT_104abb24 <= kF300_S_S537)) {
    _DAT_104abb24 = 0.0;
    DAT_104abb20 = 0;
  }
  return;
}


/* 0x1002B130 */
/* WHAT IT DOES: aims the screen wipe at a new position, to be reached over
 * the given time. Going forward it simply sets the target and speed; going
 * backward it may instead flag a bounce, so a wipe already on its way out
 * reverses when it lands rather than restarting. Note the time is a duration
 * and is divided into, so asking for zero time gives an infinite speed. */
/* @implements 0x1002B130 d3d BrFadeSetTarget */
/* @implements 0x100181A0 glide BrFadeSetTarget */

/* 0x1002B1C0 */
/* WHAT IT DOES: aims one of the two independent brightness ramps at a new
 * value over the given time, choosing to climb or fall depending on which
 * side of the target it is currently on. As with the wipe, a zero duration
 * gives an infinite rate. */
/* @implements 0x1002B1C0 d3d BrFadeSetTargetA */
/* @implements 0x10018230 glide BrFadeSetTargetA */

/* 0x1002B220 */
/* WHAT IT DOES: the same as the ramp above, for the second of the two
 * independent brightness ramps. */
/* @implements 0x1002B220 d3d BrFadeSetTargetB */
/* @implements 0x10018290 glide BrFadeSetTargetB */

/* 0x1002B2A0 */
/* WHAT IT DOES: reports whether the screen transition is on its way closed
 * -- either currently moving backward, or flagged to reverse when it lands. */
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

/* WHAT IT DOES: reports whether the screen transition is currently heading
 * TOWARDS covering the screen -- fading down rather than up -- or is set to
 * bounce back and do so. Callers use it to hold off on anything that would be
 * hidden a moment later.
 *
 * The negated comparison is load-bearing: an unordered (NaN) rate answers
 * yes, matching the original's `test ah,1` on the compare flags. */
/* @implements 0x1002B2A0 d3d BrFadeIsClosing */
/* @implements 0x10018310 glide BrFadeIsClosing */

/* 0x1002B2D0 */
/* WHAT IT DOES: reports whether the screen transition has finished moving
 * and has no reversal pending, which is how the game knows it can proceed to
 * whatever the transition was covering. */
/* @implements 0x1002B2D0 d3d BrFadeIsSettled */
/* @implements 0x10018340 glide BrFadeIsSettled */

/* 0x1002B300 */
/* WHAT IT DOES: reports whether the screen transition is fully closed:
 * moving backward, arrived at zero, and with no reversal pending. */
/* @implements 0x1002B300 d3d BrFadeIsShut */
/* @implements 0x10018370 glide BrFadeIsShut */
