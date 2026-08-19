/* slice3_44.h -- decompiled from BRD3D.dll, packet 0x10071B80-0x10075F10.
 *
 * WHAT IS HERE
 *
 * The packet turned out to be three unrelated clusters plus a lot of
 * material that other slices already own.  Only the cluster that is pure,
 * self-contained arithmetic is ported here:
 *
 *   packed 3x3 matrix helpers   0x10074830 0x100749D0 0x10074A10 0x10074A50
 *                               0x10074A90 0x10074AC0 0x10074B20 0x10074C10
 *   4x4 helpers                 0x10074B70 0x10075340
 *   rigid-body integrator       0x100742D0 0x100743A0 0x10074450 0x100745F0
 *                               0x10074870
 *   opaque field store          0x100746E0
 *   8-dword global snapshot     0x10074E00 0x10074E20
 *
 * Everything else in the packet was skipped; see the report / slice3_44.c.
 *
 * TWO MATRIX LAYOUTS LIVE SIDE BY SIDE IN THIS MODULE.  br_mat.h's BrMat4 is
 * row-major with a 4-float (0x10 byte) row stride.  Several routines here
 * work on a PACKED 3x3 -- nine consecutive floats, 3-float (0x0C byte) row
 * stride -- and a handful convert between the two.  The distinction is read
 * straight off the pointer increments in the original (0x10 vs 0x0C) and is
 * the single easiest thing to get wrong in this file.
 *
 * FLOAT PRECISION.  This entry used to be a GENERAL DEVIATION covering every
 * float routine below: "the original ... keeps intermediates in 80-bit
 * registers ... results can differ in the last ulp ... unavoidable in
 * portable C".  All three of those are wrong.  The x87 here runs at 53-BIT
 * precision -- the CRT's control word is 0x027F, see CONVENTIONS.md -- so a
 * C `double` is an EXACT model of an unspilled intermediate, the difference
 * is not a last-ulp rounding artefact, and it is entirely avoidable.
 *
 * So the rule in this file is:
 *   - an intermediate the original does NOT store goes in a `double`;
 *   - an intermediate the original DOES store to a 32-bit slot is rounded
 *     through a `float` temporary at that point, because that is what the
 *     store does.  Widening those would be the same bug reversed.
 * The ORDER of every add/sub/mul is still preserved from the traced x87
 * stack.  Per-function notes below record which values are spilled and where.
 */
#ifndef SLICE3_44_H
#define SLICE3_44_H

#include "br_vec.h"
#include "br_mat.h"
#include "slice1_09.h"   /* BrVec4, BrVec4Normalise (0x100741B0) */

/* ================================================================== */
/* Packed 3x3                                                          */
/* ================================================================== */

/* Nine consecutive floats, row-major, row stride 3.  Element (i,j) is m[3*i+j];
 * the diagonal is m[0], m[4], m[8].  This is NOT a BrMat4 with the last column
 * dropped -- the row stride differs. */
typedef struct BrMat3 { float m[9]; } BrMat3;

/* 0x10074830  out[i] = sum_k pM->m[3*i+k] * v[k].
 *
 * The accumulator is zeroed and then re-loaded/re-stored through memory once
 * per term, so each partial sum is rounded to float; that is reproduced.
 *
 * GOTCHA: the matrix pointer advances CONTINUOUSLY through all nine floats
 * while the vector pointer is reset per row.  Contrast BrMat4MulVec3
 * (br_mat.h, 0x10074720), whose matrix argument is arg2 and whose vector is
 * arg3 -- here the matrix is also arg2, but with a 0x0C row stride. */
void BrMat3MulVec3(BrVec3 *pOut, const BrMat3 *pM, const BrVec3 *pV);

/* 0x10074AC0  out = a * b   (out[3i+j] = sum_k a[3i+k] * b[3k+j]).
 *
 * Destination FIRST -- unlike BrMat4Mul (0x100306C0), which puts it last.
 * The three products are summed as (a1*b[3+j] + a0*b[j]) + a2*b[6+j]; that
 * order is from the x87 trace.
 *
 * The output pointer advances continuously, so out must not alias a or b. */
