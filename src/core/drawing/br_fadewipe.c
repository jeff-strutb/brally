/* br_fadewipe.c -- drawing: the screen-transition wipe.
 *
 * RESPONSIBILITY: drawing/ -- turn geometry and images into pixels.
 *
 * The fade's hold count and latch (BrFadeRelease, BrFadeLatch), the solid
 * wipe bars (BrFadeDrawBars), and the per-frame step that moves the wipe and
 * the two brightness ramps (BrFadeTick, which owns the fade/wipe globals).
 *
 * ALSO HERE, and not in gamedata/ where it belongs by what it does: the .rca
 * texture-record fixup (BrRcaSwapMesh, BrRcaFixupRecord and the blob /
 * segment-base globals).  They followed the fade in the address batch and
 * are byte-identical only with this file's fade code in front of them:
 * probed at the refile, every smaller prefix (the fade globals, the ramp
 * helper, the bars, the tick, each alone) and a file of their own moved both
 * bodies.  Refile them only by re-winning them.
 *
 * Filed out of the address batch slice2_16.c; its preamble is carried over
 * verbatim.  See slice2_16.h for the per-function notes and gotchas.
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
/* @t4-pass 0x100183B0 1 2026-09-07 probes 80 bytes 800 insns 216 regions 4 rows 12 census yes  (tools/crank.py) */
/* @t4-pass 0x100183B0 2 2026-09-07 probes 80 bytes 800 insns 216 regions 4 rows 12 census yes  (tools/crank.py) */
/* @t4-pass 0x100183B0 3 2026-09-10 probes 30 bytes 800 insns 220 regions 6 rows 2 census yes  (tools/crank.py) */
/* @t4-pass 0x100183B0 4 2026-09-10 probes 30 bytes 800 insns 220 regions 6 rows 2 census yes  (tools/crank.py) */
/* @t3 0x100183B0 2026-09-10 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 800/803 insns 220/222 rows 2+0 regions 6 oracle UNCLASSIFIED
 * @t3-effort passes 4 zero-movement 3 4
 * RESIDUE: two register copies, nothing else -- the whole multiset pairs
 * and the instruction gap is 2.  The three levers that closed the rest are
 * in the body: the bar's top edge is a local carried in from an earlier
 * block (a literal 0 folds the original's four-instruction zero shift
 * away), bars is read into a local whose decrement writes back, and pos2
 * reaches the bar command from the local the test already loaded.
 * 30 compiles in the last pass, levers accepted: none; every candidate and
 * score is in build/match/crank.log.
 * Do not reopen before the end-grind (CLAUDE.md rule 12). */
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
    int32_t v, n, ps;
    /* `top` is the bar's top edge, and it carries the ZERO the second arm
     * shifts.  Spelled as a literal 0 (or a 0u, or a cast, or a local
     * initialised in that arm) VC5 folds the whole `((0 << shift) & 0xFFF)
     * << 12` away to a `mov [eax+4],0`; carried in from an earlier BLOCK it
     * does not fold, and the original's four instructions -- xor / shl cl /
     * and 0xfff / shl 0xc, four instructions to produce a zero it already
     * has -- come back. */
    int32_t top = 0;

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

    /* The compare's left operand is NAMED: the original loads pos2 into a
     * register and compares register to register, where the memory operand
     * spelling gives `cmp [g],reg`.  Naming the other two tests' operands is
     * inert -- VC5 folds them straight back to memory. */
    v = DAT_104b16b0;
    if (v != 0) {
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
        /* pos2 comes from the local, not a re-read: the original keeps it
         * in a callee-saved register across BrFadeIsShut. */
        p->w0 = BR16_BAR_W0(v, DAT_106e9a2c, DAT_106ed674);
    }

    /* Both counters are read ONCE and the decrement writes the local back:
     * the original loads bars into a register, tests it against the zero it
     * keeps in a callee-saved register, and decrements the copy.  `bars -= 1`
     * on the global re-reads it and compares against memory instead. */
    ps = DAT_104b16a8;
    if (ps < DAT_106e7714) {
        n = DAT_100a7510;
        if (n != 0) {
            p = BR16_ALLOC();
            DAT_100a7510 = n - 1;
            p->w0 = BR16_BAR_W0(DAT_106e7714, DAT_106e9a2c, DAT_106ed674);
            top = ps;
            p->w1 = (uint32_t)(((uint32_t)top << DAT_106ed674) & 0xFFFu) << 12;
        }
    } else if (DAT_104b16c0 == 0.0f && (n = DAT_100a7510) != 0) {
        p = BR16_ALLOC();
        DAT_100a7510 = n - 1;
        p->w0 = BR16_BAR_W0(DAT_106e7714, DAT_106e9a2c, DAT_106ed674);
        /* top is still 0 here: the same expression, and the same four
         * instructions the original spends on it. */
        p->w1 = (uint32_t)(((uint32_t)top << DAT_106ed674) & 0xFFFu) << 12;
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
/* @t4-pass 0x100186E0 1 2026-09-07 probes 54 bytes 685 insns 165 regions 1 rows 0 census yes  (tools/crank.py) */
/* @t4-pass 0x100186E0 2 2026-09-07 probes 54 bytes 685 insns 165 regions 1 rows 0 census yes  (tools/crank.py) */
/* @t3 0x100186E0 2026-09-09 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 685/685 insns 165/165 rows 0+0 regions 1 oracle UNCLASSIFIED
 * @t3-effort passes 2 zero-movement 1 2
 * residue is one esi/edi role toggle in the backward-wipe arm (see the
 * NOT MATCHING note above); identical multiset, size-exact; two crank
 * census passes at these numbers.
 * Do not reopen before the end-grind (CLAUDE.md rule 12). */
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


/* 0x1002B9E0 has moved to br_bits.c, which carries both builds' addresses:
 * br_track.c had transcribed the same function as `swap_u16_run` under
 * BRGlide's 0x10018A50.  br_bits is a leaf both can link. */

/* 0x1002BC90 */
/* WHAT IT DOES: turns a loaded mesh header the right way round: its entry
 * count, one following word, and then three words for each entry. Note the
 * entry count is re-read from the header on every pass of the loop, exactly
 * as the original does, so swapping it can change how many entries get
 * processed. */
/* @t4-pass 0x10018D50 1 2026-09-07 probes 58 bytes 180 insns 71 regions 5 rows 0 census yes  (tools/crank.py) */
/* @t4-pass 0x10018D50 2 2026-09-07 probes 58 bytes 180 insns 71 regions 5 rows 0 census yes  (tools/crank.py) */
/* @t3 0x10018D50 2026-09-09 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 180/180 insns 71/71 rows 0+0 regions 5 oracle UNCLASSIFIED
 * @t3-effort passes 2 zero-movement 1 2
 * residue is register colouring only: identical register-blind instruction
 * multiset (rows 0+0), 5 masked regions;
 * every row pairs under t3.py's canonical classes.  Effort: 2 counted
 * @t4-pass passes (ledger lines above, zero movement on passes 1 and 2);
 * crank candidates and scores in build/match/crank.log, dead probes in the
 * comment block above.  Do not reopen before the end-grind (CLAUDE.md rule 12). */
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
/* @t4-pass 0x10018B60 1 2026-09-07 probes 77 bytes 492 insns 165 regions 2 rows 0 census yes  (tools/crank.py) */
/* @t4-pass 0x10018B60 2 2026-09-07 probes 77 bytes 492 insns 165 regions 2 rows 0 census yes  (tools/crank.py) */
/* @t3 0x10018B60 2026-09-09 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 492/493 insns 165/165 rows 0+0 regions 2 oracle UNCLASSIFIED
 * @t3-effort passes 2 zero-movement 1 2
 * residue is register colouring only: identical register-blind multiset
 * (rows 0+0), 1 byte of encoding shadow, 2 masked regions; two crank
 * census passes at these numbers.
 * Do not reopen before the end-grind (CLAUDE.md rule 12). */
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

/* 0x10028820 BrGbiTexScanRun's Glide arm is in drawing/br_gbitexscan.c. */
