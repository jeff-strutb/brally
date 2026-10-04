/* br_rbinteg.c -- driving: the rigid-body integrator and its 3x3 math.
 *
 * One physics step for a rigid body: BrRbIntegrateVelocity advances the
 * velocities, BrRbIntegrateState advances position and orientation,
 * BrRbQuatDerivative (glide 0x1006D530) turns angular velocity into the
 * orientation quaternion's rate of change, and BrRbBuildMatrix builds the
 * world transform from the result.  The packed 3x3 helpers ahead of them
 * (BrMat3Mul, BrMat3Solve, BrMat4BuildScaledTransposed) are the inertia-tensor
 * math the integrator runs on.
 *
 * Kept as one TU on purpose: BrRbQuatDerivative's x87 operand roles are set
 * by the compilation state its predecessors leave behind, and it only
 * compiles byte-exact with this whole run of functions ahead of it.
 *
 * Filed out of the address batch slice3_44.c, with that file's preamble.
 */

#include <stdlib.h>   /* a TU-size input: see BrMat3Solve's ROUNDING POINTS note */
#include "slice3_44.h"

/* ---- constants ---------------------------------------------------- */

/* ---- cross-slice --------------------------------------------------- */

/* XSLICE 0x10008B80 */
/* A bare `ret` in this build (see CONTRACT).  Name and prototype copied from
 * slice2_18.h so integration can wire it mechanically.  In the matching build
 * it is the tree's BrPodNop (glide 0x10008D60), so the call relocates. */
#define BrStub8B80_1p(p) BrPodNop()   /* 0x10008D60, a bare ret */

/* BrVec4Normalise (0x100741B0) and BrMat4MulVec3Transposed (0x10074770) come
 * in through slice1_09.h / br_mat.h. */

/* ================================================================== */
/* Packed 3x3                                                          */
/* ================================================================== */

/* 0x10074AC0 */
/* WHAT IT DOES: combines two 3x3 rotations into one, so that applying the
 * result does the same as applying both in turn. */
/* @implements 0x10074AC0 d3d BrMat3Mul */
/* @n64 0x80258EF8 located */
void BrMat3Mul(BrMat3 *pOut, const BrMat3 *pA, const BrMat3 *pB)
{
    const float *a = pA->m;
    const float *b = pB->m;
    int          i, j;

    for (i = 0; i < 3; ++i) {
        for (j = 0; j < 3; ++j) {
            /* The sum goes through a float local before the store.  That
             * local, not the term order, is what makes VC5 anchor its
             * strength-reduced row/column pointers on the third term
             * ([a+8], [b+0x18]); storing the sum straight to pOut->m
             * anchors on the second. */
            float s = a[3 * i + 0] * b[j]
                    + a[3 * i + 1] * b[3 + j]
                    + a[3 * i + 2] * b[6 + j];
            pOut->m[3 * i + j] = s;
        }
    }
}

/* A float rounding point VC5 cannot see through: the value goes to memory
 * as a float and comes back through its int image. */
#define BR_SOLVE_ROUND(dst, expr) \
    { float r_ = (expr); int i_ = *(int *)&r_; dst = *(float *)&i_; }

/* The divide reads the image's float 1.0 at 0x10077C1C: with det a double,
 * a literal would become an 8-byte constant the original .rdata does not
 * have. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
#define BR_SOLVE_ONE _DAT_10077c1c
/* WHAT IT DOES: solves the 3x3 system pM * x = pV for x by Cramer's rule and
 * writes x to pOut.  No singularity guard -- a singular matrix yields +-inf or
 * NaN, exactly as the original.  Confirmed equivalent to the original bytes by
 * an x87 emulation of 0x1006DE70 over random and structured inputs. */
/* @t4-pass 0x1006DE70 1 2026-09-07 probes 86 bytes 419 insns 162 regions 1 rows 38 census yes  (tools/crank.py) */
/* @t4-pass 0x1006DE70 2 2026-09-07 probes 86 bytes 419 insns 162 regions 1 rows 38 census yes  (tools/crank.py) */
/* @t3 0x1006DE70 2026-09-27 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 579/417 insns 203/164 rows 48+87 regions 1 oracle EQUIVALENT
 * @t3-effort passes 6 zero-movement 5 6
 * Residue: the body is written for the original's ROUNDING POINTS (see
 * the note in the body), not for its bytes; the float body it replaced was
 * 419/417 B but 1 ulp off in y and z in every live script.  This one is
 * bit-identical on 2000 random inputs (differential emulation against
 * 0x1006DE70) and EQUIVALENT in all eight live scripts.  Do not reopen
 * before the end-grind. */
