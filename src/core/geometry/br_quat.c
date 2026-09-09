/* br_quat.c -- build a normalised 4-vector (quaternion) from a 3x4 matrix.
 *
 * Glide 0x1006F840 stands alone: it reads a rotation matrix's rows (stride
 * 0x10), chooses one of four arms by comparing diagonal combinations against
 * the image constant at 0x10077C38, fills the four output floats and hands
 * them to BrVec4Normalise. */
#include "br_vec.h"

#ifdef BR_MATCHING_BUILD
extern float g_077C38;   /* 0x10077C38 -- branch threshold */
extern float g_077C4C;   /* 0x10077C4C */
extern float g_077C50;   /* 0x10077C50 */
#else
static float g_077C38, g_077C4C, g_077C50;
#endif

/* WHAT IT DOES: turns a rotation held as a 3x4 matrix into the equivalent
 * unit quaternion. It picks whichever of four formulas is numerically safest
 * for this particular rotation by comparing the matrix's diagonal terms, fills
 * in the four quaternion components, then normalises them to unit length. */
/* @implements 0x1006F840 glide BrQuatFromMatrix */
void BrQuatFromMatrix(float *m, float *q)
{
    /* Each guard is written `expr >= threshold` so the >= arm falls through
     * and the < arm is the branch target -- the original's layout (the
     * opposite spelling swaps every arm and inverts je/jne). */
    if (*m >= g_077C38) {
        /* The compared sum survives on the x87 stack into this arm's first
         * component (`fcom`, then `fadd st(1)`); a recomputed
         * `*m + m[10] + m[5]` would pop-compare and reload instead. */
        float s = m[10] + m[5];

        if (s >= g_077C38) {
            q[0] = *m + s - g_077C4C;
            q[1] = m[6] - m[9];
            q[2] = m[8] - m[2];
            q[3] = m[1] - m[4];
        } else {
            q[0] = m[6] - m[9];
            q[1] = ((*m - g_077C4C) - m[5]) - m[10];
            q[2] = m[4] + m[1];
            q[3] = m[2] + m[8];
        }
    } else {
        if (m[5] - m[10] >= g_077C38) {
            q[0] = m[8] - m[2];
            q[1] = m[4] + m[1];
            q[2] = (m[5] - (*m - g_077C50)) - m[10];
            q[3] = m[6] + m[9];
        } else {
            q[0] = m[1] - m[4];
            q[1] = m[2] + m[8];
            q[2] = m[6] + m[9];
            {
                /* Name m[0] in a temp declared IMMEDIATELY before its use so
                 * it is the FLD operand of the commutative add (orig `fld
                 * [ecx]; fadd [ecx+0x14]`); unnamed, the sum feeds an fsub and
                 * VC5 loads m[5] first. A temp declared at block top instead
                 * spills across the three stores above. */
                float m0 = m[0];
                q[3] = m[10] - ((m0 + m[5]) - g_077C50);
            }
        }
    }
    BrVec4Normalise((BrVec4 *)q);
}
