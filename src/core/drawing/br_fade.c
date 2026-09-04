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


#ifdef BR_MATCHING_BUILD

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

#endif /* BR_MATCHING_BUILD */

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
