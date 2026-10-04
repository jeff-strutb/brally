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

/* slice3_42.h declares this cdecl; the original is thiscall with one stack
 * argument.  Hide the prototype so the matching body can carry the
 * __fastcall shape with a struct-typed second argument (never
 * register-eligible, so it cannot claim edx). */
#define BrCtrlCfgLoadDefaults BrCtrlCfgLoadDefaults_cdecl
#define BrFn10069BC0          BrFn10069BC0_cdecl
#define BrFn10069C30          BrFn10069C30_cdecl
#include "slice3_42.h"
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
/* @implements 0x1006B510 d3d BrRbVelAtPoint */
/* BrS42VelAt returns a BrVec3, so MSVC will not inline it and the original
 * has no call there: the body is spelled out here.  The spill map in
 * BrS42VelAt's banner above says why the products and differences stay in
 * x87 registers. */
void BrRbVelAtPoint(BrVec3 *pOut, const BrRbBodyFull *pB, const BrVec3 *pPoint)
{
    /* Three source facts give the original's bytes:
     *   - the point is copied FIELD-WISE (all three loads, then all three
     *     stores); `p = *pPoint` interleaves them;
     *   - the cross product is a block-local BrVec3 c.  That gives the six
     *     hoisted `fld`s in r.x, r.z, r.y order and the fxch ladder, and c
     *     takes p's frame bytes once p is dead (VC5 packs locals whose
     *     lifetimes do not overlap), so the one homed component, c.y, lands
     *     at [esp+0x14].  Three float temps put it in the dead pPoint
     *     parameter slot instead. */
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
        BrVec3 c;
        c.x = r.z * pB->angVel.y - r.y * pB->angVel.z;
        c.y = r.x * pB->angVel.z - r.z * pB->angVel.x;
        c.z = r.y * pB->angVel.x - r.x * pB->angVel.y;
        pOut->x = c.x + pOut->x;
        pOut->y = c.y + pOut->y;
        pOut->z = c.z + pOut->z;
    }
}

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
/* @implements 0x1006B430 d3d BrRbVelAtBodyPoint */
/* @n64 0x80267410 located */
/* Same inlining and the same three float facts as BrRbVelAtPoint above --
 * BrS42VelAt returns a BrVec3 and so is never inlined; the velocity copy is
 * field-wise; the products put the rotated point first and stay float. */
void BrRbVelAtBodyPoint(BrVec3 *pOut, const BrRbBodyFull *pB,
                        const BrRbBodyFull *pAt)
{
    BrVec3 r;
    BrVec3 p;

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

    /* A block-local BrVec3, as in BrRbVelAtPoint: c reuses p's dead frame
     * bytes, so the homed c.y goes to [esp+0x14], not the pAt slot. */
    {
        BrVec3 c;
        c.x = r.z * pB->angVel.y - r.y * pB->angVel.z;
        c.y = r.x * pB->angVel.z - r.z * pB->angVel.x;
        c.z = r.y * pB->angVel.x - r.x * pB->angVel.y;
        pOut->x = c.x + pOut->x;
        pOut->y = c.y + pOut->y;
        pOut->z = c.z + pOut->z;
    }
}

/* 0x1006B340 */
/* WHAT IT DOES: the same again, except the attachment point is flattened --
 * its height is ignored -- and the answer comes back measured against the
 * world rather than against the body. The caller must not pass the same
 * storage in twice, because the answer slot is used as scratch on the way. */
/* @implements 0x1006B340 d3d BrRbVelAtBodyPointXY */
/* Same inlining and the same cross product as the two above.  The sums go
 * back into p, the transform's INPUT (the original's closing fstp triple
 * and the pointer passed to BrMat4MulVec3 are both at p's slot), and vel.z
 * is held in a float local: VC5 then copies it to pOut->z through the x87
 * stack and reuses that register for the z sum, where x and y are plain
 * dword moves read back from *pOut.  Graded /O2: with /Op every one of
 * those register values would be spilled. */
void BrRbVelAtBodyPointXY(BrVec3 *pOut, const BrRbBodyFull *pB,
                          const BrRbBodyFull *pAt)
{
    BrVec3 p;
    BrVec3 r;
    float cx, cy, cz;
    float vz;

    p.x = pAt->f78.x;
    p.y = pAt->f78.y;
    p.z = 0.0f;                 /* the original stores a literal 0 dword */

    BrMat4MulVec3Transposed(&r, &pB->m, &p);

    pOut->x = pB->vel.x;
    pOut->y = pB->vel.y;
    vz = pB->vel.z;
    pOut->z = vz;

    cx = r.z * pB->angVel.y - r.y * pB->angVel.z;
    cy = r.x * pB->angVel.z - r.z * pB->angVel.x;
    cz = r.y * pB->angVel.x - r.x * pB->angVel.y;

    p.x = cx + pOut->x;
    p.y = cy + pOut->y;
    p.z = cz + vz;

    BrMat4MulVec3(pOut, &pB->m, &p);
}
