/* slice2_21.h -- 0x10039200 .. 0x1003BC90, decompiled from BRD3D.dll.
 *
 * Four unrelated clusters share this address range:
 *
 *   1. Float geometry leaf routines that br_vec.h/br_mat.h do NOT cover.
 *      br_vec.h owns 0x1003AC30-0x1003B130; the routines here (0x1003ADA0,
 *      0x1003AE50, 0x1003B0A0, 0x1003B170, 0x1003B1C0) sit in the gaps of
 *      that run and are NOT duplicates -- check the addresses, not the names.
 *
 *   2. A 4x4 matrix cluster (multiply, invert, transform) at
 *      0x1003B2A0-0x1003B4F0. Row-major, ROW-VECTOR convention (v' = v * M),
 *      the same layout br_mat.h documents for BrMat4Frustum.
 *
 *   3. A 64x64 coverage grid built by rasterising the twelve edges of a
 *      six-point hull. This extends the grid br_span.h already models: it adds
 *      a column range and a per-column row range to BrSpanGrid's per-row
 *      column ranges. BrSpanAdd/BrSpanTest are reused from br_span.h, not
 *      reimplemented.
 *
 *   4. A 256-entry particle pool with an intrusive free list, plus the two
 *      per-frame integrators and the spawner that feeds it from a car's four
 *      wheels.
 *
 * NAME COLLISIONS RESOLVED HERE -- read before wiring anything up:
 *   0x1003B170 is NOT in this file. br_vec.c already ports it as
 *     BrVec3Length; slice2_20.c and slice2_11.h each extern it under a
 *     DIFFERENT name (BrVec3Len, BrVec3Length). One address, three names.
 *   0x1003AE50 is exported as BrVec3NormaliseGuard, not BrVec3Normalise:
 *     slice1_09 already DEFINES BrVec3Normalise for the other, unguarded
 *     normaliser at 0x10074180, while slice2_18.h externs the name for THIS
 *     address. Those two cannot both be `BrVec3Normalise`; the coordinator
 *     must repoint slice2_18's extern at BrVec3NormaliseGuard.
 *   0x1003B470 is exported as BrMtxMul, not BrMat4Mul: slice1_05 already
 *     defines BrMat4Mul for 0x100306C0, which is a different routine and
 *     takes its destination LAST. The two matrix multiplies are not
 *     interchangeable.
 *   0x1003B2A0 keeps slice2_18.h's name AND its exact `const float *`
 *     signature (BrMat4TransformPoint4) so the two declarations agree.
 *   0x1007C8A0 is externed as BrFtolTrunc because several other slices carry
 *     a file-static `BrFtol` with a `double` parameter.
 *
 * GLOBALS: the original reads its state from fixed addresses. Following the
 * precedent set by br_span.c, that state is passed in by pointer here instead
 * of being redeclared as globals (several other slices already declare some of
 * these, e.g. slice2_18.h's BrG_6C6620, and duplicate definitions would not
 * link). Every struct field carries the address it stands for.
 */
#ifndef SLICE2_21_H
#define SLICE2_21_H

#include <stdint.h>

#include "br_vec.h"
#include "br_mat.h"
#include "br_span.h"

/* ==========================================================================
 * 0. Cross-slice leaves
 * ========================================================================== */

/* 0x10002240 is `fld dword [esp+4]; fsin; ret`  -- sinf.  */
/* XSLICE 0x10002240 */
extern float BrSinF(float x);

/* 0x10002250 is `fld dword [esp+4]; fsqrt; ret` -- sqrtf. */
/* XSLICE 0x10002250 */
extern float BrSqrtF(float x);

/* 0x1007C8A0 is __ftol: truncate toward zero, low dword of a `fistp qword`.
 * (Known-correct fact from the contract.) Out-of-range inputs therefore give
 * the low dword of the x87 integer indefinite, i.e. 0 -- not a clamp. */
/* XSLICE 0x1007C8A0 */
extern int32_t BrFtolTrunc(float f);

