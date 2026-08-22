/* br_vec.h -- 3D vector math, decompiled from BRD3D.dll.
 *
 * The engine's vector library: a tight cluster of leaf functions at
 * 0x1003AC30-0x1003B060 with no callees of their own, called from 76 sites.
 * Semantics were recovered by tracing the x87 stack instruction by
 * instruction (these are FPU-heavy and full of fxch, so operand order is not
 * apparent without doing that).
 *
 * Argument order follows the original: destination first.
 */
#ifndef BR_VEC_H
#define BR_VEC_H

typedef struct BrVec3 { float x, y, z; } BrVec3;

/* 0x1003AC30  out = a x b   (right-handed cross product) */
void  BrVec3Cross(BrVec3 *pOut, const BrVec3 *pA, const BrVec3 *pB);
/* 0x1003AC90  returns a . b */
float BrVec3Dot(const BrVec3 *pA, const BrVec3 *pB);
/* 0x1003AEE0  out = a - b */
void  BrVec3Sub(BrVec3 *pOut, const BrVec3 *pA, const BrVec3 *pB);
/* 0x1003AF70  a += b */
void  BrVec3AddTo(BrVec3 *pA, const BrVec3 *pB);
/* 0x1003ACE0  out = v * s */
void  BrVec3Scale(BrVec3 *pOut, const BrVec3 *pV, float s);
/* 0x1003AD10  v *= s */
void  BrVec3ScaleBy(BrVec3 *pV, float s);
/* 0x1003AFE0  out = a + b * s */
void  BrVec3MulAdd(BrVec3 *pOut, const BrVec3 *pA, const BrVec3 *pB, float s);
/* 0x1003B020  a += b * s */
void  BrVec3MulAddTo(BrVec3 *pA, const BrVec3 *pB, float s);
/* 0x1003AFA0  out = (a - b) * t + b   -- note t=0 yields b, t=1 yields a */
void  BrVec3Lerp(BrVec3 *pOut, const BrVec3 *pA, const BrVec3 *pB, float t);

/* 0x1003ACC0  out = -v */
void  BrVec3Negate(BrVec3 *pOut, const BrVec3 *pV);
/* 0x1003AF40  out = a + b */
void  BrVec3Add(BrVec3 *pOut, const BrVec3 *pA, const BrVec3 *pB);
/* 0x1003AF10  a -= b */
void  BrVec3SubFrom(BrVec3 *pA, const BrVec3 *pB);
/* 0x1003AD40  out = v / s   (original computes 1.0f/s once, then multiplies) */
void  BrVec3Div(BrVec3 *pOut, const BrVec3 *pV, float s);
/* 0x1003AD70  v /= s */
void  BrVec3DivBy(BrVec3 *pV, float s);
/* 0x1003B050  out = (a + b) * 0.5f */
void  BrVec3Midpoint(BrVec3 *pOut, const BrVec3 *pA, const BrVec3 *pB);
/* 0x1003B090  v = 0 */
void  BrVec3Zero(BrVec3 *pV);

/* 0x1003B130  returns |a - b|^2  (squared, no sqrt) */
float BrVec3DistSq(const BrVec3 *pA, const BrVec3 *pB);

/* 0x1003B0E0  returns |a - b|  (true distance; calls the fsqrt wrapper) */
float BrVec3Dist(const BrVec3 *pA, const BrVec3 *pB);

/* 0x1003B170  returns |v|.
 *
 * Was MISSING from this header until a cross-check found it -- it sits inside the
 * 0x1003AC30-0x1003B130 cluster this file otherwise covers, so "the vector
 * library is complete" was wrong.
 *
 * Note the sum of squares is ROUNDED TO float32 before the sqrt (the original
 * spills it through a 4-byte stack slot), unlike BrVec3dLen which keeps double
 * precision throughout. Do not implement this as sqrtf of a double sum. */
float BrVec3Length(const BrVec3 *pV);

/* 0x10071B60  1 if pA->y is strictly below pB->y, or either is NaN; else 0. */
int BrSub10071B60(const BrVec3 *pA, const BrVec3 *pB);

#endif /* BR_VEC_H */