void BrMat3Mul(BrMat3 *pOut, const BrMat3 *pA, const BrMat3 *pB);

/* 0x100749D0  out = the skew-symmetric cross-product matrix of v:
 *
 *      [  0   -vz   vy ]
 *      [  vz   0   -vx ]
 *      [ -vy   vx   0  ]
 *
 * so that out * u == v x u under BrMat3MulVec3.  Every one of the nine slots
 * is written, three of them by an explicit zero store. */
void BrMat3Skew(BrMat3 *pOut, const BrVec3 *pV);

/* 0x10074C10  solve pM * x = v for x by Cramer's rule; x is written to pOut.
 *
 * The original computes the determinant NEGATED (it expands with the signs
 * flipped), takes 1/that once, and compensates by negating only the second
 * component.  Reproduced exactly -- the intermediate `det` this port names is
 * -det(pM), and pOut->y carries the extra minus sign.
 *
 * There is NO singularity guard: a singular matrix yields +-inf / NaN. */
void BrMat3Solve(BrVec3 *pOut, const BrMat3 *pM, const BrVec3 *pV);

/* ---- BrMat4 (0x10 row stride) -> BrMat3 (0x0C row stride) ---------- */

/* 0x10074A90  out[3i+j] = pSrc->m[i][j]   (drop the 4th column/row). */
void BrMat4ToMat3(BrMat3 *pOut, const BrMat4 *pSrc);

/* 0x10074A50  out[i + 3j] = pSrc->m[i][j]  (drop and transpose). */
void BrMat4ToMat3Transposed(BrMat3 *pOut, const BrMat4 *pSrc);

/* 0x10074A10  both of the above in one pass over pSrc.
 *
 * GOTCHA: the argument order is (transposed, straight, source).  arg1 gets
 * the TRANSPOSE, arg2 gets the straight copy.  Each source element is stored
 * to arg1 first and arg2 second, which only matters if they alias. */
void BrMat4ToMat3Both(BrMat3 *pTransposed, BrMat3 *pStraight,
                      const BrMat4 *pSrc);

/* 0x10074B20 (Glide 0x1006DD80)  a 3x3 MATRIX subtract: nine floats, once each.
 *
 * CORRECTED. This was BrVec3SubRepeated, documented as a preserved bug in
 * which "the outer loop resets the cursor" so three subtractions ran three
 * times. The outer loop does not reset it -- the reset is one instruction
 * before the loop target -- and 0x10065C80 passes a 3x3 identity scaled by
 * 1/mass, which only makes sense for nine elements. See slice3_44.c. */
void BrMat3Sub(float *pOut, const float *pA, const float *pB);

/* 0x10075340  pM->m[0][3] = m[1][3] = m[2][3] = 0, pM->m[3][3] = 1.0f.
 * Only the fourth COLUMN is touched; the upper 3x3 and the translation row
 * are left alone.  __thiscall in the original. */
void BrMat4SetLastColumn(BrMat4 *pM);

/* 0x10074B70  build pOut from a transposed, per-column-scaled copy of pA and
 * a translation taken from pS.  Positional names: the meaning of pS's row 0
 * is NOT established (see below).
 *
 *   pOut->m[i][j] = pA->m[j][i] * pS->m[0][j]      i,j in 0..2
 *   pOut->m[i][3] = 0                              i in 0..2
 *   pOut->m[3][3] = 1
 *   pOut->m[3][*] = (-pS->m[3][0], -pS->m[3][1], -pS->m[3][2]) rotated by
 *                   pOut via BrMat4MulVec3Transposed (br_mat.h, 0x10074770)
 *
 * GOTCHA: pS is indexed at BOTH +0x00..+0x08 (three per-column scale factors)
 * and +0x30..+0x38 (the translation).  If pS really is a BrMat4 then its
 * first row is being used as a scale vector, which is odd; the type here is
 * BrMat4 only because +0x30 is a 4x4's translation row.  Do not read
 * semantics into pS->m[0] beyond "three floats at offset 0".
 *
 * GOTCHA: the argument order is (pA, pOut, pS) -- the DESTINATION IS SECOND.
 */
