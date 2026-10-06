/* br_collresp.c -- the collision half of 0x10067C30.
 *
 * Transcribed from reference/brally/orig/BRGlide.dll:
 *
 *   0x10066D70  1782 B   BrCollRespTipKick   the 1-or-2-wheel pitch kick
 *   0x1006DDD0   156 B   the overlapped call BrCollRespBuildBoxMatrix wraps
 *   0x10066260   644 B   BrCollRespBoxClassify   byte-exact 2026-09-05
 *   0x10066800   332 B   BrCollRespSegBox        byte-exact 2026-09-05
 *   0x10066610   492 B   BrCollRespPointInTri    PARKED, 2 regions (see it)
 *   0x10066950   322 B   BrCrExact               byte-exact 2026-09-27
 *   0x10066AD0   669 B   BrCollRespBroadPhase    byte-exact 2026-09-05
 *
 * !! THE FOUR ABOVE SHARE ONE TIE-BREAK.  Re-spelling any of them renumbers
 * the TU's symbols and can flip a float-chain choice in another: SegBox
 * went match -> diff with no edit to its text when its neighbours changed
 * (fixed by its declaration order; note above it).  Re-sweep the whole
 * file after touching any one and read EVERY row.
 *
 * See br_collresp.h for the contracts, for what 0x10067C30 really calls, and
 * for why the OBB half of the system is inert without the car data.
 *
 * ON THE COMPARISONS.  Same rule as br_phys.c and br_carphys.c: every test
 * below is the exact negation the x87 flag test implies.
 *     test ah,0x41 (C0|C3) taken -> "less, equal or unordered"
 *     test ah,1    (C0)    taken -> "less or unordered"
 *     test ah,0x40 (C3)    taken -> "equal or unordered"
 */
#include "br_collrespsolve.h"   /* br_globals: its objects */
#include "br_cartypes.h"   /* BrCarBody, the canonical record */
#include <math.h>
#include <stddef.h>
#include <string.h>

#define BrCollRespTipKick BrCollRespTipKick_port   /* header keeps the port signature */
#include "br_collresp.h"
#undef BrCollRespTipKick
#include "br_phys.h"      /* BrGroundHit                                  */
#include "slice1_08.h"    /* BrPlaneEval == 0x10065950 (D3D 0x1006C9A0)   */

uint32_t g_cBrCollRespTipKick;
uint32_t g_cBrCollRespDegenerate;
uint32_t g_cBrCollRespBroad;
uint32_t g_cBrCollRespGathered;
uint32_t g_cBrCollRespOverflow;
uint32_t g_cBrCollRespResponded;

/* @n64 0x8021C46C located */
/* (port-only BrCollRespCountersReset removed) */


/* ==================================================================== */
/* The cube's plane constants, all read out of BRGlide.dll               */
/*                                                                       */
/* 0x10077B48 / 0x10077B50 are +-0.5 and are DOUBLES (`fcom qword ptr`), */
/* as are 0x10077B58 / 0x10077B60 (+-1) and 0x10077AB0 / 0x10077B68      */
/* (+-1.5).  The compares are float-against-double, so they happen in    */
/* double and the boundary is the double's.  0x10077A78 is a FLOAT zero  */
/* and 0x10077B20 a DOUBLE zero; they are named apart because the        */
/* addresses are.                                                        */
/* ==================================================================== */
#define BR_CR_FACE_HI    ( 0.5)
#define BR_CR_FACE_LO    (-0.5)
#define BR_CR_EDGE_HI    ( 1.0)
#define BR_CR_EDGE_LO    (-1.0)
#define BR_CR_CORNER_HI  ( 1.5)
#define BR_CR_CORNER_LO  (-1.5)
#define BR_CR_ZERO_F     0.0f
#define BR_CR_ZERO_D     (0.0)

/* ==================================================================== */
/* The three-way sign classifier, again                                  */
/*                                                                       */
/* 0x10066DB9..0x10066DEE and the three identical blocks after it.  This  */
/* is the same idiom br_carphys.c exposes as BrCarPhysSign, at a          */
/* different address; it is written out here rather than reached for      */
/* across a module boundary, because the two are only equal by            */
/* coincidence of the compiler emitting the same sequence twice.          */
/*    `fcom 0` + `test ah,0x40` + `je` -> 0.0f for EQUAL OR UNORDERED     */
/*    `fcom 0` + `test ah,0x41` + `jne` -> -1.0f for LESS OR UNORDERED    */
/* A NaN therefore lands on 0.0f at the first test and never reaches the  */
/* second.                                                               */
/* ==================================================================== */
/* (port-only BrCrSign removed) */


/* ==================================================================== */
/* The frame, and the correction to 0x1006DDD0                           */
/* ==================================================================== */

/* @n64 0x80229530 located */
/* (port-only BrCollRespFrameMat removed) */


/* (port-only BrCollRespBuildBoxMatrix removed) */


/* (port-only BrCollRespBoxDegenerate removed) */


/* ==================================================================== */
/* 0x10066D70 -- the 1-or-2-wheel pitch kick                             */
/* ==================================================================== */

/* The body as this function reads it: the four wheel pointers, the wheel's
 * world point, the matrix, the saved state, the ground hit, the contact
 * counter (read as an int) and the four extent sources that lie past the
 * end of BrRbBodyFull.  A self-contained view so every read is a plain
 * member access. */
typedef struct BrTipView {
    float               f00;              /* 0x000                          */
    struct BrTipView   *child[4];         /* 0x004                          */
    unsigned char       pad014[0x78 - 0x14];
    BrVec3              f78;              /* 0x078  wheel point, world      */
    unsigned char       pad084[0xBC - 0x84];
    BrMat4              m;                /* 0x0BC                          */
    unsigned char       pad0FC[0x114 - 0xFC];
    BrRbState           state;            /* 0x114  saved state             */
    unsigned char       pad158[0x19C - 0x158];
    BrGroundHit         hit;              /* 0x19C  ground hit              */
    int32_t             f1B4;             /* 0x1B4  contact count           */
    unsigned char       pad1B8[0x1DC - 0x1B8];
    float               f1DC;             /* 0x1DC  x extent source         */
    float               f1E0;             /* 0x1E0  y extent source         */
    float               f1E4;             /* 0x1E4  z extent source         */
    float               f1E8;             /* 0x1E8  z base                  */
} BrTipView;







