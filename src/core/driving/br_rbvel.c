/* br_rbvel.c -- driving: rigid-body point velocity.
 *
 * The velocity of a given spot on a moving, spinning rigid body (linear
 * velocity plus angular velocity crossed with the body-space lever):
 * BrRbVelAtPoint (glide 0x100644C0) for an explicit point,
 * BrRbVelAtBodyPoint (glide 0x100643E0) for another body's attachment point,
 * and BrRbVelAtBodyPointXY for the flattened attachment point, answered in
 * world space.  BrS42VelAt is the port arm's shared body.
 *
 * Filed out of the address batch slice3_42.c, with that file's preamble.
 */

#include <string.h>

#ifdef BR_MATCHING_BUILD
/* slice3_42.h declares this cdecl; the original is thiscall with one stack
 * argument.  Hide the prototype so the matching body can carry the
 * __fastcall shape with a struct-typed second argument (never
 * register-eligible, so it cannot claim edx). */
#define BrCtrlCfgLoadDefaults BrCtrlCfgLoadDefaults_cdecl
#define BrFn10069BC0          BrFn10069BC0_cdecl
#define BrFn10069C30          BrFn10069C30_cdecl
#endif
#include "slice3_42.h"
#ifdef BR_MATCHING_BUILD
#undef BrCtrlCfgLoadDefaults
#undef BrFn10069BC0
#undef BrFn10069C30
typedef struct { int32_t v; } BrCtrlProfileArg;
/* BOTH stack arguments are struct-wrapped. __fastcall skips a struct when it
 * hands out ecx/edx, so wrapping only the FIRST of them lets the SECOND take
 * edx and the function cleans 4 bytes instead of 8. Wrapping both leaves ecx
 * for `this` and puts the pair on the stack, which is thiscall exactly. */
typedef struct { int32_t v; } BrCtrlKindArg;
typedef struct { uint32_t v; } BrCtrlKeyArg;
#endif

/* =====================================================================
 * .rdata constants, read out of orig/BRD3D.dll rather than assumed.
 * ===================================================================== */

#define BR_K_0008FA54   0.0f    /* 0x1008FA54 */
#define BR_K_0008FA58   2.0f    /* 0x1008FA58 */
#define BR_K_0008FA5C   1.0f    /* 0x1008FA5C */
#define BR_K_0008FAA8  30.0f    /* 0x1008FAA8 -- the simulation rate */

/* The body of 0x1006B430 and 0x1006B510, which differ only in where the
 * point comes from. */
static BrVec3 BrS42VelAt(BrVec3 *pOut, const BrRbBodyFull *pB, const BrVec3 *pP)
{
    BrVec3 r, sum;
    double cx, cy, cz;

    BrMat4MulVec3Transposed(&r, &pB->m, pP);

    /* All three routines write pB->vel into *pOut and then read it back to
     * form the sum.  In 0x1006B340 that intermediate is the FINAL content of
     * pOut until the closing matrix multiply overwrites it, so the store is
     * kept here rather than folded away. */
    *pOut = pB->vel;

    /* SPILL MAP, checked in all three callers -- 0x1006B510, 0x1006B430 and
     * 0x1006B340 -- because a shared C body is only honest if the bodies it
     * stands for agree:
     *
     *   r comes back from 0x10074770 through a stack BrVec3, so it IS
     *   float-rounded, and reading it out of `r` here reproduces that.
     *
     *   The six products and three differences of the cross stay in x87
     *   registers.  The ONLY store among them is `fst dword [esp+0x14]` at
     *   1006B5C0 (0x1006B510) and 1006B4E4 (0x1006B430) -- and that slot is
     *   never reloaded, so it rounds nothing; the add at 1006B5CB takes the
     *   register copy `fst` left behind.  0x1006B340 has no such store at
     *   all.  So all three cross components reach their add unrounded.
     *
     *   Each sum is stored exactly once -- straight into pOut for the first
     *   two, into a stack BrVec3 for 0x1006B340 -- so it rounds to float
     *   there and nowhere earlier.  Returning a BrVec3 by value is that
     *   store.
     *
     * This is why the cross is written out here instead of calling
     * BrS42Cross: that helper returns a BrVec3, which would round all three
     * components a step early.  BrS42Cross is left alone because its other
     * caller (BrRbAccumOwnForces, 0x1006AEB0) has not been traced for spill
     * points, and widening it on the strength of this function's evidence
     * would be assuming the answer for a function nobody has read. */
    cx = (double)pB->angVel.y * (double)r.z
       - (double)pB->angVel.z * (double)r.y;
    cy = (double)pB->angVel.z * (double)r.x
       - (double)pB->angVel.x * (double)r.z;
    cz = (double)pB->angVel.x * (double)r.y
       - (double)pB->angVel.y * (double)r.x;

    sum.x = (float)(cx + (double)pOut->x);
    sum.y = (float)(cy + (double)pOut->y);
    sum.z = (float)(cz + (double)pOut->z);
    return sum;
}