/* @t4-pass 0x1006DE70 3 2026-09-27 probes 100 bytes 419 insns 162 regions 1 rows 94 census yes  (hand: symbol-table band sweep 0-1568, C and C++) */
/* @t4-pass 0x1006DE70 4 2026-09-27 probes 84 bytes 419 insns 162 regions 1 rows 94 census no  (hand: y/z rows inline vs named -- 64 subsets of n/g/k/product locals, det/n0/inv naming, const) */
/* @t4-pass 0x1006DE70 5 2026-09-27 probes 12 bytes 579 insns 203 regions 1 rows 135 census no  (hand: 12 compiler option sets on the rounding-exact body) */
/* @t4-pass 0x1006DE70 6 2026-09-27 probes 32 bytes 579 insns 203 regions 1 rows 135 census yes  (hand: symbol-table band sweep 0-992 on the rounding-exact body) */
/* @implements 0x1006DE70 glide BrMat3Solve */
/* FRAME (proven 2026-09-03, do not re-derive).  `sub esp, 0x10` is FOUR float
 * slots -- d0, m2m7, m1m5, m2m4 -- in declaration order at [esp], [esp+4],
 * [esp+8], [esp+0xc].  A FIFTH value, m[1]*m[8], lives in the dead pM
 * parameter slot [esp+0x18] (fst at 1006DEAF, fld at 1006DEFF), alongside an
 * anonymous m[4]*m[8] that occupies the same slot earlier (1006DE96 fstp /
 * 1006DE9C fsub).  Two values sharing one slot means they are COMPILER CSE
 * TEMPS, not named locals, so m[1]*m[8] is spelled inline at both its uses;
 * naming it forces `sub esp, 0x14` and a fifth frame slot.  Inlining the
 * other four instead is worse still (`sub esp, 0x14`, 435 B) -- they really
 * are named locals.
 *
 * pV is NOT copied into three float locals: the original reads [ecx], [ecx+4]
 * and [ecx+8] fifteen times, so v0/v1/v2 are macros over the parameter.
 * Naming them costs two integer copy pairs and turns every use into an
 * [esp+N] read.
 *
 * RESIDUE (347 bytes differ, 419 vs 417, instruction count -2; REGNORM
 * multiset gap 18+20).  Ruled out, do not re-probe: swapping the operands of
 * a both-memory `fmul` here (`m[5]*m[7]` for `m[7]*m[5]` -- byte-identical
 * output in THIS function; note that the same swap on an ADDITION was the
 * whole match in 0x1006DAD0, so operand order is worth probing elsewhere,
 * just not on these multiplies), and naming m[4]*m[8] (411 B / 340 diffs but
 * FIRSTDIV regresses to +0xd and the 1006DE9C memory `fsub` still does not
 * appear).  What is still
 * unexplained: (1) the original keeps `inv` in an x87 register from the
 * 1006DF14 `fdivr` all the way to the closing `fstp st(0)`, spending it as
 * three `fmul st(1)`; the recompile homes it in the dead parameter slot and
 * contends there with the m[1]*m[8] temp.  (2) Six products whose operand
 * roles are inverted (m[0] and pV->x land on the fmul side instead of the
 * fld side).  Both look like allocator choices rather than source shape. */
