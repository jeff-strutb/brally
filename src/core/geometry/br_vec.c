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
#include "slice2_19.h"   /* the BrVec3Copy prototype, moved here from slice2_19.c */

#include <math.h>

/* 0x10035C70  DESTINATION FIRST -- see the header. */
/* WHAT IT DOES: copies a point or direction from one place to another. Note
 * the destination is the first argument, not the second. */
/* @implements 0x10035C70 d3d BrVec3Copy */
/* @n64 0x802245D4 exact */
void BrVec3Copy(BrVec3 *pDst, const BrVec3 *pSrc)
{
    pDst->x = pSrc->x;
    pDst->y = pSrc->y;
    pDst->z = pSrc->z;
}

/* WHAT IT DOES: the cross product -- the direction perpendicular to two
 * others, which is how surface normals and sideways axes are built. Uses
 * temporaries so the output may safely be one of the inputs. */
/* @implements 0x1003AC30 d3d BrVec3Cross */
/* @n64 0x8022439C located */
void BrVec3Cross(BrVec3 *pOut, const BrVec3 *pA, const BrVec3 *pB)
{
    float x = pA->y * pB->z - pA->z * pB->y;
    float y = pA->z * pB->x - pA->x * pB->z;
    float z = pA->x * pB->y - pA->y * pB->x;
    pOut->x = x; pOut->y = y; pOut->z = z;
}

/* WHAT IT DOES: the dot product -- how much two directions agree. Positive
 * means roughly the same way, zero means at right angles, negative means
 * opposing. The workhorse behind every angle and projection test in the
 * game. */
/* @implements 0x1003AC90 d3d BrVec3Dot */
/* @n64 0x80224404 located */
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

/* WHAT IT DOES: subtract one vector from another into a third. The usual way
 * to get the direction and distance FROM pB TO pA. */
/* @implements 0x1003AEE0 d3d BrVec3Sub */
/* @n64 0x80224808 exact */
void BrVec3Sub(BrVec3 *pOut, const BrVec3 *pA, const BrVec3 *pB)
{
    pOut->x = pA->x - pB->x;
    pOut->y = pA->y - pB->y;
    pOut->z = pA->z - pB->z;
}

/* WHAT IT DOES: add one vector into another IN PLACE -- pA becomes pA + pB.
 * The out-of-place twin is BrVec3Add. */
/* @implements 0x1003AF70 d3d BrVec3AddTo */
/* @n64 0x802248C8 exact */
/* SAME FP SCHEDULING WALL as BrVec3Scale below, 6 diffs: the original loads
 * pB first for ALL THREE components (`fld [ecx+n]; fadd [eax+n]`); we get
 * that for y and z but the x component comes out reversed, because the
 * parameter VC5 loads into a register first is the one it puts on the `fld`
 * side. Writing the first component (or all three) as `pA->x = pB->x + pA->x`
 * does not move it -- VC5 canonicalises the commutative FADD. */
/* N64 CONFIRMS THE WALL, AND PROMOTES IT FROM INFERENCE TO PROOF. The above
 * was reached by probing source forms and finding none that moved the bytes,
 * which cannot distinguish "our spelling is right" from "we have not found the
 * right spelling". The N64 twin settles it: IDO does NOT canonicalise
 * commutative operands (verified directly -- `a->x + b->x` loads a1 then a2,
 * `b->x + a->x` loads a2 then a1), so its bytes record the original's source
 * order. This function is byte-exact against the ROM at 0x802248C8 AS WRITTEN,
 * and the swapped spelling `pA->x = pB->x + pA->x` is NOT exact there.
 *
 * So the source below is the original's own spelling, and the PC residue is
 * purely VC5's choice of which operand to load into a register first. It is a
 * codegen wall, not a missing source form. DO NOT SPEND MORE PROBES ON
 * OPERAND ORDER HERE.
 *
 * One axis the oracle cannot settle: `pA->x += pB->x` and
 * `pA->x = pA->x + pB->x` are byte-identical under IDO too, so the compound
 * form is unproven either way -- it just does not matter to either target.
 * Method: n64/tools/n64match.py; see the commutative-addition entry in
 * docs/VC5-IDIOMS.md. */
void BrVec3AddTo(BrVec3 *pA, const BrVec3 *pB)
{
    pA->x += pB->x;
    pA->y += pB->y;
    pA->z += pB->z;
}