/* 0x1002B920 is `fld dword [esp+4]; jmp __ftol` -- the same conversion
 * reached through a stack argument. Kept separate because it is a separate
 * address that other slices may also name. Verified by disassembly: the whole
 * body is those two instructions. */
/* XSLICE 0x1002B920 */
extern int32_t BrFtolArg(float f);

/* 0x1003BD50 -- the 27-bit generator, already ported in slice2_22. */
/* XSLICE 0x1003BD50 */
extern uint32_t BrDPlayRandStep(uint32_t *pSeed);

/* ==========================================================================
 * 1. Float geometry leaves (the gaps in br_vec.h's run)
 * ========================================================================== */

/* A 2-component point. 0x1003BA70 and 0x1003BC90 read only offsets 0 and 4 of
 * everything they are handed, so their operands are genuinely 2D. */
typedef struct BrVec2 { float x, y; } BrVec2;

/* 0x1003ADA0  out = normalise(to - from).
 *
 * GOTCHA: the subtrahend comes SECOND and the minuend THIRD -- the routine
 * computes arg3 - arg2, not arg2 - arg3.
 * GOTCHA: a zero-length difference yields (0, 0, 1), NOT a zero vector and
 * not the input. Contrast BrVec3dNormalise (br_vecd.h) which leaves a zero
 * vector untouched: the two normalisers in this engine disagree. */
void BrVec3Direction(BrVec3 *pOut, const BrVec3 *pFrom, const BrVec3 *pTo);

/* 0x1003AE50  normalise IN PLACE. Zero length yields (0, 0, 1). */
void BrVec3NormaliseGuard(BrVec3 *pV);

/* 0x1003B170  |v| */
float BrVec3Len(const BrVec3 *pV);

/* 0x1003B1C0  |(v.x, v.y)| -- z is not read. */
float BrVec3LenXY(const BrVec3 *pV);

/* 0x1003B0A0  |(a.x - b.x, a.y - b.y)| -- z is not read. */
float BrVec3DistXY(const BrVec3 *pA, const BrVec3 *pB);

/* 0x1003B7B0  atan2 by bisection, result normalised to [0, 2*pi).
 *
 * Argument order is (x, y), i.e. the OPPOSITE of C's atan2(y, x).
 *
 * The original reduces the angle into [0, pi/4] by (a) negating both
 * components and adding pi when y < 0, (b) rotating (x,y) -> (y,-x) and adding
 * pi/2 while x < 0, (c) swapping x and y and adding pi/4 when x < y, then runs
 * a fixed 16-step bisection on sin() over that octant with an early exit once
 * |sin(ang) - y/r| < 0.005. Accuracy is therefore about 0.01 rad, not machine
 * precision -- do not substitute atan2f() if bit-comparing against the game.
 *
 * Returns exactly 0.0f when x == y == 0. */
float BrAtan2(float x, float y);

/* ==========================================================================
 * 2. 4x4 matrices  (row-major, row-vector: v' = v * M)
 * ========================================================================== */

/* 0x1003B2A0  pOut[j] = sum_k m[k][j] * v[k] + m[3][j],  j = 0..3.
 * The w row (m[3]) is added, so this transforms a POINT, and it produces four
 * components -- pOut must have room for 4 floats, not 3.
 * NOTE the argument order: (out, vector, matrix). br_mat.h's BrMat4MulVec3
 * takes (out, matrix, vector). Preserved, not harmonised.
 * The matrix is a bare `const float *` to match slice2_18.h's declaration of
 * the same address; it is still 16 floats in BrMat4's row-major layout. */
void BrMat4TransformPoint4(float pOut[4], const BrVec3 *pV, const float *pM);

/* 0x1003B3F0  out[j] = sum_k m[k][j] * v[k],  j = 0..2.
 * Same as above minus the translation and minus the fourth component, i.e. a
 * direction transform. Argument order again (out, vector, matrix). */
