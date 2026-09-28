/* br_quatmat.c -- a car's orientation quaternion + position -> its 4x4 matrix.
 *
 * Glide 0x10062640 stands alone in its translation unit (config/tu_map.csv
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
#ifdef BR_MATCHING_BUILD
typedef struct BrMat4 { float m[4][4]; } BrMat4;   /* = br_mat.h's */
void BrMat4FromCarState(BrMat4 *pOut, const BrCarState *pSrc);
#else
#include "slice3_42.h"
#endif

#define BR_K_0  0.0f    /* 0x10077A1C */
#define BR_K_2  2.0f    /* 0x10077A20 */
#define BR_K_1  1.0f    /* 0x10077A24 */

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
 *  - s lives in the dead pSrc slot, and the first cross pair reuses s for
 *    y*xs.  That makes the original's `fst [s]; fld [s]; fstp [s]` sequence:
 *    y*xs stored, reloaded, and its slot taken by w*zs.
 *  - the other two pairs name the w product first (`q`), which is the one
 *    the original homes; the other stays on the x87 stack.
 * RESIDUE, 365/363 B, one `fxch st(1)`: the original subtracts on the
 *    register copy of y*xs and adds on the reloaded one; ours does the
 *    reverse, so m[1][0] and m[0][1] each see the other copy of xy.
 *    Inert: which statement holds the assignment (`- t` first gives t its
 *    own slot, 4 instructions); both operand orders of every product and sum;
 *    `-t + s`, `s + -t`, `-(t - s)`; t declared with or without a spare;
 *    copy variables (u = s ...); int pads 0-80; <windows.h>/<math.h>/
 *    <stdio.h>/<string.h>/<stdlib.h>; /TP.  Writing y*xs into t
 *    with s = w*zs gets the whole store/reload sequence right but homes zs
 *    (w at offset 0 outranks it), 7 instructions. */
/* @t3 0x10062640 2026-09-27 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 365/363 insns 126/125 rows 0+1 regions 1 oracle EQUIVALENT
 * @t3-effort passes 2 zero-movement 1 2
 * Residue: one fxch -- which copy of y*xs (register or reloaded) feeds
 * m[1][0] and which feeds m[0][1].  Dossier and dead list above.  Do not
 * reopen before the end-grind (project rule 12). */
/* @t4-pass 0x10062640 1 2026-09-27 probes 45 bytes 365 insns 126 regions 1 rows 1 census yes  (hand: preamble census -- int pads 0-76, 5 system headers, C and /TP) */
/* @t4-pass 0x10062640 2 2026-09-27 probes 82 bytes 365 insns 126 regions 1 rows 1 census no  (hand: operand orders, sign forms, copy variables, statement placement of the y*xs / w*zs pair) */
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
        float xs = pSrc->f04 * s, ys = pSrc->f08 * s, zs = pSrc->f0C * s;
        float t;

        t = pSrc->f00 * zs;
        pOut->m[0][1] = (s = pSrc->f08 * xs) + t;
        pOut->m[1][0] = s - t;
        {
            float q = pSrc->f00 * ys, p = pSrc->f0C * xs;
            pOut->m[2][0] = p + q;
            pOut->m[0][2] = p - q;
        }
        {
            float q = pSrc->f00 * xs, p = pSrc->f0C * ys;
            pOut->m[2][1] = p - q;
            pOut->m[1][2] = p + q;
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
