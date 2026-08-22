/* br_vec.c -- vector math decompiled from BRD3D.dll. See br_vec.h.
 *
 * Each function below is a direct transcription of the x87 sequence at the
 * noted address. The originals write results in an order dictated by the FPU
 * stack (e.g. cross stores y, z, then x); that ordering is irrelevant to
 * callers, so the C is written in the natural order instead. Cross uses a
 * temporary because the original also buffers a component -- aliasing out
 * with a or b is therefore safe, exactly as in the original.
 */
#include "br_vec.h"

#include <math.h>

/* @implements 0x1003AC30 d3d BrVec3Cross */
void BrVec3Cross(BrVec3 *pOut, const BrVec3 *pA, const BrVec3 *pB)
{
    float x = pA->y * pB->z - pA->z * pB->y;
    float y = pA->z * pB->x - pA->x * pB->z;
    float z = pA->x * pB->y - pA->y * pB->x;
    pOut->x = x; pOut->y = y; pOut->z = z;
}

/* @implements 0x1003AC90 d3d BrVec3Dot */
/* PRECISION CAVEAT (Dot only; the four componentwise helpers below are exact --
 * one operation per store, one rounding, same as the original). Like
 * BrVec3Length above, the original forms this sum on the x87 stack (53-bit,
 * PC=2) and rounds once, summing y*y+z*z BEFORE x*x, whereas this body
 * evaluates left-to-right in float and rounds at every step. A last-bit
 * difference; recorded here rather than asserted equivalent, and left for an
 * audited precision pass to settle alongside BrVec3DistSq / BrVec3Dist. */
float BrVec3Dot(const BrVec3 *pA, const BrVec3 *pB)
{
    /* Term order is y, z, x deliberately: the original accumulates
     * (y + z) + x, and writing it that way reproduces the whole x87 body
     * instruction for instruction and brings the compiled size to the
     * original's 37 bytes exactly.
     *
     * NOT YET MATCHING -- two bytes remain, and they are the parameter
     * loads, not the arithmetic:
     *     original   mov eax,[esp+4]   mov ecx,[esp+8]
     *     ours       mov eax,[esp+8]   mov ecx,[esp+4]
     * i.e. the compiler assigns the two pointers to the opposite registers.
     * Since a dot product is symmetric this is pure register allocation, not
     * a maths difference. Operand order within the products does NOT move it
     * -- VC5 canonicalises commutative fmul operands, and all-pA-first,
     * all-pB-first and mixed were each tried. This is the known "register
     * load order" class, not something source order controls.
     *
     * DEVIATION (port target): float addition is not associative, so the
     * y,z,x order can differ from x,y,z in the last bit. That sits with the
     * audited precision pass still owed alongside BrVec3DistSq / BrVec3Dist. */
    return pA->y * pB->y + pA->z * pB->z + pB->x * pA->x;
}

/* @implements 0x1003AEE0 d3d BrVec3Sub */
void BrVec3Sub(BrVec3 *pOut, const BrVec3 *pA, const BrVec3 *pB)
{
    pOut->x = pA->x - pB->x;
    pOut->y = pA->y - pB->y;
    pOut->z = pA->z - pB->z;
}

/* @implements 0x1003AF70 d3d BrVec3AddTo */
void BrVec3AddTo(BrVec3 *pA, const BrVec3 *pB)
{
    pA->x += pB->x;
    pA->y += pB->y;
    pA->z += pB->z;
}

/* @implements 0x1003ACE0 d3d BrVec3Scale */
void BrVec3Scale(BrVec3 *pOut, const BrVec3 *pV, float s)
{
    /* NOT MATCHING: the original fetches the SCALAR for the x term
     * (`fld [esp+0xc]; fmul [eax]`) and the component for y and z, which ours
     * has the other way round on x only.  Swapping the operands does nothing
     * (VC5 canonicalises commutative fmul), and the naming lever that fixes
     * BrVec3Midpoint does not apply: it works by naming a dereference, and
     * `s` is already a plain memory operand with nothing to name. */
    pOut->x = pV->x * s;
    pOut->y = pV->y * s;
    pOut->z = pV->z * s;
}

/* @implements 0x1003AD10 d3d BrVec3ScaleBy */
void BrVec3ScaleBy(BrVec3 *pV, float s)
{
    pV->x *= s;
    pV->y *= s;
    pV->z *= s;
}