void BrMtxXfmDir3(BrVec3 *pOut, const BrVec3 *pV, const BrMat4 *pM);

/* 0x1003B470  out = a * b  (full 4x4), DESTINATION FIRST.
 * Not to be confused with slice1_05's BrMat4Mul (0x100306C0), which is a
 * different routine and puts the destination last.
 * The original accumulates into a 64-byte stack temp and copies it out last,
 * so pOut may alias pA or pB. */
void BrMtxMul(BrMat4 *pOut, const BrMat4 *pA, const BrMat4 *pB);

/* 0x1003B4F0  invert an affine 4x4: 3x3 adjugate/det, then
 * out[3] = -m[3] * out3x3, with column 3 forced to (0,0,0,1).
 *
 * Degenerate input (det == 0, or |det| tiny relative to the sum of the
 * determinant's terms) writes the identity instead.
 *
 * GOTCHA: the return value is 1 on BOTH paths -- the original loads eax=1 at
 * 0x1003B6DE for success and again at 0x1003B79E for the identity fallback.
 * It is useless as a success flag. Reproduced as-is; do not "fix" it to 0. */
int BrMtxInvert(BrMat4 *pOut, const BrMat4 *pM);

/* ==========================================================================
 * 3. 2D segment predicates
 * ========================================================================== */

/* 0x1003BC90  which side of the directed line a->b do c and d fall on?
 *
 *   0  both strictly on the same side (no straddle)
 *   2  cross(c) == cross(d)
 *   1  otherwise
 *
 * GOTCHA: 2 is only ever reachable when BOTH cross products are zero, i.e.
 * c and d both lie on the line a-b. Two equal non-zero crosses are on the
 * same side by definition, so the 0 rejection fires first.
 *
 * Note the argument order: the LINE is (arg1, arg2) and the two probe points
 * are (arg3, arg4). */
int BrSeg2SideTest(const BrVec2 *pA, const BrVec2 *pB,
                   const BrVec2 *pC, const BrVec2 *pD);

/* 0x1003BA70  do segments a-b and c-d intersect?
 *
 *   0  no (bounding boxes disjoint on x or y, or one straddle test fails)
 *   2  they meet and both of c, d lie ON the line a-b (see BrSeg2SideTest)
 *   1  they meet otherwise
 *
 * GOTCHA: the 1-vs-2 distinction is decided ONLY from the first straddle test
 * (c,d against line a-b). The second test (a,b against line c-d) can reject
 * with 0 but never influences the 1/2 choice. */
int BrSeg2Intersect(const BrVec2 *pA, const BrVec2 *pB,
                    const BrVec2 *pC, const BrVec2 *pD);

/* ==========================================================================
 * 4. Coverage grid  (extends br_span.h)
 * ========================================================================== */

/* The grid the original keeps at 0x10A9BBC0..0x10A9BF50. `grid` is exactly
 * br_span.h's BrSpanGrid (aMin 0x10A9BBD0, aMax 0x10A9BCD0, rowLo 0x10A9BBCC,
 * rowHi 0x10A9BBC4); the rest lives around it.
 *
 * aMin/aMax are the COLUMN range touched in each row; aRowLo/aRowHi are the
 * ROW range covering each column, derived from the former by BrSpanBuildHull.
 *
 * GOTCHA: the empty markers are 64 and 0, not INT_MAX/INT_MIN. BrSpanReset in
 * br_span.c uses INT_MAX/INT_MIN, which is a different (and incompatible)
 * emptiness convention -- BrSpanBuildHull below establishes the one the
 * original 0x1003A990 actually writes. */
typedef struct BrSpanVolume {
    BrSpanGrid grid;                /* 0x10A9BBD0 / 0x10A9BCD0 / CC / C4 */
    int        colLo;               /* 0x10A9BBC8, starts at 63 */
    int        colHi;               /* 0x10A9BBC0, starts at 0  */
    int        aRowLo[BR_SPAN_ROWS];/* 0x10A9BDD0, starts at 64 */
    int        aRowHi[BR_SPAN_ROWS];/* 0x10A9BED0, starts at 0  */
} BrSpanVolume;