/* WHAT IT DOES: scale a vector by a number into a separate output, leaving
 * the input alone. */
/* @implements 0x10034360 glide BrVec3Scale */
/* @implements 0x1003ACE0 d3d BrVec3Scale */
/* @n64 0x802244FC exact */
/* FP SCHEDULING WALL: orig emits FLD [esp+0xC](s) / FMUL [eax](pV->x) for
 * the x component, but FLD component / FMUL s for y and z.  VC5 canonicalises
 * commutative FMUL regardless of source operand order, so no source form
 * changes the FLD operand for x.  5 diffs remain. */
/* SIX MORE DEAD PROBES, 2026-09-03, all identical at 37 B / 12 insns / 5 diffs:
 * the x product written scalar-first; all three written scalar-first; the x
 * product through a named temp with either operand order; a named temp for the
 * scalar itself; and a `const float *p = &pV->x` element pointer. These were
 * run because a NAMED TEMP had just broken the sum-of-products canonicaliser
 * on 0x10060C30 BrSndPan -- it does not carry over, and the reason is the
 * boundary now recorded in docs/VC5-IDIOMS.md: the temp lever needs a flat SUM
 * to lift a term OUT of. Each component here is a lone two-operand multiply,
 * and a lone commutative fmul is genuinely out of reach from source. */
/* N64 CANNOT SETTLE THIS ONE -- a negative result, recorded so it is not
 * mistaken for a gap. The twin at 0x802244FC is byte-exact, but BOTH
 * `pV->x * s` and `s * pV->x` produce those same bytes under IDO, because the
 * scalar arrives already in a register and only the vector component is
 * loaded. So unlike BrVec3AddTo and BrVec3MulAddTo below, the oracle is blind
 * to the operand order here and the original spelling stays unknown. That does
 * not weaken the note above: the VC5 residue is still a codegen wall, it is
 * just not independently confirmed for this function. */
void BrVec3Scale(BrVec3 *pOut, const BrVec3 *pV, float s)
{
    pOut->x = pV->x * s;
    pOut->y = pV->y * s;
    pOut->z = pV->z * s;
}

/* WHAT IT DOES: scale a vector by a number IN PLACE. The in-place twin of
 * BrVec3Scale. */
/* @implements 0x1003AD10 d3d BrVec3ScaleBy */
/* @n64 0x80224528 located */
void BrVec3ScaleBy(BrVec3 *pV, float s)
{
    pV->x *= s;
    pV->y *= s;
    pV->z *= s;
}

/* WHAT IT DOES: scale one vector and add it to another, into a separate
 * output: pOut = pA + pB*s. One multiply and one add per component, the
 * everyday 'move this far in that direction' step. */
/* @implements 0x1003AFE0 d3d BrVec3MulAdd */
/* @n64 0x8022494C located */
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

/* WHAT IT DOES: the in-place form of BrVec3MulAdd -- pA += pB*s. This is
 * what integrates a velocity into a position each frame. */
/* @implements 0x1003B020 d3d BrVec3MulAddTo */
/* @n64 0x80224990 exact */
/* pA += pB*s, in place.  The x87 forms s*pB.x then adds pA.x; float add
 * commutes, so pA.x + pB.x*s is bit-identical -- one multiply, one add. */
/* N64 CONFIRMS THE MULTIPLY'S OPERAND ORDER. The twin at 0x80224990 is
 * byte-exact as written, and `s * pB->x` is NOT exact there -- IDO does not
 * canonicalise commutative operands, so its bytes record the original's own
 * spelling. `pB->x * s` is therefore the original, not merely a form that
 * happens to work. Contrast BrVec3Scale above, where both spellings give the
 * same MIPS and the oracle proves nothing. Method: n64/tools/n64match.py. */
void BrVec3MulAddTo(BrVec3 *pA, const BrVec3 *pB, float s)
{
    pA->x += pB->x * s;
    pA->y += pB->y * s;
    pA->z += pB->z * s;
}

/* WHAT IT DOES: blend between two points. t=0 gives pB, t=1 gives pA -- note
 * that order, it is the reverse of the usual lerp convention and follows from
 * the original's `(a-b)*t + b` shape. Used for smoothing a value towards a
 * target over several frames rather than snapping to it. */
