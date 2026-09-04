/* slice3_44.c -- decompiled from BRD3D.dll, packet 0x10071B80-0x10075F10.
 *
 * See slice3_44.h for the API, the layout notes and the gotchas, and the
 * pass report for the list of addresses that were deliberately skipped.
 *
 * Float constants used here, read out of orig/BRD3D.dll .rdata with
 * tools/pe.py rather than assumed:
 *
 *   0x1008FC48 = 0x3F800000 = 1.0f
 *   0x1008FC4C = 0x3F000000 = 0.5f
 *   0x1008FC54 = 0x3DAAAAAB = 1/12 (the correctly rounded float)
 *   in-line immediates: 0x3F800000 = 1.0f, 0x3F000000 = 0.5f,
 *                       0x3E322D0E = 0.174f
 */
#include "slice3_44.h"

/* ---- constants ---------------------------------------------------- */

/* ---- cross-slice --------------------------------------------------- */

/* XSLICE 0x10008B80 */
/* A bare `ret` in this build (see CONTRACT).  Name and prototype copied from
 * slice2_18.h so integration can wire it mechanically. */
extern void BrStub8B80_1p(const void *p0);

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
            /* x87 order, read off the two faddps at 1006DD55 and 1006DD60:
             * `faddp st(1)` is ST(1) += ST(0), so the accumulator is the
             * FIRST-written term.  Stack at 1006DD55 is [A, C, B], giving
             * C + A; at 1006DD60 it is [B, C+A], giving (C+A) + B.  The sum
             * is therefore (a2*b2j + a0*b0j) + a1*b1j -- the middle row is
             * the LAST term.
             *
             * RESIDUE 19 bytes, all in the two `lea`s and the two operands
             * that hang off them.  VC5 anchors each strength-reduced IV on
             * the LAST array reference in the expression: with a1/b1j last it
             * picks `lea ecx,[eax+4]` / `lea eax,[edi+0xc]` (offsets -4/0/+4),
             * where the original anchors on the a2/b2j row (`[eax+8]` /
             * `[edi+0x18]`, offsets -0x18/-0xc/0).  All six term orders were
             * probed: the three that end in the a2 term (ABC, BAC) score 17
             * and get the anchors right, but they contradict the faddp
             * reading above -- they compute a DIFFERENT association, so the
             * lower byte count is not the more faithful source.  Also ruled
             * out: direct pA->m[]/pB->m[] instead of the a/b pointer locals,
             * `b[j+6]` index spelling, and /Oy- /Op /Od (33, 19, 80).
             * DEAD 2026-09-03: naming the three products as float temps so
             * the a2/b2 row is the LAST address reference while the sum
             * keeps the (C+A)+B association -- the one combination the six
             * term-order probes could not express, because there the anchor
             * and the association move together. VC5 keeps the a1/b1 anchor
             * anyway; 19 -> 28 with the multiset gap unchanged. The anchor
             * is not chosen from the source's reference order.
             * DEAD 2026-09-04, 39 more spellings: every term order x every
             * factor order x a paren round the first product (32, ALL
             * byte-identical -- the expression is fully canonicalised);
             * walking pointers (`a += 3` per row, `++b` per column, `*o++`)
             * are byte-identical too; pre-biased pointers (`pA->m + 2`,
             * `pB->m + 6`, either or both) and 2-D `pA->m[3*i+2]`
             * indexing are worse (60-75 diffs).  The original's anchor on
             * row/column 2 is also what BOTH nests of BrMat4Mul get, so it
             * is a property of VC5's strength reduction, not of the
             * source.  Do not probe the expression again. */
            pOut->m[3 * i + j] = (a[3 * i + 2] * b[6 + j]
                                  + a[3 * i + 0] * b[j])
                                 + a[3 * i + 1] * b[3 + j];
        }
    }
}