void BrMat3Solve(BrVec3 *pOut, const BrMat3 *pM, const BrVec3 *pV)
{
    const float *m = pM->m;
    const double v0 = pV->x, v1 = pV->y, v2 = pV->z;
    float m4m8, d0, m1m5, m1m8s, m2m7s, m2m4s;
    double m1m8, m2m7, m2m4, det, inv, n0, n1, n2;

    BR_SOLVE_ROUND(m4m8, m[4] * m[8])
    BR_SOLVE_ROUND(d0, m[7] * m[5] - m4m8)
    m1m8 = m[1] * m[8];
    m2m7 = m[2] * m[7];
    BR_SOLVE_ROUND(m1m5, m[1] * m[5])
    m2m4 = m[2] * m[4];
    BR_SOLVE_ROUND(m1m8s, (float)m1m8)
    BR_SOLVE_ROUND(m2m7s, (float)m2m7)
    BR_SOLVE_ROUND(m2m4s, (float)m2m4)

    /* det is the NEGATED determinant -- the original expands with the signs
     * flipped and compensates by negating only pOut->y below. */
    det = (((m1m8 * m[3] + m[0] * d0) - m2m7 * m[3])
           - (double)m1m5 * m[6]) + m2m4 * m[6];
    inv = BR_SOLVE_ONE / det;

    n0 = ((((v0 * d0 + m1m8s * v1) - m2m7s * v1) - m1m5 * v2) + m2m4s * v2);
    {
        /* y and z stay on the x87 stack in the original: doubles here. */
        const double g0   = m[6] * m[5] - m[3] * m[8];
        const double m0m8 = m[0] * m[8];
        const double m6m2 = m[6] * m[2];
        const double m0m5 = m[0] * m[5];
        const double m3m2 = m[3] * m[2];
        const double k0   = m[6] * m[4] - m[3] * m[7];
        const double m0m7 = m[0] * m[7];
        const double m6m1 = m[6] * m[1];
        const double m0m4 = m[0] * m[4];
        const double m3m1 = m[3] * m[1];
        n1 = (((g0 * v0 + m0m8 * v1) - m6m2 * v1) - m0m5 * v2) + m3m2 * v2;
        n2 = (((k0 * v0 + m0m7 * v1) - m6m1 * v1) - m0m4 * v2) + m3m1 * v2;
    }

    pOut->x = (float)(n0 * inv);
    pOut->y = (float)-(n1 * inv);   /* the sign flip that pairs with the negated det */
    pOut->z = (float)(n2 * inv);
}

/* BrMat4BuildScaledTransposed: br_mat3.c (glide 0x1006DDD0) */


/* ================================================================== */
/* Rigid-body integrator                                               */
/* ================================================================== */


/* 0x100743A0 */
/* WHAT IT DOES: adds one time step's worth of acceleration to a body's speed
 * and spin -- the first half of the physics step, before anything has
 * actually moved. */
/* @d3donly 0x100743A0 BrRbIntegrateVelocity -- glide twin 0x1006D600 claimed by br_carphys.c:BrCpIntegrateVelocity */
/* (port-only BrRbIntegrateVelocity removed) */


/* 0x100745F0 */
/* WHAT IT DOES: advances a body one time step: moves it by its speed, turns
 * it by the rate its orientation is changing, and renormalises the
 * orientation afterwards so accumulated rounding does not slowly distort the
 * body. Speed and spin are carried across unchanged, since the previous step
 * already updated them. */
/* port-only body; Glide match is src/core/driving/BrRbIntegrateState_1006D850.cpp
 * (the original is C++: see that file). */
/* FLOAT, not double. The double model here was written for the D3D twin's
 * codegen -- BrRbBuildMatrix below records the same correction -- and the
 * GLIDE original never spills a qword: every product is
 * `fld dword [esp+dt]; fmul dword ptr [esi+N]`, i.e. dt reloaded fresh as a
 * float with the member as the memory operand. The three that ARE spilled
 * (`fstp dword [esp+N]` then reloaded) round to float once more, which the
 * explicit casts below reproduce; the spill map in the port arm records
 * which, read off each instruction. */
/* BrRbIntegrateState: the placed body is BrRbIntegrateState_1006D850.cpp */

/* 0x100742D0 */
/* WHAT IT DOES: works out how fast a body's orientation is changing, given
 * how fast it is spinning. The result is what the integrator adds to the
 * orientation each step to make the body actually turn. */
/* BYTE-EXACT 2026-09-24 (206/206, 73/73, 0 differing).  The 21-byte
 * "x87 scheduling" residue this function carried as T3 was a WRONG row 3:
 * the subtract-second spelling changed the sum.  The original's DAG,
 * (hz*q1 + hy*q0) - hx*q3, is also its exact bytes. */