void BrMat4BuildScaledTransposed(const BrMat4 *pA, BrMat4 *pOut,
                                 const BrMat4 *pS);

/* ================================================================== */
/* Rigid-body state and integrator                                     */
/* ================================================================== */

/* The 0x44-byte state block threaded through 0x100742D0 / 0x100743A0 /
 * 0x10074450 / 0x100745F0.  The field meanings are not guesses: the four
 * routines close a loop that pins them.
 *
 *   0x100743A0  vel   += dt * body-acceleration
 *               angVel+= dt * body-angular-acceleration
 *   0x100742D0  qDot   = 0.5 * (0, angVel) (x) quat      <- exact quaternion
 *                                                           derivative, all
 *                                                           four terms match
 *   0x100745F0  pos   += dt * vel ;  quat += dt * qDot ; renormalise
 *   0x10074450  quat  -> a rotation matrix, pos -> its translation row
 *
 * The quaternion is stored SCALAR FIRST: quat.f00 is w, f04/f08/f0C are
 * x/y/z.  That is forced by the derivative in 0x100742D0 and confirmed by the
 * matrix built in 0x10074450. */
typedef struct BrRbState {
    BrVec3 pos;      /* 0x00 */
    BrVec3 vel;      /* 0x0C */
    BrVec4 quat;     /* 0x18  w,x,y,z */
    BrVec3 angVel;   /* 0x28 */
    BrVec4 qDot;     /* 0x34  w,x,y,z */
} BrRbState;         /* 0x44 */

/* The larger body block.  0x10074870 touches offsets up to 0x1D8; 0x100743A0
 * reads a second block's +0xFC..+0x110.  ASSUMPTION: those are the same
 * struct.  The two functions use strictly disjoint offsets, so nothing here
 * is contradicted by the disassembly, but the identification is inference
 * from the module they sit in, not proof.  Split the type if that turns out
 * to be wrong -- no code in this file depends on the union. */
typedef struct BrRbBody {
    /* 0x00 -- six dwords cleared by BrRbInitInertia.  Type unestablished;
     * declared float because a raw 0 dword is also +0.0f. */
    float f00, f04, f08, f0C, f10, f14;
    float f18;                       /* 0x18  not touched by the ctor      */
    int   mode;                      /* 0x1C  see BrRbInitInertia          */
    float dim[3];                    /* 0x20  box extents (x,y,z)          */
    float mass;                      /* 0x2C                               */
    BrMat3 inertia;                  /* 0x30  packed 3x3                   */
    BrMat3 invInertia;               /* 0x54  packed 3x3                   */
    unsigned char pad78[0xFC - 0x78];
    float accel[3];                  /* 0xFC                               */
    float angAccel[3];               /* 0x108                              */
    unsigned char pad114[0x19C - 0x114];
    float f19C;                      /* 0x19C cleared                      */
    unsigned char pad1A0[0x1B4 - 0x1A0];
    float f1B4;                      /* 0x1B4 cleared                      */
    unsigned char pad1B8[0x1C0 - 0x1B8];
    float f1C0;                      /* 0x1C0 set to 0.174f                */
    float f1C4;                      /* 0x1C4 cleared                      */
    float f1C8;                      /* 0x1C8 set to 0.5f                  */
    float f1CC;                      /* 0x1CC cleared                      */
    float f1D0;                      /* 0x1D0 cleared                      */
    float f1D4;                      /* 0x1D4 cleared                      */
    float f1D8;                      /* 0x1D8 cleared                      */
} BrRbBody;                          /* 0x1DC */

/* 0x100742D0  pS->qDot = 0.5 * (0, pS->angVel) (x) pS->quat.
 *
 * The three half-angular-velocity terms are each rounded to float before use
 * (the original spills them to 4-byte stack slots), which is reproduced. */
void BrRbQuatDerivative(BrRbState *pS);

/* 0x100743A0  pS->vel += dt * pBody->accel, pS->angVel += dt * pBody->angAccel.
 * Six independent accumulations; nothing else in either struct is read. */
void BrRbIntegrateVelocity(BrRbState *pS, const BrRbBody *pBody, float dt);