/* 64-bit core: declared once, in br_globals.h or its struct's header */               /* 0.0f                           */
/* 64-bit core: declared once, in br_globals.h or its struct's header */               /* +sign scale                    */
/* 64-bit core: declared once, in br_globals.h or its struct's header */               /* -sign scale                    */
/* 64-bit core: declared once, in br_globals.h or its struct's header */               /* 0.5f                           */
/* 64-bit core: declared once, in br_globals.h or its struct's header */               /* the first wheel's ceiling      */
/* 64-bit core: declared once, in br_globals.h or its struct's header */               /* the stand distance ceiling     */
/* 64-bit core: declared once, in br_globals.h or its struct's header */               /* the stand velocity ceiling     */
/* 64-bit core: declared once, in br_globals.h or its struct's header */               /* -2.0f, the kick gain           */
/* 64-bit core: declared once, in br_globals.h or its struct's header */             /* "...Stand Dist... %f"          */
/* 64-bit core: declared once, in br_globals.h or its struct's header */             /* "...Stand Vel... %f"           */
/* 64-bit core: declared once, in br_globals.h or its struct's header */             /* "...Stand Point... %f"         */
/* BrPodNop: prototype in br_funcs.h */

/* The sign select: the global zero when the coordinate is zero, else the
 * positive or negative scale.  The equality test comes first. */
/* Absolute value as the original spells it: the argument is read twice. */
#define BR_TIP_ABS(v) ((v) < BrCrK_Zero ? -(v) : (v))

#define BR_TIP_SIGN(x) ((x) == BrCrK_Zero ? BrCrK_Zero \
                        : ((x) > BrCrK_Zero ? DAT_10077a7c : DAT_10077a80))

/* Matching transcription 2026-09-13 (replaces the loop-form port in the
 * matching build): 1782/1782 B, 512/512 insns, register-blind multiset 0+0,
 * 63 positional diffs in four masked regions.  Levers that landed: the wheel
 * pointer is ONE variable assigned INSIDE the arm from a re-read of
 * `child[k]` (the test reads through the field; the original's block-1 skip
 * path reloads the uninitialised pointer from the slot `best` shares -- do
 * not initialise it); `count` is folded to `= 1` in block 1 by VC5 itself;
 * block 1 compares `t` against the GLOBAL ceiling DAT_10077b70, blocks 2-4
 * against `best` (`best >= t`); the z term needs `(double)f1E8 - f1E4 * K`
 * (fld f1E8 first, then the product, fsubp) -- the plain float form is
 * canonicalised to `fsubr [f1E8]`; the absolute value of `vn` is the
 * two-read conditional BR_TIP_ABS inside the compare (a store-form abs CSEs
 * the load); the +-0.1 kick is float stores, zeros are float stores (int
 * zeros form a zero web); the angular-velocity updates go through the body,
 * `pState` only feeds the last call.  The wheel dot `vn` MUST be grouped
 * `(nx*wx + ny*wy) + nz*wz`: the flat sum is re-associated to (t1+t3)+t2 and
 * emitted sequentially around the `add esp`/`sub esp` pair (-4 insns, the
 * four `fxch` of the preload shape); `x + (y + z)` restores the shape, the
 * explicit left group also restores the term order.
 * RESIDUE (allocation only): the `count` reload web is ecx in the original,
 * eax here (3 rows); `p` and `w` sit swapped in the frame (0x24/0x18 vs
 * 0x18/0x24); in the chassis dot `s` the original loads the normal first in
 * the two esi-based products (`fld ny; fmul m01`), ours loads the matrix
 * element first -- same association (ny*m01 + nz*m02) + nx*m00, same count.
 * DEAD for `s` (byte-identical or worse): every term permutation and
 * parenthesisation, operand swaps inside the products, a running sum, the
 * normal as plain members / a float[3] / a BrGroundHit / a BrGroundHit
 * pointer / a BrVec3 pointer / `(&hit.nx)[k]` / `((float *)&hit.nx)[k]`, the
 * matrix as BrMat4 via the body / via pM (un-folds to [ebp+k], -6 B) / a
 * named-scalar view cast from `&pBody->m` / `(&m.m[0][0])[k]`, a dead early
 * read of m01/m02; corpus MISS on both dot shapes.  DEAD for the colouring:
 * `count` as unsigned/long, `++count`, `+= 1`, `= count + 1`, declared first
 * or last; `w`/`p` declaration order, one declaration either order, an
 * early reference to `w` (+8 B), `best` declared last.
 * @t4-pass 0x10066D70 1 2026-09-13 probes 23 bytes 1782 insns 512 regions 4 rows 8 census no  (vn shape: 11 spellings, 12 groupings)
 * @t4-pass 0x10066D70 2 2026-09-13 probes 17 bytes 1782 insns 512 regions 4 rows 8 census yes  (s operand order: corpus MISS, symbol/offset/shape mechanism probes)
 * @t4-pass 0x10066D70 3 2026-09-13 probes 12 bytes 1782 insns 512 regions 4 rows 8 census no  (count web, p/w slot order)
 */
/* WHAT IT DOES: after rebuilding the body matrix from the saved state, walks
 * the four wheels: for every wheel with a ground contact it places the box
 * corner on that wheel's side (half extents, signed by the wheel's world
 * point, z lowered by half the z extent), transforms it to world, measures
 * its distance from the wheel's ground plane (made positive) and keeps the
 * smallest, remembering the last contacting wheel.  With exactly one or two
 * wheels touching and the corner within the stand distance, it takes the
 * corner's body-frame velocity, projects it on the last wheel's plane normal
 * and, if the corner is nearly at rest, kicks the saved angular velocity by
 * twice a small pitch vector (sign from the chassis plane's alignment with
 * the car's own axis), then refreshes the quaternion derivative.  Returns 1
 * when the kick was applied. */
/* @t3 0x10066D70 2026-09-13 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 1782/1782 insns 512/512 rows 4+4 regions 4 oracle UNCLASSIFIED
 * @t3-effort passes 3 zero-movement 2 3
 * Residue is allocation only: the `count` reload web coloured eax for ecx
 * (3 rows), the p/w frame slot pair swapped, and the operand order inside
 * the two esi-based products of the chassis dot `s`.  Dossier and dead
 * list are the comment block above this one.
 * Do not reopen before the end-grind. */
