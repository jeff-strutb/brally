/* br_seg2.c -- geometry: 2D segment side test.
 *
 * BrSeg2SideTest reports which side of a line two points fall on.  Its
 * sibling BrSeg2Intersect stays in slice2_21.c for now: removing it from
 * that TU reorders two x87 instructions in BrSpanAddLine (compiled after it)
 * under /O2.
 *
 * Filed out of the address batch slice2_21.c; the preamble is slice2_21.c's,
 * carried whole. That file's header note:
 *
 * Every x87 sequence below was traced instruction by instruction through its
 * fxch/fsubr/fdivr chain; the reversed operand forms (fsubr, fdivr, fsubp
 * st(n)) are spelled out in the arithmetic here rather than "tidied", because
 * `fsubr m32` is `st0 = m32 - st0` and getting that backwards is silent.
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

/* --------------------------------------------------------------------------
 * 3. 2D segment predicates
 * -------------------------------------------------------------------------- */

/* 0x1003BA70 */
/* WHAT IT DOES: asks whether two line segments cross. It first rules out the
 * easy cases where their bounding boxes do not even overlap, then checks that
 * each segment really does straddle the other. It distinguishes an ordinary
 * crossing from a balanced one, but only the first of the two straddle tests
 * gets a say in which. */
/* @implements 0x1003BA70 d3d BrSeg2Intersect */
int BrSeg2Intersect(const BrVec2 *pA, const BrVec2 *pB,
                    const BrVec2 *pC, const BrVec2 *pD)
{
    float d1, d2, dummy1, dummy2;

    /* Four separating-axis rejections on the two bounding boxes. The original
     * spells all four out with min/max selected by fcomp; the asymmetry in
     * which comparison is <= and which is < is preserved by using the same
     * strict "max < min" rejection each time. */
    {
        float maxAB = (pA->x > pB->x) ? pA->x : pB->x;
        float minCD = (pC->x < pD->x) ? pC->x : pD->x;
        if (maxAB < minCD)
            return 0;
    }
    {
        float maxCD = (pC->x > pD->x) ? pC->x : pD->x;
        float minAB = (pA->x < pB->x) ? pA->x : pB->x;
        if (maxCD < minAB)
            return 0;
    }
    {
        float maxAB = (pA->y > pB->y) ? pA->y : pB->y;
        float minCD = (pC->y < pD->y) ? pC->y : pD->y;
        if (maxAB < minCD)
            return 0;
    }
    {
        float maxCD = (pC->y > pD->y) ? pC->y : pD->y;
        float minAB = (pA->y < pB->y) ? pA->y : pB->y;
        if (maxCD < minAB)
            return 0;
    }

    {
        float dy = pB->y - pA->y;
        float dx = pB->x - pA->x;
        d1 = (pC->x - pA->x) * dy - (pC->y - pA->y) * dx;
        d2 = (pD->x - pA->x) * dy - (pD->y - pA->y) * dx;
        if (d1 != K_0 && d2 != K_0 && d1 * d2 > K_0)
            return 0;
    }
    {
        float dy = pD->y - pC->y;
        float dx = pD->x - pC->x;
        dummy1 = (pA->x - pC->x) * dy - (pA->y - pC->y) * dx;
        dummy2 = (pB->x - pC->x) * dy - (pB->y - pC->y) * dx;
        if (dummy1 != K_0 && dummy2 != K_0 && dummy1 * dummy2 > K_0)
            return 0;
    }

    /* Only the FIRST test's cross products decide 1 vs 2 -- see the header. */
    return (d1 - d2 == K_0) ? 2 : 1;
}

/* 0x1003BC90 */
/* WHAT IT DOES: asks which side of a line two points fall on: nothing if they
 * are both on the same side, and otherwise whether they straddle it normally or
 * lie exactly balanced across it. */
/* @implements 0x1003BC90 d3d BrSeg2SideTest */
int BrSeg2SideTest(const BrVec2 *pA, const BrVec2 *pB,
                   const BrVec2 *pC, const BrVec2 *pD)
{
    float dx = pB->x - pA->x;
    float dy = pB->y - pA->y;
    float d1 = (pC->x - pA->x) * dy - (pC->y - pA->y) * dx;
    float d2 = (pD->x - pA->x) * dy - (pD->y - pA->y) * dx;

    if (d1 != K_0 && d2 != K_0 && d1 * d2 > K_0)
        return 0;
    return (d1 - d2 == K_0) ? 2 : 1;
}