/* 0x100745F0  explicit-Euler step from pSrc into pDst:
 *
 *   pDst->pos  = pSrc->pos  + dt * pSrc->vel
 *   pDst->vel  = pSrc->vel                       (copied verbatim)
 *   pDst->quat = pSrc->quat + dt * pSrc->qDot,  then BrVec4Normalise'd
 *   pDst->angVel, pDst->qDot = pSrc's           (copied verbatim)
 *
 * pDst == pSrc is safe and is the expected use.
 *
 * GOTCHA: the normalisation is BrVec4Normalise (0x100741B0), which has NO
 * zero-length guard -- an all-zero quaternion produces NaNs. */
void BrRbIntegrateState(BrRbState *pDst, const BrRbState *pSrc, float dt);

/* 0x10074450  pM = the row-vector rotation matrix of pS->quat, with pS->pos
 * as the translation ROW (m[3][0..2]) and m[3][3] = 1.  m[*][3] = 0.
 *
 * Row-vector convention (v' = v * M), matching BrMat4Frustum and
 * BrVec3MulMat4Point (slice1_09, 0x100747C0).  The diagonal is written in the
 * un-normalised form w^2+x^2-y^2-z^2 etc., so a non-unit quaternion scales
 * the matrix rather than being silently normalised.
 *
 * The original then calls the 0x10008B80 stub with pM; that call is kept. */
void BrRbBuildMatrix(BrMat4 *pM, const BrRbState *pS);

/* 0x10074870  reset pB and build its inertia tensor.
 *
 * Clears +0x00..+0x14, +0x19C, +0x1B4, +0x1C4, +0x1CC, +0x1D0, +0x1D4,
 * +0x1D8; sets +0x1C0 = 0.174f and +0x1C8 = 0.5f; writes the 3x3 IDENTITY
 * into pB->inertia; then:
 *
 *   mode in [0,1] : inertia diagonal = the solid-box tensor
 *                     ((z^2+y^2), (x^2+z^2), (x^2+y^2)) * mass / 12
 *                   and 0x10075330 is called with &inertia.
 *   mode != 2     : invInertia diagonal = 1 / inertia diagonal.
 *
 * GOTCHA: only the three DIAGONAL entries of invInertia are ever written.
 * Its six off-diagonal entries are left exactly as they were -- and per the
 * contract 0x1007DFE0 (operator new) does not zero memory, so a freshly
 * allocated body has garbage there.  Do not "helpfully" zero them.
 *
 * GOTCHA: mode outside [0,1] leaves inertia as the identity, so mode == 3
 * (say) yields inertia = I and invInertia diagonal = 1.  mode == 2 leaves
 * invInertia completely untouched.
 *
 * GOTCHA: the identity fill runs unconditionally and BEFORE the box formula,
 * so the box tensor's off-diagonal terms are zero and its diagonal overwrites
 * the ones. */
void BrRbInitInertia(BrRbBody *pB);

/* ================================================================== */
/* Misc                                                                */
/* ================================================================== */

/* 0x100746E0  seven dword stores into pDst.
 *
 * GOTCHA (this is the whole point of the function): the LAST argument lands
 * in slot 1 and the remaining six shift down, and slot 0 is never written.
 *
 *   pDst[1] = a8;  pDst[2] = a2;  pDst[3] = a3;  pDst[4] = a4;
 *   pDst[5] = a5;  pDst[6] = a6;  pDst[7] = a7;
 *
 * The values move through integer registers in the original, so the element
 * type is not established; unsigned int keeps the bits intact either way. */
void BrX100746E0(unsigned int *pDst,
                 unsigned int a2, unsigned int a3, unsigned int a4,
                 unsigned int a5, unsigned int a6, unsigned int a7,
                 unsigned int a8);

/* The eight-dword block at 0x11829850. */
extern unsigned int g_BrX1829850[8];

/* 0x10074E00  zero all eight dwords.
 * Name taken from the existing XSLICE declaration in slice2_20.c. */
void BrSub10074E00(void);

/* 0x10074E20  copy all eight dwords out to pDst. */
void BrSub10074E20(unsigned int *pDst);

#endif /* SLICE3_44_H */