/* @implements 0x10066D70 glide BrCollRespTipKick */
int BrCollRespTipKick(BrTipView *pBody)
{
    BrRbState *pState;
    BrMat4    *pM;
    BrTipView *pW;
    BrVec3    *pN;
    int        count;
    float      best;
    BrVec3     p;
    BrVec3     w;
    float      t;
    float      vn;
    float      s;

    pM = &(*(struct BrMat4 *)&((BrCarBody *)(pBody))->rb.m);
    pState = &(*(struct BrRbState *)&((BrCarBody *)(pBody))->rb.st1);
    BrRbBuildMatrix(pM, pState);
    count = 0;
    best = 100.0f;

    if ((*(int32_t *)&((BrCarBody *)((*(struct BrTipView * (*)[4])&((BrCarBody *)(pBody))->rb.child)[0]))->rb.f1B4) != 0) {
        pW = (*(struct BrTipView * (*)[4])&((BrCarBody *)(pBody))->rb.child)[0];
        count++;
        p.x = ((*(float *)&((BrCarBody *)(pBody))->f01DC) * BrCrK_Half) * BR_TIP_SIGN((*(BrVec3 *)&((BrCarBody *)(pW))->rb.st).x);
        p.y = ((*(float *)&((BrCarBody *)(pBody))->f01E0) * BrCrK_Half) * BR_TIP_SIGN((*(BrVec3 *)&((BrCarBody *)(pW))->rb.st).y);
        p.z = (double)(*(float *)&((BrCarBody *)(pBody))->f01E8) - (*(float *)&((BrCarBody *)(pBody))->f01E4) * BrCrK_Half;
        BrMat4TransformPoint(&w, pM, &p);
        pN = (BrVec3 *)&(*(struct BrGroundHit *)&((BrCarBody *)(pW))->rb.pPlane).nx;
        if (BrCrPlaneDist(pN, (*(struct BrGroundHit *)&((BrCarBody *)(pW))->rb.pPlane).d, &w) < BrCrK_Zero)
            t = -BrCrPlaneDist(pN, (*(struct BrGroundHit *)&((BrCarBody *)(pW))->rb.pPlane).d, &w);
        else
            t = BrCrPlaneDist(pN, (*(struct BrGroundHit *)&((BrCarBody *)(pW))->rb.pPlane).d, &w);
        if (DAT_10077b70 >= t)
            best = t;
    }
    if ((*(int32_t *)&((BrCarBody *)((*(struct BrTipView * (*)[4])&((BrCarBody *)(pBody))->rb.child)[1]))->rb.f1B4) != 0) {
        pW = (*(struct BrTipView * (*)[4])&((BrCarBody *)(pBody))->rb.child)[1];
        count++;
        p.x = ((*(float *)&((BrCarBody *)(pBody))->f01DC) * BrCrK_Half) * BR_TIP_SIGN((*(BrVec3 *)&((BrCarBody *)(pW))->rb.st).x);
        p.y = ((*(float *)&((BrCarBody *)(pBody))->f01E0) * BrCrK_Half) * BR_TIP_SIGN((*(BrVec3 *)&((BrCarBody *)(pW))->rb.st).y);
        p.z = (double)(*(float *)&((BrCarBody *)(pBody))->f01E8) - (*(float *)&((BrCarBody *)(pBody))->f01E4) * BrCrK_Half;
        BrMat4TransformPoint(&w, pM, &p);
        pN = (BrVec3 *)&(*(struct BrGroundHit *)&((BrCarBody *)(pW))->rb.pPlane).nx;
        if (BrCrPlaneDist(pN, (*(struct BrGroundHit *)&((BrCarBody *)(pW))->rb.pPlane).d, &w) < BrCrK_Zero)
            t = -BrCrPlaneDist(pN, (*(struct BrGroundHit *)&((BrCarBody *)(pW))->rb.pPlane).d, &w);
        else
            t = BrCrPlaneDist(pN, (*(struct BrGroundHit *)&((BrCarBody *)(pW))->rb.pPlane).d, &w);
        if (best >= t)
            best = t;
    }
    if ((*(int32_t *)&((BrCarBody *)((*(struct BrTipView * (*)[4])&((BrCarBody *)(pBody))->rb.child)[2]))->rb.f1B4) != 0) {
        pW = (*(struct BrTipView * (*)[4])&((BrCarBody *)(pBody))->rb.child)[2];
        count++;
        p.x = ((*(float *)&((BrCarBody *)(pBody))->f01DC) * BrCrK_Half) * BR_TIP_SIGN((*(BrVec3 *)&((BrCarBody *)(pW))->rb.st).x);
        p.y = ((*(float *)&((BrCarBody *)(pBody))->f01E0) * BrCrK_Half) * BR_TIP_SIGN((*(BrVec3 *)&((BrCarBody *)(pW))->rb.st).y);
        p.z = (double)(*(float *)&((BrCarBody *)(pBody))->f01E8) - (*(float *)&((BrCarBody *)(pBody))->f01E4) * BrCrK_Half;
        BrMat4TransformPoint(&w, pM, &p);
        pN = (BrVec3 *)&(*(struct BrGroundHit *)&((BrCarBody *)(pW))->rb.pPlane).nx;
        if (BrCrPlaneDist(pN, (*(struct BrGroundHit *)&((BrCarBody *)(pW))->rb.pPlane).d, &w) < BrCrK_Zero)
            t = -BrCrPlaneDist(pN, (*(struct BrGroundHit *)&((BrCarBody *)(pW))->rb.pPlane).d, &w);
        else
            t = BrCrPlaneDist(pN, (*(struct BrGroundHit *)&((BrCarBody *)(pW))->rb.pPlane).d, &w);
        if (best >= t)
            best = t;
    }
    if ((*(int32_t *)&((BrCarBody *)((*(struct BrTipView * (*)[4])&((BrCarBody *)(pBody))->rb.child)[3]))->rb.f1B4) != 0) {
        pW = (*(struct BrTipView * (*)[4])&((BrCarBody *)(pBody))->rb.child)[3];
        count++;
        p.x = ((*(float *)&((BrCarBody *)(pBody))->f01DC) * BrCrK_Half) * BR_TIP_SIGN((*(BrVec3 *)&((BrCarBody *)(pW))->rb.st).x);
        p.y = ((*(float *)&((BrCarBody *)(pBody))->f01E0) * BrCrK_Half) * BR_TIP_SIGN((*(BrVec3 *)&((BrCarBody *)(pW))->rb.st).y);
        p.z = (double)(*(float *)&((BrCarBody *)(pBody))->f01E8) - (*(float *)&((BrCarBody *)(pBody))->f01E4) * BrCrK_Half;
        BrMat4TransformPoint(&w, pM, &p);
        pN = (BrVec3 *)&(*(struct BrGroundHit *)&((BrCarBody *)(pW))->rb.pPlane).nx;
        if (BrCrPlaneDist(pN, (*(struct BrGroundHit *)&((BrCarBody *)(pW))->rb.pPlane).d, &w) < BrCrK_Zero)
            t = -BrCrPlaneDist(pN, (*(struct BrGroundHit *)&((BrCarBody *)(pW))->rb.pPlane).d, &w);
        else
            t = BrCrPlaneDist(pN, (*(struct BrGroundHit *)&((BrCarBody *)(pW))->rb.pPlane).d, &w);
        if (best >= t)
            best = t;
    }

    if (count > 2 || count < 1)
        return 0;
    BrPodNop();
    if (best > DAT_10077b78)
        return 0;
    BrRbVelAtPoint(&w, (const BrRbBodyFull *)pBody, &p);
    vn = ((*(struct BrGroundHit *)&((BrCarBody *)(pW))->rb.pPlane).nx * w.x + (*(struct BrGroundHit *)&((BrCarBody *)(pW))->rb.pPlane).ny * w.y) + (*(struct BrGroundHit *)&((BrCarBody *)(pW))->rb.pPlane).nz * w.z;
    BrPodNop();
    if (BR_TIP_ABS(vn) > DAT_10077af4)
        return 0;
    /* x, y, z: the original's order (the decomp's byte-exact body) */
    s = pM->m[0][0] * (*(struct BrGroundHit *)&((BrCarBody *)(pBody))->rb.pPlane).nx + (*(struct BrMat4 *)&((BrCarBody *)(pBody))->rb.m).m[0][1] * (*(struct BrGroundHit *)&((BrCarBody *)(pBody))->rb.pPlane).ny
      + (*(struct BrMat4 *)&((BrCarBody *)(pBody))->rb.m).m[0][2] * (*(struct BrGroundHit *)&((BrCarBody *)(pBody))->rb.pPlane).nz;
    p.x = 0.0f;
    p.y = 0.1f;
    if (s <= BrCrK_Zero)
        p.y = -0.1f;
    p.z = 0.0f;
    BrPodNop();
    BrMat4MulVec3Transposed(&w, pM, &p);
    (*(struct BrRbState *)&((BrCarBody *)(pBody))->rb.st1).angVel.x = (*(struct BrRbState *)&((BrCarBody *)(pBody))->rb.st1).angVel.x - w.x * DAT_10077a84;
    (*(struct BrRbState *)&((BrCarBody *)(pBody))->rb.st1).angVel.y = (*(struct BrRbState *)&((BrCarBody *)(pBody))->rb.st1).angVel.y - w.y * DAT_10077a84;
    (*(struct BrRbState *)&((BrCarBody *)(pBody))->rb.st1).angVel.z = (*(struct BrRbState *)&((BrCarBody *)(pBody))->rb.st1).angVel.z - w.z * DAT_10077a84;
    BrRbQuatDerivative(pState);
    return 1;
}