/* 0x1006B510 */
/* WHAT IT DOES: answers how fast one particular spot on a moving, spinning
 * body is travelling -- which is not the same as how fast the body is
 * travelling, because a spinning body drags its edges along faster than its
 * middle. The spot is given directly. */
/* @t4-pass 0x100644C0 1 2026-09-24 probes 15 bytes 214 insns 68 regions 0 rows 0 census no  (hand, after the field-wise copy + block-temps retranscription reached 214/214: spill-slot probes -- declaration order, float[3] locals, parameter copy -- and the six temp/add orders; none moved the spill) */
/* @t4-pass 0x100644C0 2 2026-09-24 probes 12 bytes 214 insns 68 regions 0 rows 0 census yes  (hand, slot census: the function moved to every top-level slot of slice3_42.c; residue identical in all 12 that compile) */
/* @t3 0x100644C0 2026-09-24 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 214/214 insns 68/68 rows 0+0 regions 0 oracle EQUIVALENT
 * @t3-effort passes 2 zero-movement 1 2
 * residue is ONE spill-slot choice: the single x87 spill goes to the dead
 * pPoint parameter slot ([esp+0x24]) where the original uses p.z's slot
 * ([esp+0x14]); every instruction is otherwise identical.  Dossier and dead
 * list are in the body comment below.
 * Do not reopen before the end-grind (CLAUDE.md rule 12). */
/* @implements 0x1006B510 d3d BrRbVelAtPoint */
#ifdef BR_MATCHING_BUILD
/* BrS42VelAt RETURNS A BrVec3, so MSVC will not inline it and the original
 * has no call there -- the whole 137-byte gap is one factored helper. The
 * body is spelled out here; the spill map in BrS42VelAt's banner above is
 * what says the six products and three differences stay in x87 registers.
 *
 * RESIDUE (22 regnorm, -32 bytes, x87 SCHEDULING): the original loads all
 * SIX r components onto the x87 stack up front and interleaves the three
 * cross terms through them -- 16 `fxch` and one `fst` that is never reloaded.
 * Ours evaluates the three in sequence. Was 137 bytes short and 43 regnorm
 * before the helper came inline.
 *
 * DEAD PROBES, do not re-run:
 *  - assigning the three sums straight into *pOut with no temps (worse, -40);
 *  - reordering the three cross terms AND the three adds to y, z, x, which is
 *    the order the original's `fsubp`s complete in and the order its six
 *    `fld`s pair up in -- 5+22 regnorm becomes 7+24, slightly WORSE
 *    (2026-09-03).
 *
 * ‼ AND THE REASON THE ORDER DOES NOT HELP IS NOW UNDERSTOOD. The six `fld`s
 * are hoisted above the `add esp,0xC` that cleans the call's three arguments:
 * once esp moves, every `[esp+N]` displacement for `r` changes, so MSVC loads
 * all six uses of r BEFORE adjusting the stack and then shuffles them with 16
 * `fxch` -- plus one `fld st(2)` duplicate and one dead `fst`. That is a
 * consequence of where the CALL's cleanup sits, not of how the arithmetic is
 * spelled, which is why every term ordering leaves it unchanged. A source
 * lever here would have to move the stack cleanup, not the expressions.
 *
 * ‼ CONFIRMED EXHAUSTIVELY 2026-09-05: all SIX permutations of the three
 * cross-term statements x each of the two add orders (x,y,z and the
 * original's completion order y,z,x) -- thirteen builds -- land between
 * 4+23 and 5+24 register-blind, none better than the 5+22 here, and none
 * closer than 32 bytes.  The original's six `fld`s read r.x, r.z, r.y,
 * r.x, r.z, r.y, which is exactly the use order of `cy, cz, cx`; spelling
 * that order changes nothing, which is the proof that the load block is
 * emitted by the stack-cleanup hoist and not by the statement order.
 * DO NOT PROBE TERM ORDER ON THIS FUNCTION AGAIN. */
