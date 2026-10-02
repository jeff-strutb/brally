/* br_vec_cxx.h -- C++ overloads of the vector helpers that take float arrays.
 * C converts float[3] to a struct BrVec3 * silently; C++ does not, and the
 * C++ lane calls these helpers with arrays.  Generated from br_funcs.h. */
#ifndef BR_VEC_CXX_H
#define BR_VEC_CXX_H
#ifdef __cplusplus
static inline void BrMat4BuildScaledTransposed(const struct BrMat4 * a0, struct BrMat4 * a1, const float * a2) { BrMat4BuildScaledTransposed(a0, a1, (const struct BrVec3 *)a2); }
static inline void BrMat4MulVec3(float * a0, const struct BrMat4 * a1, const struct BrVec3 * a2) { BrMat4MulVec3((struct BrVec3 *)a0, a1, a2); }
static inline void BrMat4MulVec3(struct BrVec3 * a0, const struct BrMat4 * a1, const float * a2) { BrMat4MulVec3(a0, a1, (const struct BrVec3 *)a2); }
static inline void BrMat4MulVec3(float * a0, const struct BrMat4 * a1, const float * a2) { BrMat4MulVec3((struct BrVec3 *)a0, a1, (const struct BrVec3 *)a2); }
static inline void BrMat4MulVec3Transposed(float * a0, const struct BrMat4 * a1, const struct BrVec3 * a2) { BrMat4MulVec3Transposed((struct BrVec3 *)a0, a1, a2); }
static inline void BrMat4MulVec3Transposed(struct BrVec3 * a0, const struct BrMat4 * a1, const float * a2) { BrMat4MulVec3Transposed(a0, a1, (const struct BrVec3 *)a2); }
static inline void BrMat4MulVec3Transposed(float * a0, const struct BrMat4 * a1, const float * a2) { BrMat4MulVec3Transposed((struct BrVec3 *)a0, a1, (const struct BrVec3 *)a2); }
static inline void BrMat4TransformPoint(float * a0, const struct BrMat4 * a1, const struct BrVec3 * a2) { BrMat4TransformPoint((struct BrVec3 *)a0, a1, a2); }
static inline void BrMat4TransformPoint(struct BrVec3 * a0, const struct BrMat4 * a1, const float * a2) { BrMat4TransformPoint(a0, a1, (const struct BrVec3 *)a2); }
static inline void BrMat4TransformPoint(float * a0, const struct BrMat4 * a1, const float * a2) { BrMat4TransformPoint((struct BrVec3 *)a0, a1, (const struct BrVec3 *)a2); }
static inline void BrMat4TransformPoint4(float * a0, const float * a1, const float * a2) { BrMat4TransformPoint4(a0, (const struct BrVec3 *)a1, a2); }
static inline void BrVec3Add(float * a0, const struct BrVec3 * a1, const struct BrVec3 * a2) { BrVec3Add((struct BrVec3 *)a0, a1, a2); }
static inline void BrVec3Add(struct BrVec3 * a0, const float * a1, const struct BrVec3 * a2) { BrVec3Add(a0, (const struct BrVec3 *)a1, a2); }
static inline void BrVec3Add(struct BrVec3 * a0, const struct BrVec3 * a1, const float * a2) { BrVec3Add(a0, a1, (const struct BrVec3 *)a2); }
static inline void BrVec3Add(float * a0, const float * a1, const struct BrVec3 * a2) { BrVec3Add((struct BrVec3 *)a0, (const struct BrVec3 *)a1, a2); }
static inline void BrVec3Add(float * a0, const struct BrVec3 * a1, const float * a2) { BrVec3Add((struct BrVec3 *)a0, a1, (const struct BrVec3 *)a2); }
static inline void BrVec3Add(struct BrVec3 * a0, const float * a1, const float * a2) { BrVec3Add(a0, (const struct BrVec3 *)a1, (const struct BrVec3 *)a2); }
static inline void BrVec3Add(float * a0, const float * a1, const float * a2) { BrVec3Add((struct BrVec3 *)a0, (const struct BrVec3 *)a1, (const struct BrVec3 *)a2); }
static inline void BrVec3AddTo(float * a0, const struct BrVec3 * a1) { BrVec3AddTo((struct BrVec3 *)a0, a1); }
static inline void BrVec3AddTo(struct BrVec3 * a0, const float * a1) { BrVec3AddTo(a0, (const struct BrVec3 *)a1); }
static inline void BrVec3AddTo(float * a0, const float * a1) { BrVec3AddTo((struct BrVec3 *)a0, (const struct BrVec3 *)a1); }
static inline void BrVec3Copy(float * a0, const struct BrVec3 * a1) { BrVec3Copy((struct BrVec3 *)a0, a1); }
static inline void BrVec3Copy(struct BrVec3 * a0, const float * a1) { BrVec3Copy(a0, (const struct BrVec3 *)a1); }
static inline void BrVec3Copy(float * a0, const float * a1) { BrVec3Copy((struct BrVec3 *)a0, (const struct BrVec3 *)a1); }
static inline void BrVec3Cross(float * a0, const struct BrVec3 * a1, const struct BrVec3 * a2) { BrVec3Cross((struct BrVec3 *)a0, a1, a2); }
static inline void BrVec3Cross(struct BrVec3 * a0, const float * a1, const struct BrVec3 * a2) { BrVec3Cross(a0, (const struct BrVec3 *)a1, a2); }
static inline void BrVec3Cross(struct BrVec3 * a0, const struct BrVec3 * a1, const float * a2) { BrVec3Cross(a0, a1, (const struct BrVec3 *)a2); }
static inline void BrVec3Cross(float * a0, const float * a1, const struct BrVec3 * a2) { BrVec3Cross((struct BrVec3 *)a0, (const struct BrVec3 *)a1, a2); }
static inline void BrVec3Cross(float * a0, const struct BrVec3 * a1, const float * a2) { BrVec3Cross((struct BrVec3 *)a0, a1, (const struct BrVec3 *)a2); }
static inline void BrVec3Cross(struct BrVec3 * a0, const float * a1, const float * a2) { BrVec3Cross(a0, (const struct BrVec3 *)a1, (const struct BrVec3 *)a2); }
static inline void BrVec3Cross(float * a0, const float * a1, const float * a2) { BrVec3Cross((struct BrVec3 *)a0, (const struct BrVec3 *)a1, (const struct BrVec3 *)a2); }
static inline void BrVec3Direction(float * a0, const struct BrVec3 * a1, const struct BrVec3 * a2) { BrVec3Direction((struct BrVec3 *)a0, a1, a2); }
static inline void BrVec3Direction(struct BrVec3 * a0, const float * a1, const struct BrVec3 * a2) { BrVec3Direction(a0, (const struct BrVec3 *)a1, a2); }
static inline void BrVec3Direction(struct BrVec3 * a0, const struct BrVec3 * a1, const float * a2) { BrVec3Direction(a0, a1, (const struct BrVec3 *)a2); }
static inline void BrVec3Direction(float * a0, const float * a1, const struct BrVec3 * a2) { BrVec3Direction((struct BrVec3 *)a0, (const struct BrVec3 *)a1, a2); }
static inline void BrVec3Direction(float * a0, const struct BrVec3 * a1, const float * a2) { BrVec3Direction((struct BrVec3 *)a0, a1, (const struct BrVec3 *)a2); }
static inline void BrVec3Direction(struct BrVec3 * a0, const float * a1, const float * a2) { BrVec3Direction(a0, (const struct BrVec3 *)a1, (const struct BrVec3 *)a2); }
static inline void BrVec3Direction(float * a0, const float * a1, const float * a2) { BrVec3Direction((struct BrVec3 *)a0, (const struct BrVec3 *)a1, (const struct BrVec3 *)a2); }
static inline float BrVec3Dist(const float * a0, const struct BrVec3 * a1) { return BrVec3Dist((const struct BrVec3 *)a0, a1); }
static inline float BrVec3Dist(const struct BrVec3 * a0, const float * a1) { return BrVec3Dist(a0, (const struct BrVec3 *)a1); }
static inline float BrVec3Dist(const float * a0, const float * a1) { return BrVec3Dist((const struct BrVec3 *)a0, (const struct BrVec3 *)a1); }
static inline float BrVec3DistSq(const float * a0, const struct BrVec3 * a1) { return BrVec3DistSq((const struct BrVec3 *)a0, a1); }
static inline float BrVec3DistSq(const struct BrVec3 * a0, const float * a1) { return BrVec3DistSq(a0, (const struct BrVec3 *)a1); }
static inline float BrVec3DistSq(const float * a0, const float * a1) { return BrVec3DistSq((const struct BrVec3 *)a0, (const struct BrVec3 *)a1); }
static inline float BrVec3DistXY(const float * a0, const struct BrVec3 * a1) { return BrVec3DistXY((const struct BrVec3 *)a0, a1); }
static inline float BrVec3DistXY(const struct BrVec3 * a0, const float * a1) { return BrVec3DistXY(a0, (const struct BrVec3 *)a1); }
static inline float BrVec3DistXY(const float * a0, const float * a1) { return BrVec3DistXY((const struct BrVec3 *)a0, (const struct BrVec3 *)a1); }
static inline void BrVec3Div(float * a0, const struct BrVec3 * a1, float a2) { BrVec3Div((struct BrVec3 *)a0, a1, a2); }
static inline void BrVec3Div(struct BrVec3 * a0, const float * a1, float a2) { BrVec3Div(a0, (const struct BrVec3 *)a1, a2); }
static inline void BrVec3Div(float * a0, const float * a1, float a2) { BrVec3Div((struct BrVec3 *)a0, (const struct BrVec3 *)a1, a2); }
static inline void BrVec3DivBy(float * a0, float a1) { BrVec3DivBy((struct BrVec3 *)a0, a1); }
static inline float BrVec3Dot(const float * a0, const struct BrVec3 * a1) { return BrVec3Dot((const struct BrVec3 *)a0, a1); }
static inline float BrVec3Dot(const struct BrVec3 * a0, const float * a1) { return BrVec3Dot(a0, (const struct BrVec3 *)a1); }
static inline float BrVec3Dot(const float * a0, const float * a1) { return BrVec3Dot((const struct BrVec3 *)a0, (const struct BrVec3 *)a1); }
static inline float BrVec3LenXY(const float * a0) { return BrVec3LenXY((const struct BrVec3 *)a0); }
static inline float BrVec3Length(const float * a0) { return BrVec3Length((const struct BrVec3 *)a0); }
static inline void BrVec3Lerp(float * a0, const struct BrVec3 * a1, const struct BrVec3 * a2, float a3) { BrVec3Lerp((struct BrVec3 *)a0, a1, a2, a3); }
static inline void BrVec3Lerp(struct BrVec3 * a0, const float * a1, const struct BrVec3 * a2, float a3) { BrVec3Lerp(a0, (const struct BrVec3 *)a1, a2, a3); }
static inline void BrVec3Lerp(struct BrVec3 * a0, const struct BrVec3 * a1, const float * a2, float a3) { BrVec3Lerp(a0, a1, (const struct BrVec3 *)a2, a3); }
static inline void BrVec3Lerp(float * a0, const float * a1, const struct BrVec3 * a2, float a3) { BrVec3Lerp((struct BrVec3 *)a0, (const struct BrVec3 *)a1, a2, a3); }
static inline void BrVec3Lerp(float * a0, const struct BrVec3 * a1, const float * a2, float a3) { BrVec3Lerp((struct BrVec3 *)a0, a1, (const struct BrVec3 *)a2, a3); }
static inline void BrVec3Lerp(struct BrVec3 * a0, const float * a1, const float * a2, float a3) { BrVec3Lerp(a0, (const struct BrVec3 *)a1, (const struct BrVec3 *)a2, a3); }
static inline void BrVec3Lerp(float * a0, const float * a1, const float * a2, float a3) { BrVec3Lerp((struct BrVec3 *)a0, (const struct BrVec3 *)a1, (const struct BrVec3 *)a2, a3); }
static inline void BrVec3Midpoint(float * a0, const struct BrVec3 * a1, const struct BrVec3 * a2) { BrVec3Midpoint((struct BrVec3 *)a0, a1, a2); }
static inline void BrVec3Midpoint(struct BrVec3 * a0, const float * a1, const struct BrVec3 * a2) { BrVec3Midpoint(a0, (const struct BrVec3 *)a1, a2); }
static inline void BrVec3Midpoint(struct BrVec3 * a0, const struct BrVec3 * a1, const float * a2) { BrVec3Midpoint(a0, a1, (const struct BrVec3 *)a2); }
static inline void BrVec3Midpoint(float * a0, const float * a1, const struct BrVec3 * a2) { BrVec3Midpoint((struct BrVec3 *)a0, (const struct BrVec3 *)a1, a2); }
static inline void BrVec3Midpoint(float * a0, const struct BrVec3 * a1, const float * a2) { BrVec3Midpoint((struct BrVec3 *)a0, a1, (const struct BrVec3 *)a2); }
static inline void BrVec3Midpoint(struct BrVec3 * a0, const float * a1, const float * a2) { BrVec3Midpoint(a0, (const struct BrVec3 *)a1, (const struct BrVec3 *)a2); }
static inline void BrVec3Midpoint(float * a0, const float * a1, const float * a2) { BrVec3Midpoint((struct BrVec3 *)a0, (const struct BrVec3 *)a1, (const struct BrVec3 *)a2); }
static inline void BrVec3MulAdd(float * a0, const struct BrVec3 * a1, const struct BrVec3 * a2, float a3) { BrVec3MulAdd((struct BrVec3 *)a0, a1, a2, a3); }
static inline void BrVec3MulAdd(struct BrVec3 * a0, const float * a1, const struct BrVec3 * a2, float a3) { BrVec3MulAdd(a0, (const struct BrVec3 *)a1, a2, a3); }
static inline void BrVec3MulAdd(struct BrVec3 * a0, const struct BrVec3 * a1, const float * a2, float a3) { BrVec3MulAdd(a0, a1, (const struct BrVec3 *)a2, a3); }
static inline void BrVec3MulAdd(float * a0, const float * a1, const struct BrVec3 * a2, float a3) { BrVec3MulAdd((struct BrVec3 *)a0, (const struct BrVec3 *)a1, a2, a3); }
static inline void BrVec3MulAdd(float * a0, const struct BrVec3 * a1, const float * a2, float a3) { BrVec3MulAdd((struct BrVec3 *)a0, a1, (const struct BrVec3 *)a2, a3); }
static inline void BrVec3MulAdd(struct BrVec3 * a0, const float * a1, const float * a2, float a3) { BrVec3MulAdd(a0, (const struct BrVec3 *)a1, (const struct BrVec3 *)a2, a3); }
static inline void BrVec3MulAdd(float * a0, const float * a1, const float * a2, float a3) { BrVec3MulAdd((struct BrVec3 *)a0, (const struct BrVec3 *)a1, (const struct BrVec3 *)a2, a3); }
static inline void BrVec3MulAddTo(float * a0, const struct BrVec3 * a1, float a2) { BrVec3MulAddTo((struct BrVec3 *)a0, a1, a2); }
static inline void BrVec3MulAddTo(struct BrVec3 * a0, const float * a1, float a2) { BrVec3MulAddTo(a0, (const struct BrVec3 *)a1, a2); }
static inline void BrVec3MulAddTo(float * a0, const float * a1, float a2) { BrVec3MulAddTo((struct BrVec3 *)a0, (const struct BrVec3 *)a1, a2); }
static inline void BrVec3Negate(float * a0, const struct BrVec3 * a1) { BrVec3Negate((struct BrVec3 *)a0, a1); }
static inline void BrVec3Negate(struct BrVec3 * a0, const float * a1) { BrVec3Negate(a0, (const struct BrVec3 *)a1); }
static inline void BrVec3Negate(float * a0, const float * a1) { BrVec3Negate((struct BrVec3 *)a0, (const struct BrVec3 *)a1); }
static inline void BrVec3Normalise(float * a0) { BrVec3Normalise((struct BrVec3 *)a0); }
static inline void BrVec3Project(float * a0, const struct BrVec3 * a1, const struct BrMat4 * a2) { BrVec3Project((struct BrVec3 *)a0, a1, a2); }
static inline void BrVec3Project(struct BrVec3 * a0, const float * a1, const struct BrMat4 * a2) { BrVec3Project(a0, (const struct BrVec3 *)a1, a2); }
static inline void BrVec3Project(float * a0, const float * a1, const struct BrMat4 * a2) { BrVec3Project((struct BrVec3 *)a0, (const struct BrVec3 *)a1, a2); }
static inline void BrVec3Scale(float * a0, const struct BrVec3 * a1, float a2) { BrVec3Scale((struct BrVec3 *)a0, a1, a2); }
static inline void BrVec3Scale(struct BrVec3 * a0, const float * a1, float a2) { BrVec3Scale(a0, (const struct BrVec3 *)a1, a2); }
static inline void BrVec3Scale(float * a0, const float * a1, float a2) { BrVec3Scale((struct BrVec3 *)a0, (const struct BrVec3 *)a1, a2); }
static inline void BrVec3ScaleBy(float * a0, float a1) { BrVec3ScaleBy((struct BrVec3 *)a0, a1); }
static inline void BrVec3Sub(float * a0, const struct BrVec3 * a1, const struct BrVec3 * a2) { BrVec3Sub((struct BrVec3 *)a0, a1, a2); }
static inline void BrVec3Sub(struct BrVec3 * a0, const float * a1, const struct BrVec3 * a2) { BrVec3Sub(a0, (const struct BrVec3 *)a1, a2); }
static inline void BrVec3Sub(struct BrVec3 * a0, const struct BrVec3 * a1, const float * a2) { BrVec3Sub(a0, a1, (const struct BrVec3 *)a2); }
static inline void BrVec3Sub(float * a0, const float * a1, const struct BrVec3 * a2) { BrVec3Sub((struct BrVec3 *)a0, (const struct BrVec3 *)a1, a2); }
static inline void BrVec3Sub(float * a0, const struct BrVec3 * a1, const float * a2) { BrVec3Sub((struct BrVec3 *)a0, a1, (const struct BrVec3 *)a2); }
static inline void BrVec3Sub(struct BrVec3 * a0, const float * a1, const float * a2) { BrVec3Sub(a0, (const struct BrVec3 *)a1, (const struct BrVec3 *)a2); }
static inline void BrVec3Sub(float * a0, const float * a1, const float * a2) { BrVec3Sub((struct BrVec3 *)a0, (const struct BrVec3 *)a1, (const struct BrVec3 *)a2); }
static inline void BrVec3SubFrom(float * a0, const struct BrVec3 * a1) { BrVec3SubFrom((struct BrVec3 *)a0, a1); }
static inline void BrVec3SubFrom(struct BrVec3 * a0, const float * a1) { BrVec3SubFrom(a0, (const struct BrVec3 *)a1); }
static inline void BrVec3SubFrom(float * a0, const float * a1) { BrVec3SubFrom((struct BrVec3 *)a0, (const struct BrVec3 *)a1); }
static inline void BrVec3Zero(float * a0) { BrVec3Zero((struct BrVec3 *)a0); }
/* The original's empty stubs (0x10008D60 and its twins) are cdecl, and
 * callers pass them whatever the call site had; C accepts that through an
 * unprototyped declaration, C++ needs the arguments taken and ignored. */
static inline int BrPodNop(int, ...) { return BrPodNop(); }
#endif
#endif