/* @implements 0x1003AFE0 d3d BrVec3MulAdd */
/* pOut = pA + pB*s.  The original scales the SECOND vector arg ([esp+0xc]) and
 * adds the first ([esp+8]); under cdecl that is pB and pA respectively, so the
 * argument order here matches.  One multiply then one add per component -- same
 * two roundings as the x87, so this is exact, not just close. */
void BrVec3MulAdd(BrVec3 *pOut, const BrVec3 *pA, const BrVec3 *pB, float s)
{
    pOut->x = pA->x + pB->x * s;
    pOut->y = pA->y + pB->y * s;
    pOut->z = pA->z + pB->z * s;
}

/* @implements 0x1003B020 d3d BrVec3MulAddTo */
/* pA += pB*s, in place.  The x87 forms s*pB.x then adds pA.x; float add
 * commutes, so pA.x + pB.x*s is bit-identical -- one multiply, one add. */
void BrVec3MulAddTo(BrVec3 *pA, const BrVec3 *pB, float s)
{
    pA->x += pB->x * s;
    pA->y += pB->y * s;
    pA->z += pB->z * s;
}

/* @implements 0x1003AFA0 d3d BrVec3Lerp */
void BrVec3Lerp(BrVec3 *pOut, const BrVec3 *pA, const BrVec3 *pB, float t)
{
    pOut->x = (pA->x - pB->x) * t + pB->x;
    pOut->y = (pA->y - pB->y) * t + pB->y;
    pOut->z = (pA->z - pB->z) * t + pB->z;
}

/* @implements 0x1003ACC0 d3d BrVec3Negate */
void BrVec3Negate(BrVec3 *pOut, const BrVec3 *pV)
{
    pOut->x = -pV->x;
    pOut->y = -pV->y;
    pOut->z = -pV->z;
}

/* @implements 0x1003AF40 d3d BrVec3Add */
void BrVec3Add(BrVec3 *pOut, const BrVec3 *pA, const BrVec3 *pB)
{
    pOut->x = pA->x + pB->x;
    pOut->y = pA->y + pB->y;
    pOut->z = pA->z + pB->z;
}

/* @implements 0x1003AF10 d3d BrVec3SubFrom */
void BrVec3SubFrom(BrVec3 *pA, const BrVec3 *pB)
{
    pA->x -= pB->x;
    pA->y -= pB->y;
    pA->z -= pB->z;
}

/* The original loads the constant 1.0f (at 0x1008F628), divides once, then
 * multiplies each component -- one divide instead of three. Reproduced
 * faithfully because it is also observably different from three divides in
 * the low bits, and physics code may depend on that. */
/* @implements 0x1003AD40 d3d BrVec3Div */
void BrVec3Div(BrVec3 *pOut, const BrVec3 *pV, float s)
{
    float r;
    /* The assignment to r is INSIDE the first product on purpose, and that is
     * the whole reason this matches.  The original's first term is
     * `fld st(0); fmul dword ptr [eax]` -- it duplicates the reciprocal while
     * it is still fresh on the x87 stack from the fdiv -- while the y and z
     * terms are the settled `fld mem; fmul st(1)` form.  Writing
     * `float r = 1.0f/s;` as a separate statement makes all three terms take
     * the settled form and loses the first one.
     *
     * Note what does NOT work, so nobody re-derives it: swapping the operands
     * (r * pV->x) changes nothing, because VC5 canonicalises commutative fmul
     * operands; neither does /Op, /Oa, /Ow, /O1 or /Ox.  It is expression
     * SHAPE that decides, not operand order and not compiler flags. */
    pOut->x = (r = 1.0f / s) * pV->x;
    pOut->y = pV->y * r;
    pOut->z = pV->z * r;
}

/* @implements 0x1003AD70 d3d BrVec3DivBy */
void BrVec3DivBy(BrVec3 *pV, float s)
{
    float r = 1.0f / s;
    /* NOT MATCHING.  The original duplicates the reciprocal on the x87 stack
     * (`fld st(0); fld st(1)`) and multiplies each copy by a component held as
     * a memory operand; ours loads all three components first.  Tried and
     * rejected: consuming the division inline as BrVec3Div does, and explicit
     * `pV->x = r * pV->x` products instead of `*=`.  Neither moves it, so the
     * in-place form is doing something the out-of-place BrVec3Div does not. */
    pV->x *= r;
    pV->y *= r;
    pV->z *= r;
}