/* @implements 0x100742D0 d3d BrRbQuatDerivative */
void BrRbQuatDerivative(BrRbState *pS)
{
    /* SPILL MAP, traced instruction by instruction against the Glide original
     * 0x1006D530.  The scale is a float constant (3F000000 == 0.5f), loaded
     * as `fmul dword`, so the three halved rates are plain `float` locals in
     * 12 bytes of frame -- `sub esp, 0xc`, homed at [esp], [esp+4], [esp+8].
     *
     * An earlier reading took the fst/fstp asymmetry at 1006D54E (hx: stored
     * AND popped) versus 1006D566 / 1006D574 (hy, hz: stored and KEPT) for a
     * mixed float/double source, and typed the halves `double` with per-use
     * casts.  That is a misreading of the register allocator.  Without /Op,
     * VC5 is free to keep a just-stored value in st and spend it once before
     * reloading the slot; each of hx, hy and hz is used exactly four times
     * here (twelve products in total), and the counts come out right with all
     * three plain floats: hx = four `fld [esp]`, hy = three `fld [esp+4]`
     * plus the kept register, hz = three `fld [esp+8]` plus the kept one.
     * The double spelling is what forced the `fmul qword` chain.
     *
     * The quaternion components are read straight out of the struct
     * (both-memory `fmul dword [eax+0x18..0x24]`), not through named locals.
     * The four results are `fstp dword` at 1006D5EA..1006D5F7, in field
     * order, so each expression rounds to float exactly once, at the store.
     *
     * The halves are ONE AGGREGATE, not three scalars.  With three separate
     * `float` locals VC5 packs the third into the dead `pS` parameter slot
     * (`sub esp, 8`, hz at [esp+0xc] -- the idiom at VC5-IDIOMS "PACKS
     * ORDINARY LOCALS INTO DEAD PARAMETER SLOTS"); an aggregate is allocated
     * whole and restores `sub esp, 0xc` with the slots in declaration order.
     * BrVec3 and float[3] compile identically here; BrVec3 reads better.
     *
     * ROW 3 is plus-then-minus like rows 2 and 4: the original's DAG is
     * (hz*q1 + hy*q0) - hx*q3.  A subtract-second spelling once removed an
     * fxch and passed as 'scheduling residue' -- it computed a different sum
     * (the whole-image run caught it) and the correct row is byte-exact.
     *
     * POSITION IN THE TU IS LOAD-BEARING: this function sits immediately
     * ahead of BrRbBuildMatrix; elsewhere in slice3_44.c the allocator
     * rotates. */
    BrVec3 h;

    h.x = pS->angVel.x * 0.5f;
    h.y = pS->angVel.y * 0.5f;
    h.z = pS->angVel.z * 0.5f;

    /* qDot = 0.5 * (0, wx, wy, wz) (x) q, scalar first.  The leading `fchs`
     * at 1006D588 is the unary minus on the first product only: the row is
     * `-hx*x - hy*y - hz*z`, evaluated left to right. */
    pS->qDot.f00 = -h.x * pS->quat.f04 - h.y * pS->quat.f08 - h.z * pS->quat.f0C;
    pS->qDot.f04 = h.y * pS->quat.f0C + h.x * pS->quat.f00 - h.z * pS->quat.f08;
    /* row 3 is plus-then-minus like rows 2 and 4 (the original's x87
     * DAG: (hz*q1 + hy*q0) - hx*q3); a subtract-second spelling moved
     * bytes and changed the rounding -- the whole-image run caught it */
    pS->qDot.f08 = h.z * pS->quat.f04 + h.y * pS->quat.f00 - h.x * pS->quat.f0C;
    pS->qDot.f0C = h.x * pS->quat.f08 + h.z * pS->quat.f00 - h.y * pS->quat.f04;
}

/* 0x1006D6B0 */
/* WHAT IT DOES: builds the transform matrix that places a body in the world,
 * from its orientation and position -- the matrix the renderer needs to draw
 * the car where the physics says it is. */