void BrRbVelAtPoint(BrVec3 *pOut, const BrRbBodyFull *pB, const BrVec3 *pPoint)
{
    /* 2026-09-24 re-transcription.  Two source facts took this from
     * 5+22 regnorm to 0+0 (1 byte):
     *   - the point is copied FIELD-WISE (all three loads, then all three
     *     stores); `p = *pPoint` interleaves them;
     *   - the cross terms are three block-scoped float temps x, y, z in that
     *     order.  That alone gives the original's six hoisted `fld`s in its
     *     r.x, r.z, r.y order and its fxch ladder -- the "stack-cleanup hoist"
     *     notes above were chasing the spelling of `p`, not the arithmetic.
     * RESIDUE 1 byte: the one x87 spill goes to the dead pPoint parameter
     * slot ([esp+0x24]); the original puts it in p.z's slot ([esp+0x14]).
     * Declaration order, array locals and a parameter copy do not move it. */
    BrVec3 p;
    BrVec3 r;

    p.x = pPoint->x;
    p.y = pPoint->y;
    p.z = pPoint->z;
    BrMat4MulVec3Transposed(&r, &pB->m, &p);

    /* FIELD-WISE, not `*pOut = pB->vel` (a `lea` base pointer costs edi). */
    pOut->x = pB->vel.x;
    pOut->y = pB->vel.y;
    pOut->z = pB->vel.z;

    /* v + w x r, r first with angVel as the memory operand */
    {
        float x = r.z * pB->angVel.y - r.y * pB->angVel.z;
        float y = r.x * pB->angVel.z - r.z * pB->angVel.x;
        float z = r.y * pB->angVel.x - r.x * pB->angVel.y;

        pOut->x = x + pOut->x;
        pOut->y = y + pOut->y;
        pOut->z = z + pOut->z;
    }
}
#else
void BrRbVelAtPoint(BrVec3 *pOut, const BrRbBodyFull *pB, const BrVec3 *pPoint)
{
    BrVec3 p = *pPoint;         /* the original copies it to a stack slot */
    *pOut = BrS42VelAt(pOut, pB, &p);
}
#endif

/* 0x1006B430 */
/* WHAT IT DOES: the same question, but about the spot belonging to another
 * body -- how fast is this body moving at the place where that one is
 * attached. */
/* @t4-pass 0x100643E0 1 2026-09-10 probes 24 bytes 218 insns 68 regions 0 rows 0 census no  (tools/crank.py) */
/* @t4-pass 0x100643E0 2 2026-09-10 probes 24 bytes 218 insns 68 regions 0 rows 0 census no  (tools/crank.py) */
/* @t4-pass 0x100643E0 3 2026-09-10 probes 24 bytes 218 insns 68 regions 0 rows 0 census no  (tools/crank.py) */
/* @t4-pass 0x100643E0 4 2026-09-10 probes 24 bytes 218 insns 68 regions 0 rows 0 census no  (tools/crank.py) */
/* @t4-pass 0x100643E0 5 2026-09-10 probes 11 bytes 218 insns 68 regions 0 rows 0 census yes
 * BYTE CENSUS, not a mutation sweep: with the attachment point copied
 * field-wise the whole function is byte-identical except ONE byte -- the
 * displacement of the rounding spill at +0xb4, `fst [esp+0x14]` in the
 * original against `fst [esp+0x24]` here.  Both frames are `sub esp,0x18`
 * and both spill the same value (cy, rounded before the add, per this
 * file's float-precision rule); the original puts it in the frame, VC5
 * puts it in the dead incoming-argument slot.  Eleven probes, none moved
 * it: six declaration orders (floats first / between / last, p before r,
 * one declarator per line), the sum statements reversed and written
 * destination-first, a spare float local for frame pressure, and a
 * volatile int to hold a slot.  Slot assignment only. */
/* @t3 0x100643E0 2026-09-10 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 218/218 insns 68/68 rows 0+0 regions 0 oracle UNCLASSIFIED
 * @t3-effort passes 5 zero-movement 4 5
 * Residue: ONE byte, the stack slot of the rounding spill at +0xb4 --
 * `fst [esp+0x14]` in the original against `fst [esp+0x24]` here, VC5
 * placing cy in the dead incoming-argument slot instead of the frame.
 * Dead list and the byte census are in the @t4-pass 5 line above.
 * Do not reopen before the end-grind (CLAUDE.md rule 12). */
/* @implements 0x1006B430 d3d BrRbVelAtBodyPoint */
/* @n64 0x80267410 located */
#ifdef BR_MATCHING_BUILD
/* Same inlining and the same three float facts as BrRbVelAtPoint above --
 * BrS42VelAt returns a BrVec3 and so is never inlined; the velocity copy is
 * field-wise; the products put the rotated point first and stay float.
 *
 * RESIDUE (23 regnorm, -33 bytes): the same x87 scheduling as
 * BrRbVelAtPoint -- the original holds all six products on the stack at once
 * and interleaves the three terms through them; ours evaluates in sequence.
 * Was 138 bytes short before the helper came inline. */