/* ==================================================================== */
/* 0x10066260 -- the 26-plane outcode classify                           */
/*                                                                       */
/* Three stages, each an AND of per-vertex outcodes: if every vertex is   */
/* outside the SAME plane the triangle cannot meet the cube.             */
/*                                                                       */
/* Stage 1 doubles as the accept: a vertex whose outcode is zero is       */
/* inside all six slabs, so 0x100662EE returns 1 immediately.            */
/*                                                                       */
/* THE NaN ARMS.  `fcom hi` + `test ah,0x41` + `jne` sends less, equal    */
/* AND UNORDERED to the second test, where `fcomp lo` + `test ah,1` +     */
/* `je` keeps only "not less-or-unordered".  So a NaN sets the LOW bit    */
/* of the pair -- it classifies as below the plane, not above it and not  */
/* inside.  Every stage repeats that shape and it is written out each     */
/* time rather than factored, because factoring it is how it gets         */
/* inverted.                                                             */
/* ==================================================================== */

/* One plane pair: bit 0 for "above hi", bit 1 for "below lo". */
/* (port-only BrCrSide removed) */


/* 0x100664F0 -- stage 3, the eight corner planes.  Its argument is a
 * vertex and the mask still live; it returns the subset of that mask this
 * vertex is also outside.  The four sums are formed in the original's
 * order and association: (z + y) + x, (y + x) - z, (x - y) + z, (x - y) - z.
 * Written that way because the rounding differs from the tidy form. */
/* WHAT IT DOES: the third and last stage of the cheap collision test: it
 * checks a point against the eight corner planes that cut the corners off
 * the bounding box. It is given the set of planes the point might still be
 * outside and reports which of them it actually is outside. */
/* @implements 0x100664F0 glide BrCrCorner */
static unsigned BrCrCorner(const float aV[3], unsigned mask)
{
    unsigned out = 0u;
    float    t;

    if ((mask & 0x03u) != 0u) {
        /* The redundant paren round aV[2] is NOT decoration: it is what
         * makes VC5 `fld [ecx+8]` and `fadd [ecx+4]` instead of the other
         * way round.  Permuting the summands does nothing (VC5 canonicalises
         * a flat float sum); parenthesising the FIRST operand is the only
         * thing that moves the pair.  Without it this function is 2 bytes
         * out and, worse, reads `match` in report.csv while the image gate
         * fails -- see docs/brally/VC5-IDIOMS.md, "the leading-operand paren". */
        t = ((aV[2]) + aV[1]) + aV[0];
        if ((mask & 0x01u) != 0u && !((double)t <= BR_CR_CORNER_HI)) {
            out |= 0x01u;
        } else if ((mask & 0x02u) != 0u && !((double)t >= BR_CR_CORNER_LO)) {
            out |= 0x02u;
        }
    }
    if ((mask & 0x0Cu) != 0u) {
        t = (aV[1] + aV[0]) - aV[2];
        if ((mask & 0x04u) != 0u && !((double)t <= BR_CR_CORNER_HI)) {
            out |= 0x04u;
        } else if ((mask & 0x08u) != 0u && !((double)t >= BR_CR_CORNER_LO)) {
            out |= 0x08u;
        }
    }
    if ((mask & 0x30u) != 0u) {
        t = (aV[0] - aV[1]) + aV[2];
        if ((mask & 0x10u) != 0u && !((double)t <= BR_CR_CORNER_HI)) {
            out |= 0x10u;
        } else if ((mask & 0x20u) != 0u && !((double)t >= BR_CR_CORNER_LO)) {
            out |= 0x20u;
        }
    }
    if ((mask & 0xC0u) != 0u) {
        t = (aV[0] - aV[1]) - aV[2];
        if ((mask & 0x40u) != 0u && !((double)t <= BR_CR_CORNER_HI)) {
            out |= 0x40u;
        } else if ((mask & 0x80u) != 0u && !((double)t >= BR_CR_CORNER_LO)) {
            out |= 0x80u;
        }
    }
    return out;
}

