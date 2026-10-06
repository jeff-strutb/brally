/* br_quatmat.c -- a car's orientation quaternion + position -> its 4x4 matrix.
 *
 * Glide 0x10062640 stands alone in its translation unit (config/brally/tu_map.csv
 * tu_047).  BrCarState's f00 is the quaternion's scalar, f04..f0C its vector
 * part, f10..f18 the position (slice3_42.h).
 *
 * The preamble is a codegen input.  With slice3_42.h, or with br_mat.h and
 * slice1_02.h together, VC5 lands outside the original's window (+24..+48 B,
 * a different frame); either header alone, or up to ~128 prototypes, is
 * inside it.  So the matching build states BrMat4's layout here instead of
 * pulling in br_mat.h.
 */
#include "slice1_02.h"      /* BrCarState */
/* BrMat4: br_mat.h */
/* BrMat4FromCarState: prototype in br_funcs.h */

#define BR_K_0  0.0f    /* 0x10077A1C */
#define BR_K_2  2.0f    /* 0x10077A20 */
#define BR_K_1  1.0f    /* 0x10077A24 */

/* A float rounding point VC5 cannot see through: the value goes to memory
 * as a float and comes back through its int image. */
#define BR_QM_ROUND(dst, expr) \
    { float r_ = (expr); int i_ = *(int *)&r_; dst = *(float *)&i_; }

/* By value: the inlined parameter is a fresh temp, which VC5 homes (a dword
 * copy through eax/edx, then `fld [slot]; fmul [slot]`) or keeps on the x87
 * stack (`fld st(0); fmulp`) exactly as the original does for the four
 * squares. */
static __inline float BrSq(float a)
{
    return a * a;
}

/* WHAT IT DOES: turns a car's stored orientation quaternion into the 4x4
 * matrix the renderer draws with, and copies the car's position into the
 * translation row.  The quaternion need not be unit length: the rotation
 * terms are scaled by 2/|q|^2, and a zero quaternion gives the identity
 * rotation.  Called once per car per frame. */
/* Hand-transcribed from the Glide bytes 2026-09-27.
 *  - the four squares go through an inline by-value helper, declared
 *    x, y, z, w: that reproduces the original's integer copies of y, w and
 *    x into the dead pSrc slot and z squared on the stack.
 *  - the zero test takes the zero arm only on the equal flag (`test
 *    ah,0x40; jne`), so `norm != 0` guards the divide.
 *  - s lives in the dead pSrc slot (a float: the original stores 2/norm).
 *  - the cross terms follow the original's rounding points: w*zs, w*ys and
 *    w*xs are stored to a float slot and read back; y*xs is used once from
 *    the stack (m[1][0]) and once reloaded from its float copy (m[0][1]);
 *    xs/ys/zs, xz and yz stay on the stack.  BR_QM_ROUND forces each store.
 * HISTORY: the byte-closest body (365/363 B, one fxch) paired the two copies
 *    of y*xs the other way round -- m[1][0] got the rounded one -- and was
 *    1 ulp off on 566 of 2000 random inputs.  Its dead list: which statement
 *    holds the assignment; operand orders of every product and sum; `-t + s`,
 *    `s + -t`, `-(t - s)`; copy variables; int pads 0-80; five system
 *    headers; /TP. */
/* @t3 0x10062640 2026-09-27 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 433/363 insns 147/125 rows 19+41 regions 3 oracle EQUIVALENT
 * @t3-effort passes 4 zero-movement 3 4
 * Residue: the cross terms are written for the original's ROUNDING POINTS
 * (w*zs, w*ys, w*xs and the reloaded copy of y*xs are float stores; the
 * rest stays on the 53-bit x87 stack), not for its bytes.  The 365/363 B
 * body it replaced paired the rounded and unrounded copies of y*xs the other
 * way round and was 1 ulp off on 566 of 2000 random inputs; this one is
 * bit-identical on all 2000 (differential emulation against 0x10062640).
 * Placed as the /O2 compile (config/brally/t3_variant_c.csv).  Do not reopen
 * before the end-grind. */
/* @t4-pass 0x10062640 1 2026-09-27 probes 45 bytes 365 insns 126 regions 1 rows 1 census yes  (hand: preamble census -- int pads 0-76, 5 system headers, C and /TP) */
/* @t4-pass 0x10062640 2 2026-09-27 probes 82 bytes 365 insns 126 regions 1 rows 1 census no  (hand: operand orders, sign forms, copy variables, statement placement of the y*xs / w*zs pair) */
/* @t4-pass 0x10062640 3 2026-09-27 probes 12 bytes 433 insns 147 regions 3 rows 60 census no  (hand: 12 compiler option sets on the rounding-exact body) */
/* @t4-pass 0x10062640 4 2026-09-27 probes 32 bytes 433 insns 147 regions 3 rows 60 census yes  (hand: symbol-table band sweep 0-992 on the rounding-exact body) */
/* @implements 0x100695D0 d3d BrMat4FromCarState */
void BrMat4FromCarState(BrMat4 *pOut, const BrCarState *pSrc)
{
    float xx = BrSq(pSrc->f04), yy = BrSq(pSrc->f08),
          zz = BrSq(pSrc->f0C), ww = BrSq(pSrc->f00);
    float yz2 = zz + yy;
    float norm = ww + yz2 + xx;
    float s;

    if (norm != BR_K_0)
        s = BR_K_2 / norm;
    else
        s = 0.0f;

    pOut->m[0][0] = BR_K_1 - s * yz2;
    pOut->m[1][1] = BR_K_1 - s * (zz + xx);
    pOut->m[2][2] = BR_K_1 - s * (yy + xx);

    {
        const double xs = pSrc->f04 * s, ys = pSrc->f08 * s, zs = pSrc->f0C * s;
        float wz, xy_r, yw, xw;
        double xy;

        BR_QM_ROUND(wz, pSrc->f00 * zs)
        xy = pSrc->f08 * xs;
        BR_QM_ROUND(xy_r, xy)
        pOut->m[1][0] = (float)(xy - wz);
        pOut->m[0][1] = (float)(xy_r + wz);
        {
            const double xz = pSrc->f0C * xs;
            BR_QM_ROUND(yw, pSrc->f00 * ys)
            pOut->m[2][0] = (float)(xz + yw);
            pOut->m[0][2] = (float)(xz - yw);
        }
        {
            const double yz = pSrc->f0C * ys;
            BR_QM_ROUND(xw, pSrc->f00 * xs)
            pOut->m[2][1] = (float)(yz - xw);
            pOut->m[1][2] = (float)(yz + xw);
        }
    }

    pOut->m[3][0] = pSrc->f10;
    pOut->m[3][1] = pSrc->f14;
    pOut->m[3][2] = pSrc->f18;
    pOut->m[0][3] = 0.0f;
    pOut->m[1][3] = 0.0f;
    pOut->m[2][3] = 0.0f;
    pOut->m[3][3] = 1.0f;
}