/* NOT MATCHING -- but CLOSE, and the old double-precision model was for the
 * D3D twin's codegen, not this binary's.  The GLIDE original computes the
 * whole function in FLOAT precision: squares from spilled float locals
 * (a,b,c,d copied to slots via integer movs), cross products with both
 * operands read straight from the struct, each doubled with fadd st(0),st(0).
 * This float form compiles to 400 bytes against the original's 405 with the
 * identical opening; the residue is one spill-allocator divergence: the
 * original uses SEVEN stack slots (frame 0x1c) and re-reads aa/ab from
 * memory, where VC5 here packs the same DAG into SIX (frame 0x18) and keeps
 * one register copy (fst vs fstp).  Statement reordering does not move it:
 * four orderings of the locals compile to byte-identical output (X3-X6
 * scratch experiment, 2026-08-22), so the DAG is canonicalised and the slot
 * count is an allocator-internal decision.  /Ox, /O1, /Og/Ot, /O2/Oy- all
 * land farther away. */
/* @t4-pass 0x1006D6B0 1 2026-09-07 probes 64 bytes 397 insns 132 regions 3 rows 12 census yes  (tools/crank.py) */
/* @t4-pass 0x1006D6B0 2 2026-09-07 probes 64 bytes 397 insns 132 regions 3 rows 12 census yes  (tools/crank.py) */
/* @t3 0x1006D6B0 2026-09-27 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 559/405 insns 176/136 rows 41+81 regions 1 oracle EQUIVALENT
 * @t3-effort passes 6 zero-movement 5 6
 * Residue: the body is written for the original's ROUNDING POINTS (which
 * values it stores to float slots and which it keeps on the 53-bit x87
 * stack), not for its bytes; the hill-climbed float body it replaced was
 * 399/405 B but 1 ulp off on 1215 of 2000 random inputs.  This one is
 * bit-identical on all 2000 (differential emulation) and EQUIVALENT in all
 * eight live scripts.  Do not reopen before the end-grind.
 */
/* @t4-pass 0x1006D6B0 3 2026-09-27 probes 61 bytes 399 insns 133 regions 3 rows 25 census yes  (hand: fresh transcription with by-value square helper BrSq, 60 random declaration orders of the 10 products -- 447..451 B, worse) */
/* @t4-pass 0x1006D6B0 4 2026-09-27 probes 32 bytes 399 insns 133 regions 3 rows 25 census no  (hand: symbol-table band sweep, extern-int pads 0-992) */
/* @t4-pass 0x1006D6B0 5 2026-09-27 probes 12 bytes 559 insns 176 regions 1 rows 122 census no  (hand: 12 compiler option sets on the rounding-exact body) */
/* @t4-pass 0x1006D6B0 6 2026-09-27 probes 32 bytes 559 insns 176 regions 1 rows 122 census yes  (hand: symbol-table band sweep 0-992 on the rounding-exact body) */
/* @implements 0x1006D6B0 glide BrRbBuildMatrix */
void BrRbBuildMatrix(BrMat4 *pM, const BrRbState *pS)
{
    /* ROUNDING POINTS (2026-09-27, derived from the listing and checked by
     * differential emulation against 0x1006D6B0).  The original stores ww,
     * xx, yy, zz, ww - xx and the doubled products 2yx, 2yw, 2zy to float
     * slots and reads them back; 2zw, 2zx and 2xw are stored as float copies
     * but also used once straight from the stack.  Everything the original
     * keeps on the x87 stack is a double here. */
    const float w = pS->quat.f00, x = pS->quat.f04, y = pS->quat.f08, z = pS->quat.f0C;
    float ww, xx, yy, zz, ab, yx2, yw2, zy2, zw2s, zx2s, xw2s;
    double zw2, zx2, xw2;

    BR_SOLVE_ROUND(ww, w * w)
    BR_SOLVE_ROUND(xx, x * x)
    BR_SOLVE_ROUND(yy, y * y)
    BR_SOLVE_ROUND(zz, z * z)
    BR_SOLVE_ROUND(ab, ww - xx)
    zw2 = z * w;  zw2 += zw2;
    zx2 = z * x;  zx2 += zx2;
    xw2 = x * w;  xw2 += xw2;
    {
        double t;
        t = y * x;  BR_SOLVE_ROUND(yx2, t + t)
        t = y * w;  BR_SOLVE_ROUND(yw2, t + t)
        t = z * y;  BR_SOLVE_ROUND(zy2, t + t)
    }
    BR_SOLVE_ROUND(zw2s, zw2)
    BR_SOLVE_ROUND(zx2s, zx2)
    BR_SOLVE_ROUND(xw2s, xw2)

    pM->m[0][0] = (float)((((double)xx + ww) - yy) - zz);
    pM->m[0][1] = (float)(zw2 + yx2);
    pM->m[0][2] = (float)(zx2 - yw2);
    pM->m[0][3] = 0.0f;
    pM->m[1][0] = (float)((double)yx2 - zw2s);
    pM->m[1][1] = (float)(((double)yy + ab) - zz);
    pM->m[1][2] = (float)(xw2 + zy2);
    pM->m[1][3] = 0.0f;
    pM->m[2][0] = (float)((double)yw2 + zx2s);
    pM->m[2][1] = (float)((double)zy2 - xw2s);
    pM->m[2][2] = (float)(((double)ab - yy) + zz);
    pM->m[2][3] = 0.0f;
    pM->m[3][0] = pS->pos.x;
    pM->m[3][1] = pS->pos.y;
    pM->m[3][2] = pS->pos.z;
    pM->m[3][3] = 1.0f;
    BrStub8B80_1p();
}