/* WHAT IT DOES: the cheap three-stage test of a triangle (already in box
 * space) against the car's unit collision cube: 1 if some vertex is inside
 * it, 0 if one of the 26 face/edge/corner planes separates the triangle from
 * it, and -1 when neither is provable and the exact test has to decide. */
/* @implements 0x10066260 glide BrCollRespBoxClassify */
int BrCollRespBoxClassify(const float aV[9])
{
    unsigned mask;
    int      i;

    /* ---- stage 1, the six faces.  0x10066272..0x100662FD ------------- */
    /* Written out per component rather than through BrCrSide: a static
     * helper is never auto-inlined under /O2 (docs/brally/VC5-IDIOMS.md), and the
     * original's `fcom HI ... fcomp LO` pair is one load compared twice. */
    mask = 0xFFFFFFFFu;
    for (i = 0; i < 3; ++i) {
        const float *p = aV + i * 3;
        unsigned     c = 0u;
        float        t;

        /* Each component goes through the named `t`: that is what keeps
         * the one load on the stack across both compares (`fcom HI` then
         * `fcomp LO`, with `fstp st(0)` in the above-HI arm). */
        t = p[0];
        if (!((double)t <= BR_CR_FACE_HI)) {
            c |= 0x01u;
        } else if (!((double)t >= BR_CR_FACE_LO)) {
            c |= 0x02u;
        }
        t = p[1];
        if (!((double)t <= BR_CR_FACE_HI)) {
            c |= 0x04u;
        } else if (!((double)t >= BR_CR_FACE_LO)) {
            c |= 0x08u;
        }
        t = p[2];
        if (!((double)t <= BR_CR_FACE_HI)) {
            c |= 0x10u;
        } else if (!((double)t >= BR_CR_FACE_LO)) {
            c |= 0x20u;
        }

        /* 0x100662EE: a vertex inside every slab settles it. */
        if (c == 0u) {
            return 1;
        }
        mask &= c;
    }
    /* 0x10066303: any surviving bit is a separating face plane. */
    if (mask != 0u) {
        return 0;
    }

    /* ---- stage 2, the twelve edge planes.  0x1006630E..0x100664AF ---- */
    mask = 0xFFFFFFFFu;
    for (i = 0; i < 3; ++i) {
        const float *p = aV + i * 3;
        unsigned     c = 0u;
        float        t;

        /* The six sums, in the original's order: x+y, x-y, x+z, x-z,
         * y+z, y-z, each against +-1.  The FIRST one is an accumulation,
         * not a sum: `t = p[0] + p[1]` comes out `fld p[1]; fadd p[0]`
         * once the return below is the ternary (the tie-break moves with
         * the function), and neither the leading-operand paren nor the
         * declaration order moves it back -- `t += p[1]` does. */
        if ((mask & 0x003u) != 0u) {
            t = p[0];
            t += p[1];
            if ((mask & 0x001u) != 0u && !((double)t <= BR_CR_EDGE_HI)) {
                c |= 0x001u;
            } else if ((mask & 0x002u) != 0u
                       && !((double)t >= BR_CR_EDGE_LO)) {
                c |= 0x002u;
            }
        }
        if ((mask & 0x00Cu) != 0u) {
            t = p[0] - p[1];
            if ((mask & 0x004u) != 0u && !((double)t <= BR_CR_EDGE_HI)) {
                c |= 0x004u;
            } else if ((mask & 0x008u) != 0u
                       && !((double)t >= BR_CR_EDGE_LO)) {
                c |= 0x008u;
            }
        }
        if ((mask & 0x030u) != 0u) {
            t = p[0] + p[2];
            if ((mask & 0x010u) != 0u && !((double)t <= BR_CR_EDGE_HI)) {
                c |= 0x010u;
            } else if ((mask & 0x020u) != 0u
                       && !((double)t >= BR_CR_EDGE_LO)) {
                c |= 0x020u;
            }
        }
        if ((mask & 0x0C0u) != 0u) {
            t = p[0] - p[2];
            if ((mask & 0x040u) != 0u && !((double)t <= BR_CR_EDGE_HI)) {
                c |= 0x040u;
            } else if ((mask & 0x080u) != 0u
                       && !((double)t >= BR_CR_EDGE_LO)) {
                c |= 0x080u;
            }
        }
        if ((mask & 0x300u) != 0u) {
            t = p[1] + p[2];
            if ((mask & 0x100u) != 0u && !((double)t <= BR_CR_EDGE_HI)) {
                c |= 0x100u;
            } else if ((mask & 0x200u) != 0u
                       && !((double)t >= BR_CR_EDGE_LO)) {
                c |= 0x200u;
            }
        }
        if ((mask & 0xC00u) != 0u) {
            t = p[1] - p[2];
            if ((mask & 0x400u) != 0u && !((double)t <= BR_CR_EDGE_HI)) {
                c |= 0x400u;
            } else if ((mask & 0x800u) != 0u
                       && !((double)t >= BR_CR_EDGE_LO)) {
                c |= 0x800u;
            }
        }

        mask = c;
        /* 0x10066496: the moment the intersection empties, stage 2 is
         * done and stage 3 starts -- MID-LOOP, so the remaining vertices
         * are not classified at all. */
        if (mask == 0u) {
            break;
        }
    }
    if (mask != 0u) {
        return 0;
    }

    /* ---- stage 3, the eight corner planes.  0x100664B0..0x100664D9 --- */
    mask = 0xFFFFFFFFu;                /* `or eax,0xffffffff`, pushed whole */
    for (i = 0; i < 3; ++i) {
        mask = BrCrCorner(aV + i * 3, mask);
        if (mask == 0u) {
            break;
        }
    }
    /* 0x100664CE: `neg/sbb/neg/dec` -- 0 when the mask survived, -1 when
     * it emptied.  The TERNARY: `(mask != 0u) - 1` is `setne`/`dec`. */
    return (mask != 0u) ? 0 : -1;
}