/* 0.5f constant lives at 0x1008F638. */
/* @implements 0x1003B050 d3d BrVec3Midpoint */
void BrVec3Midpoint(BrVec3 *pOut, const BrVec3 *pA, const BrVec3 *pB)
{
    /* The original does not fetch the two operands in a consistent order:
     * x and z load from pB first, y loads from pA first.  The compiler picks
     * that alternation by itself for x and y, but not for z, so z names its
     * pB component to force it to be the fetched operand -- the same lever
     * that matches BrVec3Div and BrMat4MulVec3Transposed. */
    pOut->x = (pA->x + pB->x) * 0.5f;
    pOut->y = (pA->y + pB->y) * 0.5f;
    {
        float bz = pB->z;
        pOut->z = (bz + pA->z) * 0.5f;
    }
}

/* @implements 0x1003B090 d3d BrVec3Zero */
void BrVec3Zero(BrVec3 *pV)
{
    /* Written back to front: the original stores z, then y, then x, and MSVC
     * emits plain stores to distinct addresses in source order.  Unlike the
     * register assignment that blocks BrVec3Dot, store order IS controllable
     * from the source, so this is a real fix rather than a coincidence. */
    pV->z = 0.0f;
    pV->y = 0.0f;
    pV->x = 0.0f;
}

/* The original spills dx and dy to stack scratch and multiplies them back in,
 * which is just how MSVC scheduled the x87 stack; the arithmetic is a plain
 * squared distance. */
/* @implements 0x1003B130 d3d BrVec3DistSq */
float BrVec3DistSq(const BrVec3 *pA, const BrVec3 *pB)
{
    float dx = pA->x - pB->x;
    float dy = pA->y - pB->y;
    float dz = pA->z - pB->z;
    return dx * dx + dy * dy + dz * dz;
}

/* 0x1003B0E0 -- identical body to BrVec3DistSq, then tail-calls the fsqrt
 * wrapper at 0x10002250 (`fld [esp+4]; fsqrt; ret`). Summation order is
 * (dx*dx + dy*dy) + dz*dz, matching DistSq: at 0x1003B119 the stack is
 * st0=dy^2 st1=dz^2 st2=dx^2, so `faddp st(2)` makes dx^2+dy^2 and the
 * `faddp st(1)` at 0x1003B11B adds dz^2 to it. */
/* WHAT IT DOES: the straight-line distance between two points in 3D. One of
 * the most-used routines in the game -- how far a car is from anything. */
/* @implements 0x1003B0E0 d3d BrVec3Dist */
/* The original does NOT call BrVec3DistSq and does NOT use an inline fsqrt: it
 * spells the three deltas out in its own body -- the same spill-and-multiply-
 * back sequence BrVec3DistSq compiles to -- and then tail-calls the fsqrt
 * wrapper at 0x10002250.  Calling the neighbour and taking the `sqrt`
 * intrinsic gave a 32-byte body against the original's 75.  slice2_21.c
 * already writes its distances this way.
 *
 * The two builds differ ONLY in which square root is named, and the arithmetic
 * above it is identical in both.  The split exists because 0x10002250's single
 * definition lives in slice4_53.c, a large module with a long include chain
 * that nothing in the port links today -- naming it unconditionally leaves
 * test_vec with an undefined symbol, and dragging slice4_53 into a vector
 * test's deps to reach a seven-byte leaf is the wrong trade. */
#ifdef BR_MATCHING_BUILD
extern float BrSqrtF(float x);   /* 0x10002250 -- fld [esp+4]; fsqrt; ret */
float BrVec3Dist(const BrVec3 *pA, const BrVec3 *pB)
{
    float dx = pA->x - pB->x;
    float dy = pA->y - pB->y;
    float dz = pA->z - pB->z;
    return BrSqrtF(dx * dx + dy * dy + dz * dz);
}
#else
float BrVec3Dist(const BrVec3 *pA, const BrVec3 *pB)
{
    float dx = pA->x - pB->x;
    float dy = pA->y - pB->y;
    float dz = pA->z - pB->z;
    return sqrtf(dx * dx + dy * dy + dz * dz);
}
#endif