/* 0x10074870 BrRbInitInertia now lives in src/core/driving/br_rbinertia.c. */

/* ==========================================================================
 * 0x10074870 (glide 0x1006DAD0) -- the body's inertia setup.
 *
 * Its original TU is BrMat3Solve's (tu_055: 0x1006DAD0 sits just before
 * 0x1006DE70), and its three `1 / inertia` divides read the SAME float 1.0 at
 * 0x10077C1C that BrMat3Solve does -- so it lives here and names that
 * constant (BR_SOLVE_ONE).  Filed in br_carphys.c it pooled its own 1.0f
 * with that TU's (0x10077A7C) and matched only because the image gate copied
 * the address from the original.  It sits LAST in the file: ahead of
 * BrRbQuatDerivative / BrRbBuildMatrix it moves their codegen (TU state).
 *
 * Float constants read out of orig/BRD3D.dll .rdata with tools/pe.py rather
 * than assumed:  0x1008FC54 = 0x3DAAAAAB = 1/12 (the correctly rounded
 * float); in-line immediates 0x3F800000 = 1.0f, 0x3F000000 = 0.5f,
 * 0x3E322D0E = 0.174f.
 * ========================================================================== */

/* 0x1008FC54.  1.0f/12.0f rounds to the same 0x3DAAAAAB, but the value is
 * spelled out so a future reader does not have to re-derive it. */
#define BR_K_ONE_TWELFTH  0.0833333358168602f
/* the 0x1C0 immediate 0x3E322D0E */
#define BR_K_1C0          0.174f

/* XSLICE 0x10075330 */
/* Name copied from slice2_16.h. */
/* BrGbiCall10075330: prototype in br_funcs.h */

/* 0x10074870 */
/* WHAT IT DOES: sets a body up for physics: clears its accumulated forces,
 * plants a few fixed constants, and computes how hard it is to spin about
 * each axis. For the two box-shaped modes that comes from the standard
 * solid-box formula using the body's size and mass; other modes are left
 * with the placeholder value of one, which is worth knowing because it means
 * an unrecognised mode gets unit resistance rather than an error. */