/* ==================================================================== */
/* 0x10066800 -- segment versus the unit cube                            */
/*                                                                       */
/* The slab rejection uses the DIRECTION'S SIGN rather than a min/max, so */
/* it is written the original's way: sgn[i] is +1 unless d[i] is negative */
/* or NaN, and the two rejects are `A[i]*sgn[i] > 0.5` and                */
/* `B[i]*sgn[i] < -0.5`.  Then the three cross-product tests, with the    */
/* cube's half-extent as the FLOAT 0.5 at 0x10077AC8 -- not the double    */
/* the slab test uses.                                                   */
/* ==================================================================== */
/* WHAT IT DOES: does the line segment from A to B pass through the car's
 * collision box (the unit cube, in box space)?  It rejects quickly if the
 * segment lies wholly beyond one of the six faces, then tests the three
 * edge-direction cross products; returns 1 when the segment intersects. */
/* @implements 0x10066800 glide BrCollRespSegBox */
int BrCollRespSegBox(const BrVec3 *pA, const BrVec3 *pB)
{
    /* DECLARATION ORDER IS LOAD-BEARING: i, j, k BEFORE the two arrays.
     * This function was byte-exact with them last, then re-spelling the
     * two neighbours above and below it (0x10066260 / 0x10066610) flipped
     * ONE instruction here -- `fxch st(1); faddp st(2)` became
     * `fxch st(2); faddp st(1)` in the half-sum -- with no edit to this
     * text at all.  Of the 20 single-move orderings of these five lines,
     * exactly the four that put `i, j, k` ahead of `sgn` restore it; no
     * expression spelling (term swap, leading paren, factor swap) does. */
    int          i, j, k;
    const float *a = &pA->x;
    const float *b = &pB->x;
    int          sgn[3];
    float        d[3];

    d[0] = b[0] - a[0];
    d[1] = b[1] - a[1];
    d[2] = b[2] - a[2];

    /* 0x10066836: `fcomp 0.0f` + `test ah,1` + `je` -- +1 unless C0, so a
     * NaN direction takes -1. */
    for (i = 0; i < 3; ++i) {
        sgn[i] = !(d[i] >= BR_CR_ZERO_F) ? -1 : 1;
    }

    /* 0x10066871: the slabs.  Both compares are against DOUBLES. */
    for (i = 0; i < 3; ++i) {
        if (!((double)(a[i] * sgn[i]) <= BR_CR_FACE_HI)) {
            return 0;
        }
        if (!((double)(b[i] * sgn[i]) >= BR_CR_FACE_LO)) {
            return 0;
        }
    }

    /* 0x100668AF: the three axis pairs (1,2), (2,0), (0,1) -- the original
     * forms j and k with `idiv` each pass (`esi` is i+2 strength-reduced),
     * so they are computed modulo 3, not read from a table. */
    for (i = 0; i < 3; ++i) {
        float C;

        j = (i + 1) % 3;
        k = (i + 2) % 3;
        C = a[j] * d[k] - a[k] * d[j];

        /* 0x10066919: `fcompp` of h*h against C*C, then `test ah,1` --
         * REJECT on less-or-unordered, i.e. keep only h*h >= C*C.
         *
         * SPELLING IS LOAD-BEARING.  Only C is a named local: it is the
         * value the original `fst`s into the dead pB arg slot and re-reads
         * for C*C.  The half-sum h is written out TWICE and CSE'd -- naming
         * it (`S`, `h`, either or both, any statement order) puts the
         * chain in the other association, 23 B short, and ALSO flips the
         * sgn/d slot order.  And the compare is `C*C > h*h`, which is what
         * puts h*h in st(0) for the `test ah,1`.  The slab loop above
         * multiplies by sgn[i] IN PLACE for the same reason: a named
         * `float s = sgn[i]` turns `fld a; fmul st(1)` into
         * `fld st(0); fmul [a]`. */
        if (C * C > ((sgn[k] * d[j] + sgn[j] * d[k]) * BR_CR_HALF)
                    * ((sgn[k] * d[j] + sgn[j] * d[k]) * BR_CR_HALF)) {
            return 0;
        }
    }
    return 1;
}

/* ==================================================================== */
/* 0x10066950 -- the exact test, for the classify's -1                   */
/* ==================================================================== */
/* WHAT IT DOES: the expensive answer when the cheap classify said -1: the
 * triangle meets the unit cube if any edge passes through it, or else if
 * the cube's diagonal that points along the normal pierces the triangle.
 * The diagonal is s = sign(n) per axis (+-1 INTS, `fild`ed where used);
 * t = dot(n, v0) / dot(n, s) places the hit on it, |t| <= 0.5 keeps it
 * inside the cube (a NaN keeps going too: `test ah,0x41` + `jne`), and the
 * point t*s is handed to the point-in-triangle test.  A zero denominator
 * gives an infinity the window test rejects -- no guard, none needed.
 *
 * Byte-exact 2026-09-27.  Four source facts decide the x87 schedule:
 * the normal is read through the struct (`pN->x`) in the denominator and
 * through `n[]` in the numerator; v0 is its own BrVec3 pointer local; both
 * dot products are written flat (`a + b + c`, no inner parentheses); and
 * the function sits here, after SegBox and ahead of PointInTri, as in the
 * original.  Without them (float)s[1] stays in a register instead of
 * being spilled to pN's arg slot, the frame is 0x18 not 0x1c, and the
 * y/z and n0/v0 load roles swap. */
/* @implements 0x10066950 glide BrCrExact */
static int BrCrExact(const float aV[9], const BrVec3 *pN)
{
    const float *n = &pN->x;
    const BrVec3 *v0 = (const BrVec3 *)(const void *)aV;
    int          s[3];
    BrVec3       P;
    float        t;
    int          i;

    for (i = 0; i < 3; ++i) {
        if (BrCollRespSegBox((const BrVec3 *)(const void *)(aV + i * 3),
                             (const BrVec3 *)(const void *)
                             (aV + ((i + 1) % 3) * 3)) != 0) {
            return 1;
        }
    }
    for (i = 0; i < 3; ++i) {
        s[i] = !((&pN->x)[i] >= BR_CR_ZERO_F) ? -1 : 1;
    }
    t = (n[0] * v0->x + n[1] * v0->y + n[2] * v0->z)
      / (pN->x * s[0] + pN->y * s[1] + pN->z * s[2]);
    if (!((t - BR_CR_FACE_LO) * (t - BR_CR_FACE_HI) <= BR_CR_ZERO_D)) {
        return 0;
    }
    P.x = t * s[0];
    P.y = t * s[1];
    P.z = t * s[2];
    return BrCollRespPointInTri(aV, pN, &P);
}