/* 0x1003B170 (Glide 0x100347F0, `shared`, matched by body) -- 65 bytes, traced
 * through every fxch:
 *
 *   1003B17B  fld [eax]          st0=x
 *   1003B185  fld [esp]          st0=z            st1=x
 *   1003B189  fld [esp+8]        st0=y            st1=z     st2=x
 *   1003B18D  fmul [esp+8]       st0=y*y
 *   1003B191  fxch st(1)         st0=z            st1=y*y   st2=x
 *   1003B193  fmul [esp]         st0=z*z
 *   1003B197  fld st(2)          st0=x            st1=z*z   st2=y*y  st3=x
 *   1003B199  fmul st(3)         st0=x*x
 *   1003B19B  fxch st(1)         st0=z*z          st1=x*x   st2=y*y
 *   1003B19D  faddp st(2)        st2 = y*y + z*z, pop
 *   1003B1A0  faddp st(1)        st1 = (y*y + z*z) + x*x, pop
 *   1003B1A2  fstp dword [esp]   ONE round, to float32
 *   1003B1A7  call 0x10002250    `fld [esp+4]; fsqrt; ret`
 *
 * Two things the previous transcription had wrong, and they are separate:
 *
 * ASSOCIATION.  Y and Z are summed FIRST; X joins last.  The old body was
 * `x*x + y*y + z*z`, i.e. (x^2+y^2)+z^2, which is a different expression.
 * Fixed here.  Measured, it is not observable through this function's float32
 * return -- the two groupings of three NON-NEGATIVE addends differ by at most
 * ~2^-51 relative, which is 2^27 times finer than float32 can record, and a
 * search of 20M uniform plus 400M targeted triples found no float32
 * disagreement.  It is corrected because it is what the instructions say, not
 * because a test can see it.
 *
 * PRECISION.  The three products and both partial sums stay in x87 registers
 * and are rounded exactly ONCE, at the `fstp dword`.  The old body rounded to
 * float32 after every operation, so each square could underflow to zero (or to
 * the wrong denormal) before the sum was formed.  That IS observable:
 * (2e-23, 2e-23, 2e-23) gives 3.74339207e-23 here and gave 0 before, and
 * (3e-23, 3e-23, 0) gives 3.74339207e-23 here and gave 5.29395592e-23 before.
 *
 * DEVIATION, stated: the intermediates are modelled with `double`, not `long
 * double`.  MSVC's CRT initialises the x87 control word to 0x027F, i.e. 53-bit
 * precision, so `double` is an EXACT model of these registers rather than an
 * approximation -- `long double` would model a 64-bit-mantissa control word
 * this process never runs in.  Each product is exact either way (a float32
 * square needs at most 48 mantissa bits).  The final `sqrtf` of a float32 is
 * likewise exact: a correctly-rounded wider sqrt re-rounded to float32 equals
 * the correctly-rounded float32 sqrt, so the wrapper's fsqrt needs no model.
 *
 * NOT fixed, and deliberately: BrVec3DistSq / BrVec3Dist above have the same
 * single-rounding shape (plus a `fst`/`fmul` pair that squares a 53-bit
 * register difference against its own float32 copy) and are still written in
 * float32 throughout.  That is a separate transcription, out of this change's scope,
 * and is recorded here rather than left silent. */
/* WHAT IT DOES: how long a vector is -- the distance from the origin to the
 * point it names. Used everywhere a speed or a reach has to come out of an
 * (x, y, z) triple. */
/* @implements 0x1003B170 d3d BrVec3Length */
#ifdef BR_MATCHING_BUILD
float BrVec3Length(const BrVec3 *pV)
{
    float x = pV->x;
    float y = pV->y;
    float z = pV->z;

    return BrSqrtF(y * y + z * z + x * x);
}
#else
float BrVec3Length(const BrVec3 *pV)
{
    double xx = (double)pV->x * (double)pV->x;
    double yy = (double)pV->y * (double)pV->y;
    double zz = (double)pV->z * (double)pV->z;

    /* `fstp dword [esp]` -- the sum is rounded to float32 BEFORE the sqrt. */
    float sum = (float)((yy + zz) + xx);

    return sqrtf(sum);
}
#endif

/* WHAT IT DOES: returns 1 if the first vector's y is below the second's y
 * (or either is a NaN), else 0. A two-way "is lower" test, not a three-way
 * qsort comparator. The original compares the float at +4 of each pointer;
 * that offset is BrVec3.y.  `test ah,1` is C0, so unordered takes the true
 * side -- written `!(a >= b)`, never `a < b`. */
/* @implements 0x10071B60 d3d BrSub10071B60 */
int BrSub10071B60(const BrVec3 *pA, const BrVec3 *pB)
{
    if (!(pA->y >= pB->y))
        return 1;
    return 0;
}