/* @implements 0x10074870 d3d BrRbInitInertia */
void BrRbInitInertia(BrRbBody *pB)
{
    int i, j;

    pB->f00 = 0.0f;
    pB->child[0] = 0;
    pB->child[1] = 0;
    pB->child[2] = 0;
    pB->child[3] = 0;
    pB->f14 = 0.0f;

    pB->f1B4 = 0.0f;
    pB->pPlane = 0;
    pB->f1C4 = 0.0f;
    pB->f1C0 = BR_K_1C0;
    pB->f1CC = 0.0f;
    pB->f1D0 = 0.0f;
    pB->f1C8 = 0.5f;

    /* if/else, NOT a ternary: the original stores both arms as integer
     * immediates (`mov dword [esi+ebx*4], 0x3f800000` / `, edi` with edi the
     * zero register), which is the float-constant store VC5 emits for a plain
     * literal assignment.  A ternary makes the value runtime-selected and
     * costs an `fld` of each constant plus an `fstp`.  The index stays
     * `3*i + j` -- the strength reducer folds inertia's 0x30 byte offset into
     * the row IV, which is where the `mov ecx, 0xc` / `add ecx, 3` /
     * `cmp ecx, 0x15` walk comes from. */
    for (i = 0; i < 3; ++i) {
        for (j = 0; j < 3; ++j) {
            if (i == j)
                pB->inertia.m[3 * i + j] = 1.0f;
            else
                pB->inertia.m[3 * i + j] = 0.0f;
        }
    }

    if (pB->mode >= 0 && pB->mode <= 1) {
        /* SPILL MAP (Glide 0x1006DB5E..0x1006DBE5).  Everything here is
         * FLOAT: every fmul and fadd in the block is a `dword` operand.  An
         * earlier reading took the fst/fstp asymmetry for a double source and
         * typed the sides `double` with per-use casts; see the VC5-IDIOMS
         * entry "`fst` (not `fstp`) of a float local does NOT mean the source
         * was double" -- without /Op the scheduler keeps unrounded values in
         * st freely, and the double spelling is what produced the `fmul qword`
         * / `fdivr qword` chain.
         *
         * The three edge lengths are three SCALAR float locals, copied out of
         * dim[] with integer movs at 1006DB67/DB6F/DB73.  Only two get frame
         * slots -- x at [esp+0xc], y at [esp+0x10], which is the whole
         * `sub esp, 8` -- and z is packed into the dead pB parameter slot
         * [esp+0x18].  The two squares that need memory (z*z, then y*y) reuse
         * that same slot after z dies, so they are compiler CSE temps and the
         * squares are spelled inline; x*x is never stored at all (duplicated
         * with `fld st(2)` at 1006DB9E).  mass and the 1/12 constant are
         * re-read per row, not cached. */
        /* DECLARATION ORDER IS LOAD-BEARING: x must come LAST.  It decides
         * which of the three lands in the dead parameter slot -- with x first
         * VC5 never homes y at all (`fld [esi+0x24]` + `fmul st`), and with x
         * in the middle the squares come out in the wrong order.  y before z
         * or z before y are both byte-exact; this is the order the formula
         * first mentions them in. */
        float y = pB->dim[1];
        float z = pB->dim[2];
        float x = pB->dim[0];

        /* The addends of the FIRST row are load-bearing too: `y*y + z*z`, not
         * `z*z + y*y`.  That one swap is the difference between byte-exact and
         * 113 differing bytes -- it picks which square is computed first, and
         * so which value the dead parameter slot is recycled for. */
        pB->inertia.m[0] = (y * y + z * z) * pB->mass * BR_K_ONE_TWELFTH;
        pB->inertia.m[4] = (x * x + z * z) * pB->mass * BR_K_ONE_TWELFTH;
        pB->inertia.m[8] = (x * x + y * y) * pB->mass * BR_K_ONE_TWELFTH;

        BrNop6E590();
    }

    if (pB->mode != 2) {
        /* only the diagonal -- see the header's gotcha.
         *
         * `fld [1.0f]; fdiv DWORD ptr [esi+0x30]; fstp dword ptr` at
         * 1006DBF6..1006DC17.  A `fdiv dword` is a float divide, so this is
         * `1.0f / x`, not a double divide narrowed once -- an earlier reading
         * spelled it `(float)(1.0 / (double)x)` and got `fdivr qword`.  (The
         * two agree numerically on every normal float, so no test could catch
         * the difference; the bytes are the only evidence, and they say
         * float.) */
        pB->invInertia.m[0] = BR_SOLVE_ONE / pB->inertia.m[0];
        pB->invInertia.m[4] = BR_SOLVE_ONE / pB->inertia.m[4];
        pB->invInertia.m[8] = BR_SOLVE_ONE / pB->inertia.m[8];
    }

    pB->f1D4 = 0.0f;
    pB->f1D8 = 0.0f;
}