/* ==================================================================== */
/* 0x10066610 -- point in triangle, by 2D crossing count                 */
/*                                                                       */
/* The projection drops the axis of the largest |n|, and WHICH of the     */
/* other two becomes u depends on the SIGN of n at that axis, so the      */
/* winding survives the projection.  The accumulator is a signed crossing */
/* count, not a boolean, and the original returns it as-is.               */
/* ==================================================================== */
/* WHAT IT DOES: is the point P inside the triangle?  Both are projected
 * onto the plane that drops the normal's largest axis (choosing the other
 * two so the winding survives), and the three edges are crossing-counted
 * against P; the signed count comes back as-is, non-zero meaning inside.
 *
 * NOT MATCHING 2026-09-27: 486/492 B, register-blind 0+2.  The four
 * crossing tests are `fld X; fld p; fcompp; test ah,1` in the original: both
 * operands loaded, which VC5 does for (double) operands or named float
 * locals, never for a bare `B[u] > p[u]` (that folds one into `fcomp m32`).
 * The (double) casts took this from 7+12 to 0+2.  Left: the fourth test
 * loads p[v] before A[v] and fxch's them, and `v` is re-read in each arm
 * where VC5 here hoists one read above the branch.  Inert on top of the
 * casts: (double)/(float) mixes and !(>=) forms of each test (1024);
 * inlined Lt/Gt helpers with float or double params (1024); the four cross
 * locals in all 24 orders or inline; if/else, else-if and continue arm
 * structures; the whole file vs a standalone TU. */
/* @t4-pass 0x10066610 1 2026-09-07 probes 150 bytes 485 insns 174 regions 5 rows 14 census yes  (tools/brally/crank.py) */
/* @t4-pass 0x10066610 2 2026-09-07 probes 131 bytes 485 insns 174 regions 5 rows 14 census yes  (tools/brally/crank.py) */
/* @t4-pass 0x10066610 3 2026-09-27 probes 1404 bytes 486 insns 176 regions 3 rows 2 census yes  (hand, after the (double) compares: 4th-compare forms and casts x !(>=) (1024), cross-local orders x arm structures (300+80); mechanism micro-test: fld/fld/fcompp only for (double) or named-float operands) */
/* @t4-pass 0x10066610 4 2026-09-27 probes 1030 bytes 486 insns 176 regions 3 rows 2 census no  (hand: inlined Lt/Gt helpers, float and double params, x4 forms (1024); function order in the TU; standalone TU vs whole file) */
/* @t3 0x10066610 2026-09-27 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 486/492 insns 176/178 rows 2+0 regions 3 oracle EQUIVALENT
 * @t3-effort passes 4 zero-movement 3 4
 * Residue is x87 scheduling only: the fourth crossing test loads p[v]
 * before A[v] (plus one fxch), and v is re-read in each arm where VC5 here
 * hoists one read above the branch.  Dossier and dead list in the header
 * above.  Do not reopen before the end-grind. */
/* @implements 0x10066610 glide BrCollRespPointInTri */
int BrCollRespPointInTri(const float aV[9], const BrVec3 *pN,
                         const BrVec3 *pP)
{
    const float *n = &pN->x;
    const float *p = &pP->x;
    float        aAbs[3];
    int          acc;
    int          dom, u, v, i;

    /* 0x10066628: `fcomp 0.0f` + `test ah,1` + `fchs` -- a NaN is negated
     * rather than left alone, which is the x87 absolute value this tree
     * documents in three other places. */
    for (i = 0; i < 3; ++i) {
        aAbs[i] = !(n[i] >= BR_CR_ZERO_F) ? -n[i] : n[i];
    }

    /* 0x10066644: the dominant axis, by the original's two-compare tree.
     * Each `test ah,0x41` + `jne` takes the SECOND arm for less, equal or
     * unordered, so ties go to the later axis.  The arms are TERNARIES on
     * 0/1 -- the original materialises the value with the 0 arm first
     * (`xor ecx,ecx; jmp`) and, in the second arm, adds one to it
     * (`lea ecx,[eax+1]`).  Spelled as booleans (`aAbs[0] <= aAbs[1]`) the
     * 1 arm comes first; spelled `? 1 : 2` the add disappears. */
    if (aAbs[0] > aAbs[2]) {
        dom = (aAbs[0] > aAbs[1]) ? 0 : 1;
    } else {
        dom = ((aAbs[1] > aAbs[2]) ? 0 : 1) + 1;
    }

    /* 0x10066688: the sign of n at the dominant axis picks the handedness
     * of the projection. */
    if (!(n[dom] >= BR_CR_ZERO_F)) {
        u = (dom + 2) % 3;
        v = (dom + 1) % 3;
    } else {
        u = (dom + 1) % 3;
        v = (dom + 2) % 3;
    }

    /* 0x100666DB: the three edges (v0,v1), (v1,v2), (v2,v0).  `acc = 0`
     * is a statement HERE, not an initialiser: the original zeroes it
     * after u and v are formed, sharing the `xor eax,eax` with i. */
    acc = 0;
    for (i = 0; i < 3; ++i) {
        const float *A = aV + i * 3;
        const float *B = aV + ((i + 1) % 3) * 3;
        int          su, sv;

        /* Each of these four is `fcompp` + `test ah,1`, i.e. 1 on
         * strictly-less-or-unordered, with both operands loaded -- the
         * (double) casts are what make VC5 load both. */
        su = ((double)B[u] > (double)p[u]) - ((double)A[u] > (double)p[u]);
        if (su != 0) {
            sv = ((double)B[v] > (double)p[v]) - ((double)A[v] > (double)p[v]);
            if (sv != 0) {
                float eu = B[u] - A[u];
                float ev = B[v] - A[v];
                float pu = p[u] - A[u];
                float pv = p[v] - A[v];

                /* 0x100667A8: `fcompp` of (eu*pv)*s against (pu*ev)*s, then
                 * `test ah,1` + `jne <skip>` -- so the crossing counts on
                 * greater-or-equal, and a NaN skips.  The multiply order is
                 * the original's: the two products are formed first and
                 * only then scaled by s. */
                if (!((pu * ev) * su > (eu * pv) * su)) {
                    acc += su;
                }
            } else {
                /* 0x100667C3: the edge does not straddle in v, so the
                 * crossing is decided by which side of P it lies --
                 * `test ah,0x41` + `je <skip>` counts it for
                 * less-or-equal-or-unordered. */
                if (A[v] <= p[v]) {
                    acc += su;
                }
            }
        }
    }
    return acc;
}