/* Cell size: the original multiplies every world coordinate by 0.03125 before
 * converting to a cell index, and by 32.0 to go back. */
#define BR_SPAN_CELL 32.0f

/* 0x1003A6B0  rasterise the segment (x0,y0)-(x1,y1) into the grid.
 *
 * Adds both endpoints, widens colLo/colHi from the endpoint columns and
 * rowLo/rowHi from the endpoint rows, then for each row line the segment
 * crosses adds the crossing cell in BOTH that row and the row above it, plus
 * the neighbouring column when the crossing sits on a column boundary.
 *
 * GOTCHA: the endpoints are swapped so that y0 <= y1 for the scan, but the
 * two unconditional endpoint inserts and the column min/max happen BEFORE the
 * swap and use the caller's order. A zero dy returns after those. */
void BrSpanAddLine(BrSpanVolume *pVol, float x0, float y0, float x1, float y1);

/* 0x1003A950  point test in world units: BrSpanTest at (x/32, y/32). */
int BrSpanTestPoint(const BrSpanVolume *pVol, float x, float y);

/* 0x1003A990  clear the volume and rebuild it from six points.
 *
 * The twelve edges drawn are: 0-1, 0-2, 0-3, 0-4, 5-1, 5-2, 5-3, 5-4, 1-2,
 * 2-3, 3-4, 4-1 -- i.e. a quadrilateral ring (1,2,3,4) with apexes 0 and 5,
 * the silhouette of a view frustum. Only .x and .y of each point are read.
 *
 * The original takes the points from the six BrVec3s at 0x106C3310, stride
 * 0xC. */
void BrSpanBuildHull(BrSpanVolume *pVol, const BrVec3 aPt[6]);

/* ==========================================================================
 * 5. Particle pool
 * ========================================================================== */

/* One pool entry. Stride 0x20 is fixed by the free-list walk in 0x1003A4D0
 * (`add eax, 0x20`) and by the index scaling everywhere else (`shl x, 5`).
 * The pool base is 0x10A99BB8; index 0 is the null sentinel and is never
 * handed out. */
typedef struct BrPfxRec {
    BrVec3   pos;     /* +0x00 */
    BrVec3   vel;     /* +0x0C */
    float    age;     /* +0x18 */
    uint16_t iNext;   /* +0x1C  intrusive link, 0 terminates */
    uint8_t  f1E;     /* +0x1E  recomputed every tick from age */
    uint8_t  f1F;     /* +0x1F  set once at spawn                */
} BrPfxRec;

#define BR_PFX_RECS 256

typedef struct BrPfxPool {
    uint16_t iFree;              /* 0x10A99BA8 */
    uint16_t iListAC;            /* 0x10A99BAC */
    uint16_t iListB0;            /* 0x10A99BB0 */
    uint16_t iListB4;            /* 0x10A99BB4 */
    BrPfxRec aRec[BR_PFX_RECS];  /* 0x10A99BB8 */
} BrPfxPool;

/* Per-frame inputs the integrators read from fixed addresses. */
typedef struct BrPfxEnv {
    float  dt;      /* 0x106C2CFC */
    BrVec3 drift;   /* 0x104B2560, 0x104B2564, 0x104B2568 -- added to the
                     * position every tick, already scaled by dt elsewhere */
} BrPfxEnv;

/* 0x1003A4D0 (partial)  rebuild the free list 1,2,...,255,0 and empty the
 * three active lists.
 *
 * PARTIAL: the original also zeroes the first dword of each 0x2B68-byte
 * record in the table at 0x10ACEF04, [0x100B36FC] of them. That table is not
 * identified in this packet, so that half is deliberately absent. (Note that
 * 0x1003A4D0 does NOT touch the span grid at all, despite the note in
 * br_span.c -- the grid is cleared by 0x1003A990 / BrSpanBuildHull.) */