/* WHAT IT DOES: solves the 3x3 system pM * x = pV for x by Cramer's rule and
 * writes x to pOut.  No singularity guard -- a singular matrix yields +-inf or
 * NaN, exactly as the original.  Confirmed equivalent to the original bytes by
 * an x87 emulation of 0x1006DE70 over random and structured inputs. */
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
#define v0 pV->x
#define v1 pV->y
#define v2 pV->z

    /* --- shared sub-expressions, in the order the original spills them --- */
    const float d0    = m[7] * m[5] - m[4] * m[8];   /* cofactor of m[0] */
    const float m2m7  = m[2] * m[7];
    const float m1m5  = m[1] * m[5];
    const float m2m4  = m[2] * m[4];

    /* det is the NEGATED determinant -- the original expands with the signs
     * flipped and compensates by negating only pOut->y below. */
    const float det = (((m[1] * m[8] * m[3] + m[0] * d0)
                        - m2m7 * m[3])
                       - m1m5 * m[6])
                      + m2m4 * m[6];
    const float inv = 1.0f / det;

    const float n0 = (((v0 * d0 + m[1] * m[8] * v1)
                       - m2m7 * v1)
                      - m1m5 * v2)
                     + m2m4 * v2;

    const float g0   = m[6] * m[5] - m[3] * m[8];
    const float m0m8 = m[0] * m[8];
    const float m6m2 = m[6] * m[2];
    const float m0m5 = m[0] * m[5];
    const float m3m2 = m[3] * m[2];
    const float n1 = (((g0 * v0 + m0m8 * v1)
                       - m6m2 * v1)
                      - m0m5 * v2)
                     + m3m2 * v2;

    const float k0   = m[6] * m[4] - m[3] * m[7];
    const float m0m7 = m[0] * m[7];
    const float m6m1 = m[6] * m[1];
    const float m0m4 = m[0] * m[4];
    const float m3m1 = m[3] * m[1];
    const float n2 = (((k0 * v0 + m0m7 * v1)
                       - m6m1 * v1)
                      - m0m4 * v2)
                     + m3m1 * v2;

    pOut->x =  n0 * inv;
    pOut->y = -(n1 * inv);   /* the sign flip that pairs with the negated det */
    pOut->z =  n2 * inv;
#undef v0
#undef v1
#undef v2
}

/* 0x10074B70 */
void BrMat4BuildScaledTransposed(const BrMat4 *pA, BrMat4 *pOut,
                                 const BrMat4 *pS)
{
    BrVec3 t, r;
    int    i, j;

    for (i = 0; i < 3; ++i) {
        for (j = 0; j < 3; ++j)
            pOut->m[i][j] = pA->m[j][i] * pS->m[0][j];
        pOut->m[i][3] = 0.0f;
    }
    pOut->m[3][3] = 1.0f;

    /* ARG1, NOT ARG3 -- and the two are one byte apart in the listing.
     *
     * Glide 0x1006DDD0 loads the translation at 0x1006DE2C with
     * `mov ecx,[esp+0x24]`.  An earlier load at 0x1006DDD9 uses the SAME
     * displacement, `mov ebp,[esp+0x24]`, and that one IS pS.  They differ
     * because `push esi` / `push edi` sit between them:
     *
     *     entry            esp = R      (arg1 R+4, arg2 R+8, arg3 R+0xC)
     *     sub esp,0x10     esp = R-0x10
     *     push ebx         esp = R-0x14   [esp+0x18] = R+4  = pA
     *     push ebp         esp = R-0x18   [esp+0x24] = R+0xC = pS   <-- arg3
     *     push esi/edi     esp = R-0x20   [esp+0x24] = R+4  = pA    <-- arg1
     *
     * This file read pS here, and br_collresp.h then explained the resulting
     * garbage as a deliberate "frame overlap" -- a misreading dressed as a
     * preserved bug, which CONVENTIONS.md calls the durable kind, and the
     * test below asserted it.  The falsifier is semantic, not just textual:
     * pS is the scale VECTOR, whose m[3][*] is off the end of the three
     * floats that exist, so under the old reading the box matrix had no
     * translation at all and the OBB sat at the world origin.  A car at
     * (927, 361) could not have collided with anything on any track. */
    t.x = -pA->m[3][0];
    t.y = -pA->m[3][1];
    t.z = -pA->m[3][2];

    /* DEVIATION: the original passes &pOut->m[3][0] straight to 0x10074770 as
     * the output vector.  Routing it through a local avoids casting a float[4]
     * row to BrVec3*; 0x10074770 reads only rows 0..2, so this is
     * observationally identical. */
    BrMat4MulVec3Transposed(&r, pOut, &t);
    pOut->m[3][0] = r.x;
    pOut->m[3][1] = r.y;
    pOut->m[3][2] = r.z;
}

/* ================================================================== */
/* Rigid-body integrator                                               */
/* ================================================================== */

