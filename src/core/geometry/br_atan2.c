/* br_atan2.c -- the game's own arctangent.
 *
 * RESPONSIBILITY: geometry -- the arithmetic that moves positions around.
 *
 * Moved here out of src/core/slice2_21.c (an address batch, not a module),
 * whose preamble -- the port-name shims, slice2_21.h, <string.h> and the
 * .rdata constant table -- is carried over VERBATIM below, because a TU's
 * surroundings decide its codegen (docs/VC5-IDIOMS.md).
 */
#ifdef BR_MATCHING_BUILD
#define BrSpanTestPoint BrSpanTestPoint_port
#define BrPfxReset      BrPfxReset_port
#endif
#include "slice2_21.h"
#ifdef BR_MATCHING_BUILD
#undef BrSpanTestPoint
#undef BrPfxReset
int  BrSpanTestPoint(float x, float y);
void BrPfxReset(void);
int  BrSpanContains(int param_1, int param_2);
#endif

#include <string.h>

/* --------------------------------------------------------------------------
 * Constants, all read straight out of .rdata rather than guessed.
 * -------------------------------------------------------------------------- */
#define K_0            0.0f                    /* 0x1008F62C, 0x1008F59C */
#define K_1            1.0f                    /* 0x1008F628, 0x1008F588 */
#define K_EPS_REL      1.0000000036274937e-15f /* 0x1008F63C */
#define K_PI           3.1415927410125732f     /* 0x1008F640 */
#define K_PI_2         1.5707963705062866f     /* -0x1008F644 */
#define K_PI_4         0.7853981852531433f     /* -0x1008F648, +0x1008F658 */
#define K_PI_8         0.39269909262657166f    /* 0x1008F64C */
#define K_PI_16        0.19634954631328583f    /* 0x1008F650 */
#define K_SIN_TOL      0.004999999888241291f   /* 0x1008F654 */
#define K_CELL_RECIP   0.03125f                /* 0x1008F61C, 0x1008F608 */
#define K_CELL         32.0f                   /* 0x1008F624 */
#define K_65280_RECIP  1.5318628356908448e-05f /* 0x1008F5FC = 1/65280 */
#define K_65536_RECIP  1.5259021893143654e-05f /* 0x1008F57C = 1/65536 */

/* 0x1003B7B0 == BRGlide 0x10034E30, byte-exact 2026-09-04 after four
 * separable source facts, each one region of the diff:
 *  1. the first fold writes acc BEFORE it negates x and y (`acc = K_PI;
 *     x = -x; y = -y`); the other two orders of x/y are 4 B short.
 *  2. the rotation arm copies the OLD x (`t = x; x = y; y = -t`) -- with
 *     `t = y` VC5 loads y before it updates acc; the original updates acc,
 *     loads y, then negates.  Both rotation arms update acc FIRST.
 *  3. `if ((r = BrSqrtF(...)) == K_0)` -- assigning inside the test is what
 *     produces the original's `fld st(0); fcomp` copy-compare; a separate
 *     `r = ...; if (r == K_0)` is a bare `fcom` and one instruction short.
 *  4. `ang, step` are declared BEFORE `acc`.  Declaration order decides
 *     which of two locals is the `fld` operand of `acc + ang`: declared
 *     after ang, acc is loaded first (`fld acc; fadd ang`) as the original
 *     has it; declared before, ang is loaded first and, since the other
 *     arm also begins `fld ang`, VC5 hoists that load above the bDirect
 *     test (2 B short).  Redundant parens, a copy of acc, `acc += ang`, an
 *     inverted test and a ternary are all inert -- only the declaration
 *     order moved it. */
/* 0x1003B7B0 */
/* WHAT IT DOES: works out the compass angle of a direction. Rather than a
 * lookup table it folds the direction into one eighth of a circle and then
 * hunts for the answer by halving the interval sixteen times, stopping early
 * once it is close enough -- about a quarter of a degree. Note its two
 * arguments are in the opposite order to the C library's atan2. */
/* @implements 0x1003B7B0 d3d BrAtan2 */
float BrAtan2(float x, float y)
{
    float ang, step;
    float acc = 0.0f;
    float r;
    int   i;
    int   bDirect = 1;  /* edi: cleared by the x<y swap only */

    if (y < K_0) {
        acc = K_PI;
        x = -x;
        y = -y;
    }
    if (x < K_0) {
        float t = x;
        acc = acc + K_PI_2;
        x = y;
        y = -t;
    }
    if (x < y) {
        float t = x;
        acc += K_PI_4;
        x = y;
        y = t;
        bDirect = 0;
    }

    if ((r = BrSqrtF(y * y + x * x)) == K_0)
        return K_0;

    y = y / r;                      /* sin of the reduced angle */
    ang  = K_PI_8;
    step = K_PI_16;

    for (i = 0; i < 16; i++) {
        float s = BrSinF(ang);
        if (s < y) {
            if (y - s < K_SIN_TOL)
                break;
            ang = step + ang;
        } else if (s > y) {
            if (s - y < K_SIN_TOL)
                break;
            ang = ang - step;
        } else {
            break;                  /* exact hit: the original drops out too */
        }
        step = step * 0.5f;
    }

    if (bDirect)
        return acc + ang;
    /* fsubr against the saved offset: acc - (ang - pi/4). */
    return acc - (ang - K_PI_4);
}