void BrPfxReset(BrPfxPool *pPool);

/* Snapshot layout written by 0x1003A610: four u16 heads in the order
 * free, B0, AC, B4 -- which is NOT the address order of the globals -- then
 * a verbatim 8192-byte copy of the record array. */
typedef struct BrPfxSnapshot {
    uint16_t iFree;              /* from 0x10A99BA8 */
    uint16_t iListB0;            /* from 0x10A99BB0 */
    uint16_t iListAC;            /* from 0x10A99BAC */
    uint16_t iListB4;            /* from 0x10A99BB4 */
    BrPfxRec aRec[BR_PFX_RECS];
} BrPfxSnapshot;

/* 0x1003A610  copy the four heads and the whole record array out. */
void BrPfxSaveState(const BrPfxPool *pPool, BrPfxSnapshot *pOut);

/* 0x1003A200  integrate list B0 (0x10A99BB0).
 *
 * age += dt * 0.3; scale = (f1E * f1F) / 65280; the position gains
 * vel * scale * dt + drift with a constant +0.8 added to vel.z first;
 * f1E is reloaded with (int)(5.7375 / age^2); an entry whose PREVIOUS scale
 * was below 0.03125 is unlinked and returned to the free list. */
void BrPfxUpdateB0(BrPfxPool *pPool, const BrPfxEnv *pEnv);

/* 0x1003A340  integrate lists B4 then AC, in that order.
 *
 * Same integrator as BrPfxUpdateB0 but age += dt * 0.7, vel.z also loses
 * dt * 19.62 each tick (gravity: 19.62 = 2g), f1E is reloaded with
 * (int)(102 / age), and an entry dies when scale < 0.03125 OR the NEW vel.z
 * has fallen below -30. */
void BrPfxUpdateB4AC(BrPfxPool *pPool, const BrPfxEnv *pEnv);

/* ==========================================================================
 * 6. Car-driven effects
 * ========================================================================== */

/* The car/entity record is stride 0x2B68 (contract fact). Only a handful of
 * its fields are touched here and the rest is unmapped, so it stays opaque and
 * is addressed by byte offset inside slice2_21.c. Pass a buffer of at least
 * 0x2B68 bytes, aligned for float. Field offsets, all established directly
 * from the disassembly:
 *
 *   +0x0000  BrVec3   subtracted from every spawned position and velocity
 *   +0x0010  BrVec3   axis A
 *   +0x0020  BrVec3   axis B
 *   +0x0070  BrVec3[4], stride 0x40   per-wheel point
 *   +0x0370 / +0x057C / +0x0788 / +0x0994   the four wheel records,
 *            stride 0x20C -- but indexed in the ORDER {0x994, 0x57C, 0x370,
 *            0x788}, which is NOT the address order. This ordering is used
 *            identically by 0x10039200 and 0x10039F20.
 *   wheel +0x01A0  signed char surface id, valid range 1..3
 *   wheel +0x01B4  int32, zero means "not in contact"
 *   +0x1024  BrVec3   axis C
 *   +0x1030  float    speed
 *   +0x106C  float[4] per-wheel spawn accumulator
 *   +0x107C  BrVec3[4] previous spawn position   (0x10039F20)
 *   +0x10AC  four parallel int/float arrays of 4, stride 4 within an array
 *            and 0x10 between arrays: +0x00 float, +0x10 int, +0x20 float,
 *            +0x30 int. (The cursor advances by 4 per wheel, not by the
 *            record size -- this is a struct-of-arrays.)
 *   +0x10EC  BrVec3[4] previous position         (0x10039200)
 *   +0x1140  float[3] per wheel, stride 0x480, mirrored to -0x20 bytes
 *   +0x2326  int16[..] per wheel, stride 0xD8, slots [-3..2]
 *   +0x2680  uint16    per wheel, stride 0x12
 *   +0x26C8  BrVec3   origin subtracted before the s16 encode
 *   +0x290C  uint16   index into the 84-byte record table
 *   +0x294C  int32    gate for that lookup
 *   +0x2994  float    half of this is added to the spawned position's z
 *   +0x36D   uint8    gate and integer multiplier for the spawn rate
 */