/* @implements 0x1003AFA0 d3d BrVec3Lerp */
/* @n64 0x802248FC located */
void BrVec3Lerp(BrVec3 *pOut, const BrVec3 *pA, const BrVec3 *pB, float t)
{
    pOut->x = (pA->x - pB->x) * t + pB->x;
    pOut->y = (pA->y - pB->y) * t + pB->y;
    pOut->z = (pA->z - pB->z) * t + pB->z;
}

/* WHAT IT DOES: flip a direction to point the opposite way, writing the
 * result somewhere else and leaving the input alone. */
/* @implements 0x1003ACC0 d3d BrVec3Negate */
/* @n64 0x80224434 exact */
void BrVec3Negate(BrVec3 *pOut, const BrVec3 *pV)
{
    pOut->x = -pV->x;
    pOut->y = -pV->y;
    pOut->z = -pV->z;
}

/* WHAT IT DOES: add two vectors into a third. Used both for combining
 * directions and for offsetting a position by a displacement. */
/* @implements 0x1003AF40 d3d BrVec3Add */
/* @n64 0x80224894 exact */
void BrVec3Add(BrVec3 *pOut, const BrVec3 *pA, const BrVec3 *pB)
{
    pOut->x = pB->x + pA->x;
    pOut->y = pB->y + pA->y;
    pOut->z = pB->z + pA->z;
}

/* WHAT IT DOES: subtract one vector from another IN PLACE -- pA becomes
 * pA - pB. The out-of-place twin is BrVec3Sub; the pair exists because the
 * caller usually has no use for the old value. */
/* @implements 0x1003AF10 d3d BrVec3SubFrom */
/* @n64 0x8022483C exact */
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
/* WHAT IT DOES: scale a vector down by a scalar into a separate output.
 * Dividing once and multiplying three times is the ORIGINAL's arithmetic,
 * not an optimisation added here -- see the note above on why that matters. */
/* @implements 0x1003AD40 d3d BrVec3Div */
/* @n64 0x8022455C located */
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

/* WHAT IT DOES: scale a vector down by a scalar IN PLACE. The in-place twin
 * of BrVec3Div. */
/* @implements 0x100343F0 glide BrVec3DivBy */
/* @implements 0x1003AD70 d3d BrVec3DivBy */
/* @n64 0x80224594 located */
void BrVec3DivBy(BrVec3 *pV, float s)
{
    /* Each product copies the live reciprocal into a FRESH named temp:
     * `(t = r) * mem` emits `fld st; fmul [mem]`.  Without the copies the
     * components take the fld side and leftover r is fstp-discarded. */
    float r, r2, r3;
    pV->x = (r = 1.0f / s) * pV->x;
    pV->y = (r2 = r) * pV->y;
    pV->z = (r3 = r) * pV->z;
}

/* 0.5f constant lives at 0x1008F638. */
/* WHAT IT DOES: the point exactly halfway between two points. */
/* @implements 0x1003B050 d3d BrVec3Midpoint */
/* @n64 0x802249D4 located */
void BrVec3Midpoint(BrVec3 *pOut, const BrVec3 *pA, const BrVec3 *pB)
{
    /* The GLIDE original fetches pB first for x but pA first for y and z.
     * The compiler alternates x by itself; y and z name their pA component
     * to force it to be the fetched operand -- the same lever that matches
     * BrVec3Div and BrMat4MulVec3Transposed. */
    pOut->x = (pA->x + pB->x) * 0.5f;
    {
        float ay = pA->y;
        pOut->y = (ay + pB->y) * 0.5f;
    }
    {
        float az = pA->z;
        pOut->z = (az + pB->z) * 0.5f;
    }
}

/* WHAT IT DOES: reset a vector to the origin (0,0,0). */
/* @implements 0x10034710 glide BrVec3Zero */
/* @n64 0x80224A1C exact */
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
/* WHAT IT DOES: the SQUARED straight-line distance between two points.
 * Cheaper than BrVec3Dist because it skips the square root, and that is
 * enough whenever the caller only compares distances or tests a radius --
 * which is most of the collision and proximity code. */
/* @implements 0x1003B130 d3d BrVec3DistSq */
/* @n64 0x80224ACC located */
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
/* @n64 0x80224A78 located */
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
/* @n64 0x80224B08 located */
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
/* @n64 0x80223850 located */
int BrSub10071B60(const BrVec3 *pA, const BrVec3 *pB)
{
    if (!(pA->y >= pB->y))
        return 1;
    return 0;
}