/* 0x100742D0 */
/* WHAT IT DOES: works out how fast a body's orientation is changing, given
 * how fast it is spinning. The result is what the integrator adds to the
 * orientation each step to make the body actually turn. */
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
     * ROW 3 SUBTRACTS SECOND, NOT LAST.  `h.z*f04 - h.x*f0C + h.y*f00`, not
     * `h.z*f04 + h.y*f00 - h.x*f0C`.  Only row 3 takes this form; rows 2 and
     * 4 keep the plus-then-minus shape, and their own alternatives change
     * nothing (measured, all give the same score).
     *
     * ‼ CORRECTION.  An earlier version of this note claimed 23 bytes of
     * residue with "instruction stream, count and size exact (RAW and REGNORM
     * multiset gap 0+0)".  That was never true: measured at that note's own
     * commit the function was 208 bytes against 206, 74 instructions against
     * 73, with ONE SURPLUS `fxch` and REGNORM 1+0.  A claim of parity is what
     * stops the next reader from working a function, so it has to be measured
     * before it is written.  The subtract-second row above is what actually
     * removes that `fxch`; the function is only NOW at 206/206, 73/73 and
     * RAW/REGNORM 0+0, with 21 differing bytes.
     *
     * RESIDUE (21 bytes): every remaining differing byte is the ModRM of an
     * `fxch`/`faddp`/`fsubp` st(i) index -- the x87 stack holds the same
     * values in a different permutation from 1006D565 on.  Probed and ruled
     * out, do NOT re-run: the full 4x4x4 sweep of per-row term orders and
     * associations for rows 2/3/4 (64 builds, nothing beats 21, and the six
     * that tie differ only in rows 2 and 4); all five non-identity orderings
     * of the three h assignments RE-RUN against the corrected row 3 (50, 85,
     * 90, 165 and 170 diffs -- x, y, z still wins); three re-associations of
     * row 1 (30, 141 and 168); `float[3]` vs `BrVec3` (byte-identical); and
     * `/O2 /Op` (214 bytes, 134 diffs -- strictly worse, this TU is /O2). */
    BrVec3 h;

    h.x = pS->angVel.x * 0.5f;
    h.y = pS->angVel.y * 0.5f;
    h.z = pS->angVel.z * 0.5f;

    /* qDot = 0.5 * (0, wx, wy, wz) (x) q, scalar first.  The leading `fchs`
     * at 1006D588 is the unary minus on the first product only: the row is
     * `-hx*x - hy*y - hz*z`, evaluated left to right. */
    pS->qDot.f00 = -h.x * pS->quat.f04 - h.y * pS->quat.f08 - h.z * pS->quat.f0C;
    pS->qDot.f04 = h.y * pS->quat.f0C + h.x * pS->quat.f00 - h.z * pS->quat.f08;
    pS->qDot.f08 = h.z * pS->quat.f04 - h.x * pS->quat.f0C + h.y * pS->quat.f00;
    pS->qDot.f0C = h.x * pS->quat.f08 + h.z * pS->quat.f00 - h.y * pS->quat.f04;
}

/* 0x100743A0 */
/* WHAT IT DOES: adds one time step's worth of acceleration to a body's speed
 * and spin -- the first half of the physics step, before anything has
 * actually moved. */
/* @d3donly 0x100743A0 BrRbIntegrateVelocity -- glide twin 0x1006D600 claimed by br_carphys.c:BrCpIntegrateVelocity */
void BrRbIntegrateVelocity(BrRbState *pS, const BrRbBody *pBody, float dt)
{
    /* SPILL MAP.  Six products, and exactly ONE of them is rounded:
     *
     *   100743EB  fstp dword [esp+8]   accel[2]*dt -- stored AND POPPED, then
     *   100743F5  fld  dword [esp+8]   reloaded.  This one really is float.
     *
     *   100743FB  fst  dword [esp]     angAccel[0]*dt \  stored and KEPT, and
     *   10074405  fst  dword [esp+4]   angAccel[1]*dt  > the three slots are
     *   1007440B  fst  dword [esp+8]   angAccel[2]*dt /  NEVER RELOADED.
     *
     * Nothing between 1007440F and the epilogue reads [esp], [esp+4] or
     * [esp+8].  Those three stores are dead -- register-allocator spill slots
     * the compiler wrote and did not need -- so the adds at 1007441B,
     * 10074420 and 10074425 all consume the unrounded register copies.  A
     * spill that is never reloaded rounds nothing, and treating `fst` as
     * evidence of rounding without checking for the matching `fld` would have
     * put a float here on the strength of an instruction with no effect.
     *
     * accel[0]*dt and accel[1]*dt are never stored at all.
     *
     * All six results are `fstp dword` at 1007442F..10074442, so each sum
     * rounds to float once, at the store. */
    const double d = (double)dt;

    /* 0x0C <- 0xFC   product never spilled */
    pS->vel.x    = (float)((double)pBody->accel[0] * d + (double)pS->vel.x);
    /* 0x10 <- 0x100  product never spilled */
    pS->vel.y    = (float)((double)pBody->accel[1] * d + (double)pS->vel.y);
    /* 0x14 <- 0x104  product SPILLED and reloaded: rounded before the add */
    pS->vel.z    = (float)((double)(float)((double)pBody->accel[2] * d)
                           + (double)pS->vel.z);
    /* 0x28 <- 0x108  spill slot written but never read */
    pS->angVel.x = (float)((double)pBody->angAccel[0] * d
                           + (double)pS->angVel.x);
    /* 0x2C <- 0x10C  likewise */
    pS->angVel.y = (float)((double)pBody->angAccel[1] * d
                           + (double)pS->angVel.y);
    /* 0x30 <- 0x110  likewise */
    pS->angVel.z = (float)((double)pBody->angAccel[2] * d
                           + (double)pS->angVel.z);
}