struct BrCar;   /* tag only: slice2_15.h already defines the full struct */

/* Fixed-address inputs for 0x10039200. */
typedef struct BrCarFxEnv {
    float                dt;          /* 0x106C2CFC */
    int32_t              mode6620;    /* 0x106C6620 */
    int32_t              sel0B380C;   /* 0x100B380C */
    int32_t              flag6624;    /* 0x106C6624 */
    const unsigned char *pRecs;       /* 0x106C7CA8; stride 84, bit 0x10 of
                                       * the byte at +0x4C is read. May be
                                       * NULL, in which case the lookup is
                                       * skipped. */
} BrCarFxEnv;

/* 0x10039200  per-wheel effect update (thiscall; ecx was the car).
 *
 * Runs four wheels in the {0x994, 0x57C, 0x370, 0x788} order, advancing a
 * sawtooth timer, deriving a direction vector from the car's three axes, and
 * writing it out twice: as s16 * 127 into the +0x2326 stream and as a
 * float-of-s16 into the +0x1140 stream.
 *
 * Returns early (doing nothing) when mode6620 is set and sel0B380C is neither
 * 2 nor 8. */
void BrCarWheelFx(struct BrCar *pCar, const BrCarFxEnv *pEnv,
                  uint32_t *pSeed);

/* 0x10039F20  spawn pool particles from the four wheels (thiscall).
 *
 * Does nothing unless the +0x36D gate is non-zero AND speed > 40. Each wheel
 * accumulates (0x36D * 0.03) * (dt*0.5 + speed*0.00066006603) and spawns when
 * that passes 0.75. Surface id 3 goes on list B4, 1 and 2 on list AC.
 *
 * pSeed is the generator state 0x1003BD50 advances (0x10A9BFD0). */
void BrCarPfxSpawn(struct BrCar *pCar, BrPfxPool *pPool, const BrPfxEnv *pEnv,
                   uint32_t *pSeed);

/* 0x10039020 -- thiscall, called on each car before BrCarWheelFx in one of
 * the three dispatch modes. Not in this packet. */
/* XSLICE 0x10039020 */
extern void BrCarSub9020(struct BrCar *pCar);

/* Fixed-address inputs for 0x1003A530. */
typedef struct BrPfxTickEnv {
    int32_t   mode6620;   /* 0x106C6620 */
    int32_t   mode661C;   /* 0x106C661C */
    int32_t   flag6624;   /* 0x106C6624 */
    int32_t   nCar;       /* 0x100B36F8 */
    struct BrCar **apCar; /* 0x10ACD4F8; the original walks a stride-0x80
                           * table and takes the pointer at offset 0 of each
                           * row. Flattened to an array of pointers here. */
    int32_t  *pbInit;     /* 0x10A9BBB8, the one-shot init flag */
} BrPfxTickEnv;

/* 0x1003A530  the per-frame driver. Three mutually exclusive modes:
 *
 *   mode6620 != 0                 -> BrPfxUpdateB0,  then per car
 *                                    BrCarSub9020 + BrCarWheelFx
 *   else mode661C == 0 && flag6624 == 0
 *                                 -> BrPfxUpdateB4AC, then per car
 *                                    BrCarPfxSpawn + BrCarWheelFx
 *   else                          -> per car BrCarWheelFx only
 *
 * Cars whose table slot holds a null pointer are skipped. On the first call
 * (*pbInit == 0) the pool is reset first and the flag set. */
void BrPfxTick(BrPfxPool *pPool, const BrPfxEnv *pEnv,
               const BrCarFxEnv *pFxEnv, const BrPfxTickEnv *pTick,
               uint32_t *pSeed);

#endif /* SLICE2_21_H */
