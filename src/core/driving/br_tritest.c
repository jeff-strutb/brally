/* br_tritest.c -- driving: is a point inside a collision triangle?
 *
 * 0x100656F0, the 2-D barycentric containment test the collision response
 * leans on. The triangle record carries its plane normal and three vertex
 * POINTERS; the test projects everything onto the two axes the normal is
 * least aligned with and solves the little 2x2 system.
 */

#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif

#include <stdint.h>

#ifdef BR_MATCHING_BUILD

extern float _DAT_10077a78;      /* 0.0f */
extern float _DAT_10077a7c;      /* 1.0f */

typedef struct BrTriRec {
    float  n[3];                 /* +0x00  the plane normal      */
    int    f0C;                  /* +0x0C  not read here         */
    float *pA;                   /* +0x10  vertex A              */
    float *pB;                   /* +0x14  vertex B              */
    float *pC;                   /* +0x18  vertex C              */
} BrTriRec;

/* WHAT IT DOES: says whether a point lies inside a collision triangle. The
 * triangle is flattened onto the two coordinate axes its normal points along
 * least (so the projection is as fat as possible), the point is expressed in
 * edge coordinates u along one edge and v along the other, and it is inside
 * exactly when u and v are non-negative and u, v and u+v all stay below one.
 * The degenerate arm keeps the divide safe when the first edge is flat on
 * the chosen axis. */
/* PARKED at 3 regions / 8 msetdiff rows, 591/599 B, 196/200 insns.  What
 * landed: goto-funnel exits (shared zero_out for the v/u+v fails, plain
 * out for the u fails) give the ebx result web and the fstp-on-edge; the
 * ||-chain per arm gives the shared tail; the barycentric v is a CSE of
 * the written-out division, never a named local (naming it homes a slot
 * the original lacks); u > 1.0f (not !(u < 1.0f)) gives test 0x41; the
 * d2/c2 slot pair is a declaration-order tie-break.
 * Residue, ONE construct at two sites: the original's u+v is
 * `fld [u]; fadd st(1); ...; fstp st(0)` (u pushed, CSE-v as the register
 * operand, v dropped after) where ours folds `fadd [u]` into v.  Corpus
 * MISS on the 6-insn window at +0x18c.  Dead: sum operand order
 * (canonicalised), leading paren on u, constant-first ceiling, separate
 * if statements, per-site r=0 (duplicates the tail), inline returns
 * (constant-folds the web), if/else arms with fall-through return (same).
 * Plus the P-load SIB pair `[eax+edi]` vs `[edi+eax]` -- the 0x100540D0
 * emitter-byte class.  A1 4 / A2 8 / A3 8; everything else PASSES. */
/* @implements 0x100656F0 glide BrTriContainsPoint */
int16_t BrTriContainsPoint(BrTriRec *pT, float *pP)
{
    float a0, a1;
    int   c, i1, i2;
    float d1, c2, b1, b2, c1, d2;
    float u;
    int r;

    a0 = pT->n[0];
    if (pT->n[0] < _DAT_10077a78) {
        a0 = -a0;
    }
    a1 = pT->n[1];
    if (pT->n[1] < _DAT_10077a78) {
        a1 = -a1;
    }
    if (a0 > a1) {
        a0 = pT->n[0];
        if (pT->n[0] < _DAT_10077a78) {
            a0 = -a0;
        }
        a1 = pT->n[2];
        if (pT->n[2] < _DAT_10077a78) {
            a1 = -a1;
        }
        if (a0 > a1) {
            c = 0;
        } else {
            c = 2;
        }
    } else {
        a0 = pT->n[1];
        if (pT->n[1] < _DAT_10077a78) {
            a0 = -a0;
        }
        a1 = pT->n[2];
        if (pT->n[2] < _DAT_10077a78) {
            a1 = -a1;
        }
        if (a0 > a1) {
            c = 1;
        } else {
            c = 2;
        }
    }

    r  = 0;
    i1 = (c + 1) % 3;
    i2 = (c + 2) % 3;

    d1 = pP[i1] - pT->pA[i1];
    d2 = pP[i2] - pT->pA[i2];
    b1 = pT->pB[i1] - pT->pA[i1];
    b2 = pT->pB[i2] - pT->pA[i2];
    c1 = pT->pC[i1] - pT->pA[i1];
    c2 = pT->pC[i2] - pT->pA[i2];

    if (b1 == _DAT_10077a78) {
        u = d1 / c1;
        if (u < _DAT_10077a78) {
            goto out;
        }
        if (u > _DAT_10077a7c) {
            goto out;
        }
        /* The second coordinate is written out twice and CSEd -- a named
         * local gets homed to a slot the original does not have. */
        if ((d2 - u * c2) / b2 < _DAT_10077a78
            || u + (d2 - u * c2) / b2 > _DAT_10077a7c) {
            goto zero_out;
        }
        r = 1;
        goto out;
    }
    u = (d2 * b1 - d1 * b2) / (c2 * b1 - c1 * b2);
    if (u < _DAT_10077a78) {
        goto out;
    }
    if (u > _DAT_10077a7c) {
        goto out;
    }
    if ((d1 - u * c1) / b1 < _DAT_10077a78
        || u + (d1 - u * c1) / b1 > _DAT_10077a7c) {
        goto zero_out;
    }
    r = 1;
    goto out;
zero_out:
    r = 0;
out:
    return (int16_t)r;
}

#endif /* BR_MATCHING_BUILD */