void BrRbVelAtBodyPoint(BrVec3 *pOut, const BrRbBodyFull *pB,
                        const BrRbBodyFull *pAt)
{
    BrVec3 r;
    BrVec3 p;
    float cx, cy, cz;

    /* FIELD-WISE, not a struct copy: the original reads [pAt+0x78/0x7c/0x80]
     * directly, where `p = pAt->f78` makes VC5 build the address first
     * (`add R,0x78`) and copy from [M]/[M+4]/[M+8].  Same spelling as
     * BrRbVelAtBodyPointXY below. */
    p.x = pAt->f78.x;
    p.y = pAt->f78.y;
    p.z = pAt->f78.z;

    BrMat4MulVec3Transposed(&r, &pB->m, &p);

    pOut->x = pB->vel.x;
    pOut->y = pB->vel.y;
    pOut->z = pB->vel.z;

    cx = r.z * pB->angVel.y - r.y * pB->angVel.z;
    cy = r.x * pB->angVel.z - r.z * pB->angVel.x;
    cz = r.y * pB->angVel.x - r.x * pB->angVel.y;

    pOut->x = cx + pOut->x;
    pOut->y = cy + pOut->y;
    pOut->z = cz + pOut->z;
}
#else
void BrRbVelAtBodyPoint(BrVec3 *pOut, const BrRbBodyFull *pB,
                        const BrRbBodyFull *pAt)
{
    BrVec3 p = pAt->f78;
    *pOut = BrS42VelAt(pOut, pB, &p);
}
#endif

/* 0x1006B340 */
/* WHAT IT DOES: the same again, except the attachment point is flattened --
 * its height is ignored -- and the answer comes back measured against the
 * world rather than against the body. The caller must not pass the same
 * storage in twice, because the answer slot is used as scratch on the way. */
/* @implements 0x1006B340 d3d BrRbVelAtBodyPointXY */
#ifdef BR_MATCHING_BUILD
/* Same inlining and the same three float facts, except the sum goes to a
 * stack BrVec3 rather than into *pOut, because the closing matrix multiply
 * reads it. BrS42VelAt's banner records that this one has no `fst` at all. */
void BrRbVelAtBodyPointXY(BrVec3 *pOut, const BrRbBodyFull *pB,
                          const BrRbBodyFull *pAt)
{
    /* TWO stack vectors, `sub esp,0x18`, not three: the sums go back into
     * `r` -- the original's closing `fstp` triple targets the very slot the
     * transform wrote -- and there is no separate sum variable.
     *
     * RESIDUE (6 regnorm, +10 bytes): the two vectors are SWAPPED in the
     * frame. The original puts the transform's INPUT at the deeper slot
     * (E-0x18) and its output at E-0xC; ours does the reverse, and every
     * displacement follows. Every instruction is otherwise in place, 72
     * against 73. Probed and dead: swapping the declarations, and renaming
     * both locals twice -- the /Od name-hash homing recorded in
     * BrCarGfxReadColour does not apply at /O2.
     *
     * MORE DEAD, 2026-09-05, all scored with an /O2 /Op compile (fn.py's
     * /O2-only diff is phantom on this TU): four declaration orders
     * including the floats first and between the vectors (all byte-
     * identical, 146); reading BOTH vectors out of ONE `BrVec3 v[2]` or an
     * anonymous struct, which is what the original's 12-byte slot spacing
     * looks like -- v[0] as the transform input gets 141, v[1] as the input
     * gets 146, so the array DOES reach the layout question but does not
     * settle it; and re-reading `pB->vel.z` (142) or all three vel
     * components (143) at the sum instead of reading `*pOut` back, which is
     * what the original's `fadd st(3)` against a duplicated `fld st(1)`
     * hints at.  Nothing has beaten 141 and everything is still 4 bytes
     * short.  The next idea has to explain the ONE duplicated x87 copy of
     * vel.z, not the slot order. */
    BrVec3 p;
    BrVec3 r;
    float cx, cy, cz;

    p.x = pAt->f78.x;
    p.y = pAt->f78.y;
    p.z = 0.0f;                 /* the original stores a literal 0 dword */

    BrMat4MulVec3Transposed(&r, &pB->m, &p);

    pOut->x = pB->vel.x;
    pOut->y = pB->vel.y;
    pOut->z = pB->vel.z;

    cx = r.z * pB->angVel.y - r.y * pB->angVel.z;
    cy = r.x * pB->angVel.z - r.z * pB->angVel.x;
    cz = r.y * pB->angVel.x - r.x * pB->angVel.y;

    r.x = cx + pOut->x;
    r.y = cy + pOut->y;
    r.z = cz + pOut->z;

    BrMat4MulVec3(pOut, &pB->m, &r);
}
#else
void BrRbVelAtBodyPointXY(BrVec3 *pOut, const BrRbBodyFull *pB,
                          const BrRbBodyFull *pAt)
{
    BrVec3 p, sum;

    p.x = pAt->f78.x;
    p.y = pAt->f78.y;
    p.z = 0.0f;                 /* the original stores a literal 0 dword */

    sum = BrS42VelAt(pOut, pB, &p);
    BrMat4MulVec3(pOut, &pB->m, &sum);
}
#endif