/* 0x10066AA0 -- classify, and resolve the inconclusive answer. */
/* WHAT IT DOES: decides whether a triangle touches the car's collision box.
 * It tries the cheap box test first, and only when that cannot say either
 * way does it fall through to the exact -- and much more expensive --
 * intersection test. */
/* @implements 0x10066AA0 glide BrCrTest */
int BrCrTest(const float aV[9], const BrVec3 *pN)
{
    int r = BrCollRespBoxClassify(aV);

    if (r != -1) {
        return r;
    }
    return BrCrExact(aV, pN);
}

/* ==================================================================== */
/* 0x10066230 / 0x10067C4E -- the candidate list                         */
/* ==================================================================== */

/* 64-bit core: declared once, in br_globals.h or its struct's header */              /* 0x11778198 */

/* 64-bit core: declared once, in br_globals.h or its struct's header */  /* 0x117781B0 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                  /* 0x11778844 */

/* (port-only BrCollRespListReset removed) */


/* The original's allocator is a POINTER bump cursor (0x11778844), not the
 * port's index -- and it has no bound, which the header explains is safe. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */           /* 0x11778844 */

/* WHAT IT DOES: remember one more surface the car is currently touching, by
 * pushing it onto the front of this frame's contact list. The collision
 * response then walks that list to work out which way to push the car.
 *
 * The original allocates by bumping a cursor with NO bound check. That is
 * safe rather than lucky: a cell holds at most 150 contacts and the pool is
 * 200 nodes. The port's copy below enforces the bound anyway and counts
 * refusals, because a port that silently wrote past the array would be a
 * worse bug than the one it models. */
/* @implements 0x10066230 glide BrCrListPush */
/* @n64 0x8025C24C located */
void BrCrListPush(const BrCollPlane *pPlane)
{
    BrCollRespNode *pN = g_pBrCrCursor++;

    pN->pPlane = pPlane;
    pN->pNext  = g_pBrCollRespList;
    g_pBrCollRespList = pN;
}

/* ==================================================================== */
/* 0x10066AD0 -- the gather                                              */
/*                                                                       */
/* THE WALK DIRECTION IS A GLOBAL.  0x10066B09 tests 0x11778848: non-zero */
/* walks the cell BACKWARDS from its last record, zero walks it forwards. */
/* The two arms are otherwise byte-identical (0x10066B33 and 0x10066C5D   */
/* are the same 214 bytes), and the backward arm reaches its start by     */
/* biasing the base by one record -- `lea esi,[edx+0x11773678]` with the  */
/* real base at 0x11773698.  Nothing in this port writes 0x11778848, so   */
/* it is modelled as the forward arm with the flag exposed rather than    */
/* two copies of the same loop.                                          */
/* ==================================================================== */

/* 64-bit core: declared once, in br_globals.h or its struct's header */               /* 0x11778848 */

/* The original reads the grid and its per-cell counts as the two static
 * arrays they are (0x10063DD0's reset names them the same way), takes the
 * cell straight from BrCollGridCellAcquire with no sign check, and neither
 * clamps the count nor counts anything but the gathered records.  The two
 * walk arms are byte-identical 214-byte copies, so the body is one macro
 * expanded in each; the port below keeps its single guarded loop. */
/* 64-bit core: declared once, in br_globals.h or its struct's header */  /* the grid      */
/* 64-bit core: declared once, in br_globals.h or its struct's header */                        /* cell counts   */

#define BR_CR_GATHER_ONE(pP)                                                \
    do {                                                                    \
        BrMat4TransformPoint((BrVec3 *)(void *)&aV[0], pMatBox, (pP)->pV0); \
        BrMat4TransformPoint((BrVec3 *)(void *)&aV[3], pMatBox, (pP)->pV1); \
        BrMat4TransformPoint((BrVec3 *)(void *)&aV[6], pMatBox, (pP)->pV2); \
        e1.x = aV[3] - aV[0]; e1.y = aV[4] - aV[1]; e1.z = aV[5] - aV[2];   \
        e2.x = aV[6] - aV[0]; e2.y = aV[7] - aV[1]; e2.z = aV[8] - aV[2];   \
        nrm.x = e2.z * e1.y - e2.y * e1.z;                                  \
        nrm.y = e2.x * e1.z - e2.z * e1.x;                                  \
        nrm.z = e2.y * e1.x - e2.x * e1.y;                                  \
        if (BrCrTest(aV, &nrm) != 0) {                                      \
            BrCrListPush(pP);                                               \
            ++n;                                                            \
        }                                                                   \
    } while (0)

/* WHAT IT DOES: the broad phase of car-versus-track collision: fetch the
 * grid cell under the car, transform every triangle record in it into the
 * car's box space, and keep -- on this frame's contact list -- each one
 * that touches the box.  Returns how many it kept.  A global picks whether
 * the cell is walked backwards or forwards. */
/* @implements 0x10066AD0 glide BrCollRespBroadPhase */
int BrCollRespBroadPhase(const BrRbBodyFull *pBody, const BrMat4 *pMatBox)
{
    float  aV[9];
    BrVec3 nrm, e1, e2;
    const BrCollPlane *pP;
    short  cell;
    int    count, i, n = 0;

    cell  = BrCollGridCellAcquire(pBody->m.m[3][0], pBody->m.m[3][1]);
    count = g_brCrPlane.aCellCount[cell];
    /* pP is the loop's OWN induction variable, stepped in the for clause.
     * Recomputing it from the index inside the body (`pP = &grid[cell][i]`)
     * makes VC5 bias its pointer register to the middle field (+0x14) and
     * spend a `lea eax,[esi-0x14]` on every push -- 10 B, 6 regions. */
    if ((*(int *)&g_brRaceBeginMirrorOff)) {
        pP = &DAT_11773698[cell][count - 1];
        for (i = count - 1; i >= 0; --i, --pP) {
            BR_CR_GATHER_ONE(pP);
        }
    } else {
        pP = DAT_11773698[cell];
        for (i = 0; i < count; ++i, ++pP) {
            BR_CR_GATHER_ONE(pP);
        }
    }
    return n;
}
#undef BR_CR_GATHER_ONE
