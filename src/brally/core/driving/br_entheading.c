/* br_entheading.c -- driving: turning an entity to a compass heading.
 *
 * BrEntSetHeading (glide 0x1006F720) points a car or other world object in a
 * given direction about the vertical, writing the new orientation (matrix
 * and quaternion) into every copy of the state the physics keeps, then
 * rebuilds its world matrix.
 *
 * Filed out of the address batch slice3_45.c, with that file's preamble and
 * the two .rdata constants only this function uses.
 */

#include <math.h>
#include <string.h>

#include "br_match.h"
/* Header is cdecl (this, x, y, z). Original is thiscall with ret 0xC. */
#define BrEntSetPos BrEntSetPos_hdr
/* The entity setters are thiscall with three stack floats; hide the
 * port's cdecl prototypes so the twins can carry the fastcall shape. */
#define BrEntSetMatrix      BrEntSetMatrix_port
#define BrEntSetVel         BrEntSetVel_port
#define BrEntSetAngVel      BrEntSetAngVel_port
#define BrEntSetOrientation BrEntSetOrientation_port
#define BrEntSetHeading     BrEntSetHeading_port
#include "slice3_45.h"
#undef BrEntSetMatrix
#undef BrEntSetVel
#undef BrEntSetAngVel
#undef BrEntSetOrientation
#undef BrEntSetHeading
#undef BrEntSetPos

/* ====================================================================== */
/* Constants read out of reference/brally/orig/BRD3D.dll .rdata (do not re-derive)          */
/* ====================================================================== */

/* 0x1008FCA4 = 0xBFC90FDB. The float nearest -pi/2. BrEntSetHeading
 * SUBTRACTS it, i.e. adds pi/2. */
static const float kBrNegHalfPi = -1.5707963705062866f;

/* 0x1008FCA8 = 0x3F000000, exactly 0.5. The quaternion half-angle factor. */
static const float kBrHalf = 0.5f;

/* ====================================================================== */
/* 1. Entity state setters                                                 */
/* ====================================================================== */

/* 0x100764C0 */
/* WHAT IT DOES: points a car (or other object in the world) in a given
 * compass direction, keeping it upright -- it can only turn about the
 * vertical, not tip or roll. It writes the new facing into every copy of the
 * object's state the physics keeps, so nothing is left pointing the old way. */
/* @t4-pass 0x1006F720 1 2026-09-09 probes 10 bytes 279 insns 74 regions 1 rows 0 census no  (hand, fn.py variants: decl/store orders, dword-pun zeros, chain forms, h placement, all inert or worse) */
/* @t4-pass 0x1006F720 2 2026-09-09 probes 10 bytes 279 insns 74 regions 1 rows 0 census yes  (hand, fn.py variants: store-order swaps, temps, mul order, q-decl forms, all inert or worse; corpus MISS at +0x30 len 12 -- the stores-before-fstp schedule is proven nowhere) */
/* @implements 0x100764C0 d3d BrEntSetHeading */
/* thiscall + one stack float (ret 4); sin/cos are the float-arg tree
 * wrappers, as in BrEntSetOrientation below.
 *
 * The original keeps sb on the FPU stack across the c/s/0 stores and pops
 * it into m[1][1] after them: m[1][1] is written through a pointer of its
 * own, which VC5 will not schedule ahead of the stores through pE (written
 * through pE, the pop lands at the call return).  The plain float copies
 * become integer movs, the z triple-store is a chained assignment
 * (fst/fst/fstp), and qw/qx/qy reload as dword puns after the second
 * half-angle call. */
extern float BrSinF(float a);      /* glide 0x10002560 */
extern float BrCosF(float a);      /* glide 0x100023E0 */

void __fastcall BrEntSetHeading(BrEnt *pE, float a)
{
    float c  = BrCosF(a);
    float s  = BrSinF(a);
    float b  = a - kBrNegHalfPi;   /* a + pi/2, to the float's precision */
    float cb = BrCosF(b);
    float sb = BrSinF(b);
    float h;
    float *m = &pE->mat0.m[1][1];
    uint32_t qw, qx, qy;

    pE->mat0.m[0][0] = c;
    pE->mat0.m[0][1] = s;
    pE->mat0.m[0][2] = 0.0f;
    *m = sb;
    h = a * kBrHalf;
    pE->mat0.m[1][0] = cb;
    pE->mat0.m[1][2] = 0.0f;
    pE->mat0.m[2][0] = 0.0f;
    pE->mat0.m[2][1] = 0.0f;
    pE->mat0.m[2][2] = 1.0f;

    pE->st.quat.f00 = BrCosF(h);
    pE->st.quat.f04 = 0.0f;
    pE->st.quat.f08 = 0.0f;

    pE->stA.quat.f0C = pE->stB.quat.f0C = pE->st.quat.f0C = BrSinF(h);

    qw = *(uint32_t *)&pE->st.quat.f00;
    qx = *(uint32_t *)&pE->st.quat.f04;
    qy = *(uint32_t *)&pE->st.quat.f08;

    *(uint32_t *)&pE->stB.quat.f00 = qw;
    *(uint32_t *)&pE->stA.quat.f00 = qw;
    *(uint32_t *)&pE->stB.quat.f04 = qx;
    *(uint32_t *)&pE->stB.quat.f08 = qy;
    *(uint32_t *)&pE->stA.quat.f04 = qx;
    *(uint32_t *)&pE->stA.quat.f08 = qy;

    BrRbBuildMatrix(&pE->matrix, &pE->st);
}