/* 0x100745F0 */
/* WHAT IT DOES: advances a body one time step: moves it by its speed, turns
 * it by the rate its orientation is changing, and renormalises the
 * orientation afterwards so accumulated rounding does not slowly distort the
 * body. Speed and spin are carried across unchanged, since the previous step
 * already updated them. */
/* @implements 0x100745F0 d3d BrRbIntegrateState */
#ifdef BR_MATCHING_BUILD
/* FLOAT, not double. The double model here was written for the D3D twin's
 * codegen -- BrRbBuildMatrix below records the same correction -- and the
 * GLIDE original never spills a qword: every product is
 * `fld dword [esp+dt]; fmul dword ptr [esi+N]`, i.e. dt reloaded fresh as a
 * float with the member as the memory operand. The three that ARE spilled
 * (`fstp dword [esp+N]` then reloaded) round to float once more, which the
 * explicit casts below reproduce; the spill map in the port arm records
 * which, read off each instruction. */
void BrRbIntegrateState(BrRbState *pDst, const BrRbState *pSrc, float dt)
{
    /* All three products FIRST, then the adds: the original loads dt three
     * times over and holds the products live, which is what forces the
     * spills. A `(float)` cast on an already-float expression is a no-op and
     * changes nothing; only the register pressure does.
     *
     * RESIDUE (57 regnorm, -11 bytes, 86 instructions against 85): six of
     * the seven products come out `fld <member>; fmul dt` where the original
     * has `fld dt; fmul <member>`, and ours holds a base pointer in ebx
     * where the original spills to four stack slots. The operand order is
     * NOT source-reachable here -- `pSrc->vel.x * dt` and `dt * pSrc->vel.x`
     * compile to byte-identical output, so VC5 canonicalises a float
     * multiply of two MEMORY operands. It IS reachable when one side is a
     * local: see the rigid-body velocity trio in slice3_42.c, where the same
     * flip was worth 12 instructions. Was -55 bytes and 66 instructions
     * under the old double model. */
    float px, py, pz;
    float q0, q1, q2, q3;

    px = dt * pSrc->vel.x;
    py = dt * pSrc->vel.y;
    pz = dt * pSrc->vel.z;
    pDst->pos.x = pSrc->pos.x + px;
    pDst->pos.y = pSrc->pos.y + py;
    pDst->pos.z = pSrc->pos.z + pz;

    pDst->vel = pSrc->vel;

    q0 = dt * pSrc->qDot.f00;
    q1 = dt * pSrc->qDot.f04;
    q2 = dt * pSrc->qDot.f08;
    q3 = dt * pSrc->qDot.f0C;
    pDst->quat.f00 = pSrc->quat.f00 + q0;
    pDst->quat.f04 = pSrc->quat.f04 + q1;
    pDst->quat.f08 = pSrc->quat.f08 + q2;
    pDst->quat.f0C = pSrc->quat.f0C + q3;

    BrVec4Normalise(&pDst->quat);

    pDst->angVel = pSrc->angVel;
    pDst->qDot   = pSrc->qDot;
}
#else
void BrRbIntegrateState(BrRbState *pDst, const BrRbState *pSrc, float dt)
{
    /* SPILL MAP.  Seven products; three are spilled and reloaded, four are
     * not, and they alternate in a way no rule of thumb would predict:
     *
     *   10074623  fstp [esp+0x10]  dt*vel.z    reloaded 10074637  ROUNDED
     *   10074670  fstp [esp+0x0C]  dt*qDot[1]  reloaded 10074681  ROUNDED
     *   10074689  fstp [esp+0x14]  dt*qDot[3]  reloaded 100746A0  ROUNDED
     *
     * dt*vel.x, dt*vel.y, dt*qDot[0] and dt*qDot[2] are never stored: they go
     * straight into their `fadd` from the register (10074611, 1007461F,
     * 10074666, 10074695).  So pos.x and pos.y round once and pos.z rounds
     * twice; quat.f00 and quat.f08 round once and quat.f04 and quat.f0C round
     * twice.  There is no pattern to carry over -- each one was read off its
     * own instruction.
     *
     * (The reload at 100746A0 reads [esp+0x18], not [esp+0x14], because the
     * `push eax` at 10074697 moved esp by four between the store and the
     * load.  Same slot, different displacement -- the trap CONVENTIONS.md
     * records under the wheel-probe entry.)
     *
     * Every destination is `fstp dword`, so each expression rounds to float
     * at its store.  vel, angVel and qDot are copied as raw dwords by
     * integer moves and carry no arithmetic at all. */
    const double d = (double)dt;

    /* 10074611: fadd from the register */
    pDst->pos.x = (float)((double)pSrc->pos.x + d * (double)pSrc->vel.x);
    /* 1007461F: fadd st(1), also from the register */
    pDst->pos.y = (float)((double)pSrc->pos.y + d * (double)pSrc->vel.y);
    /* 10074623/10074637: spilled, reloaded, so the product is float first */
    pDst->pos.z = (float)((double)pSrc->pos.z
                          + (double)(float)(d * (double)pSrc->vel.z));

    pDst->vel = pSrc->vel;

    /* 10074666: from the register */
    pDst->quat.f00 = (float)((double)pSrc->quat.f00
                             + d * (double)pSrc->qDot.f00);
    /* 10074670/10074681: spilled and reloaded */
    pDst->quat.f04 = (float)((double)pSrc->quat.f04
                             + (double)(float)(d * (double)pSrc->qDot.f04));
    /* 10074695: from the register */
    pDst->quat.f08 = (float)((double)pSrc->quat.f08
                             + d * (double)pSrc->qDot.f08);
    /* 10074689/100746A0: spilled and reloaded */
    pDst->quat.f0C = (float)((double)pSrc->quat.f0C
                             + (double)(float)(d * (double)pSrc->qDot.f0C));

    BrVec4Normalise(&pDst->quat);

    pDst->angVel = pSrc->angVel;
    pDst->qDot   = pSrc->qDot;
}
#endif

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
/* @implements 0x1006D6B0 glide BrRbBuildMatrix */
void BrRbBuildMatrix(BrMat4 *pM, const BrRbState *pS)
{
    float a = pS->quat.f00;   /* w */
    float b = pS->quat.f04;   /* x */
    float aa = a * a;
    float bb = b * b;
    float c = pS->quat.f08;   /* y */
    float d = pS->quat.f0C;   /* z */
    float cc = c * c;
    float cb = pS->quat.f08 * pS->quat.f04;
    float da = pS->quat.f0C * pS->quat.f00;
    float db = pS->quat.f0C * pS->quat.f04;
    float ca = pS->quat.f08 * pS->quat.f00;
    float ab = aa - bb;
    float dc = pS->quat.f0C * pS->quat.f08;
    float ba = pS->quat.f04 * pS->quat.f00;
    float t2cb = cb + cb;
    float x = bb + aa - cc;
    float t2da = da + da;
    float t2db = db + db;
    float t2ca = ca + ca;
    float t2dc = dc + dc;
    float t2ba = ba + ba;
    float y = cc + ab;
    float dd = d * d;

    pM->m[0][0] = x - dd;
    pM->m[0][1] = t2da + t2cb;
    pM->m[0][2] = t2db - t2ca;
    pM->m[0][3] = 0.0f;
    pM->m[1][0] = t2cb - t2da;
    pM->m[1][1] = y - dd;
    pM->m[1][2] = t2ba + t2dc;
    pM->m[1][3] = 0.0f;
    pM->m[2][0] = t2ca + t2db;
    pM->m[2][1] = t2dc - t2ba;
    pM->m[2][2] = ab - cc + dd;
    pM->m[2][3] = 0.0f;
    pM->m[3][0] = pS->pos.x;
    pM->m[3][1] = pS->pos.y;
    pM->m[3][2] = pS->pos.z;
    pM->m[3][3] = 1.0f;
    BrStub8B80_1p(pM);
}

/* 0x10074870 BrRbInitInertia now lives in src/core/driving/br_rbinertia.c. */


/* ── Ghidra-matched functions ─────────────────────────── */
#ifdef BR_MATCHING_BUILD
extern int DAT_118ed1a0;
int FUN_1006c750();
int FUN_1006c800();
int FUN_1006c880();
int FUN_1006c8b0();



#endif /* BR_MATCHING_BUILD */
