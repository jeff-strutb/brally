/* br_mat.h -- matrix math, decompiled from BRD3D.dll.
 *
 * Matrices are row-major with a 16-byte (4 float) row stride -- the outer
 * loop of the original advances the matrix pointer by 0x10 per output
 * component while the inner loop steps by 4. Only the upper 3x3 participates
 * in these two routines; the fourth column is skipped, not zeroed, so the
 * storage is a 4x4 whose translation column these functions ignore.
 */
#ifndef BR_MAT_H
#define BR_MAT_H

#include "br_vec.h"

/* Row-major 4x4; row stride is what the original arithmetic implies. */
typedef struct BrMat4 { float m[4][4]; } BrMat4;

/* 0x10074720  out[i] = sum_k m[i][k] * v[k]   (rotate by M) */
void BrMat4MulVec3(BrVec3 *pOut, const BrMat4 *pM, const BrVec3 *pV);

/* 0x10074770  out[i] = sum_k m[k][i] * v[k]   (rotate by M-transpose)
 * The original walks the same buffer with a 0x10 inner stride instead of 4,
 * which is exactly a transposed access pattern. For a pure rotation this is
 * the inverse rotation. */
void BrMat4MulVec3Transposed(BrVec3 *pOut, const BrMat4 *pM, const BrVec3 *pV);

/* 0x100349C0  project pV through pM (v' = v*M, linear+projection column only,
 * no translation row) and divide x/y/z by the resulting w.  Glide-only.
 * NOTE the argument order is (out, v, M) -- vector before matrix, unlike the
 * two MulVec3 routines above. */
void BrVec3Project(BrVec3 *pOut, const BrVec3 *pV, const BrMat4 *pM);

/* 0x100307A0  copy a full 4x4 (16 dwords).
 *
 * WARNING: argument order is SOURCE FIRST, unlike every routine in br_vec.h
 * which takes the destination first. Verified from the original: it loads
 * arg2 into the write cursor and reads through an arg1-arg2 displacement.
 * Do not "fix" this to match the others -- callers depend on it. */
void BrMat4Copy(const BrMat4 *pSrc, BrMat4 *pDst);

/* 0x100307D0  set to the 4x4 identity (float). */
void BrMat4Identity(BrMat4 *pM);

/* 0x10030810 -- guFrustumF (N64 SDK), as modified by Boss Game Studios.
 *
 * Identified from the string "Error: guFrustumF(): unable to compute matrix"
 * reached by its error path, and from three equality guards (l==r, b==t,
 * n==f) that jump to it.
 *
 * IMPORTANT: this build takes SEVEN arguments. Stock libultra guFrustumF has
 * an eighth `scale` parameter; it is absent here (the function never reads
 * [esp+0x20]). Do not substitute a stock implementation.
 *
 * Layout is row-major with -1 at [2][3], i.e. the row-vector convention
 * (v' = v * M), matching BrMat4MulVec3. The -1 is a hardcoded 0xBF800000
 * store, and every zero slot is an explicit store, so the layout below is
 * read directly off the original rather than inferred.
 *
 * Returns 0 on success, non-zero if the frustum is degenerate (in which case
 * the matrix is left untouched, exactly as the original does). */
int BrMat4Frustum(BrMat4 *pM, float l, float r, float b, float t,
                  float n, float f);

/* 0x10030930 -- guPerspective (N64 SDK), as modified by Boss Game Studios.
 *
 * Builds a frustum from a vertical field of view, delegating to
 * BrMat4Frustum. Confirmed from the disassembly: the constant at 0x1008F4A8
 * is pi/360 (so fovy is in DEGREES and the multiply yields the half-angle in
 * radians), followed by `fptan`, multiplies by aspect and near, exactly two
 * `fchs` negations (for -l and -b), then the call to 0x10030810.
 *
 * DEVIATION FROM STOCK: the original writes a hardcoded 1 to *pPerspNorm
 * (`mov word ptr [eax], 1`). Stock libultra computes a real perspective
 * normalisation value from near/far. Reproduced as-is.
 *
 * ERRATUM: this declaration is INCOMPLETE. Two call sites (0x10033E83,
 * 0x10033F7E) clean up 28 bytes = SEVEN arguments, and one uses literals
 * pinning the order: (mf, perspNorm, 45.0f, 1.3333334f, 10.0f, 2000.0f, 1.0f)
 * -- i.e. stock guPerspectiveF INCLUDING a trailing `scale` omitted below.
 * slice2_19 declares the correct form as BrMat4Perspective7. Prefer that.
 *
 * CAVEAT: the argument ORDER below follows libultra's guPerspectiveF and is
 * consistent with the observed operation sequence and the perspNorm write,
 * but the interleaved pushes make the stack slots hard to pin absolutely.
 * Worth re-checking against a real call site before relying on it. */
int BrMat4Perspective(BrMat4 *pM, unsigned short *pPerspNorm,
                      float fovyDegrees, float aspect, float n, float f);

/* 0x100310F0  build a scale matrix: diagonal (sx, sy, sz, 1), rest zero.
 * Same 4x4 layout as BrMat4Frustum -- the diagonal stores land on +0x00,
 * +0x14, +0x28 and a hardcoded 1.0f (0x3F800000) on +0x3C. */
void BrMat4Scale(BrMat4 *pM, float sx, float sy, float sz);

/* 0x1002A7F0  build a translation matrix: identity with (dx, dy, dz) in the
 * bottom row at +0x30, +0x34, +0x38 and 1.0f on the diagonal. */
void BrMat4Translate(BrMat4 *pM, float dx, float dy, float dz);

#endif /* BR_MAT_H */
