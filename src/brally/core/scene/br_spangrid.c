/* br_spangrid.c -- testing a world point against the coarse span grid.
 *
 * RESPONSIBILITY: what is in the world and where -- the cheap grid that
 * stands in for a shape's footprint: built from the shape's edges
 * (BrSpanBuildHull), and asked whether a point lands on it.
 *
 * Moved here out of src/brally/core/slice2_21.c (an address batch, not a module).
 */
/* The original takes the two coordinates only; the port's prototype leads
 * with the volume.  Hide it so the matching body can carry the real shape. */
#define BrSpanTestPoint BrSpanTestPoint_port
#include "slice2_21.h"
#undef BrSpanTestPoint
int  BrSpanTestPoint(float x, float y);
int  BrSpanContains(int param_1, int param_2);

/* 0x1008F61C / 0x1008F608 -- the cell size reciprocal. */
#define K_CELL_RECIP   0.03125f

/* 0x1003A950 */
/* WHAT IT DOES: asks whether a point falls inside the covered area, by
 * dropping it into the coarse grid and checking that cell. */
/* @implements 0x1003A950 d3d BrSpanTestPoint */
/* @implements 0x10033FD0 glide BrSpanTestPoint */
int BrSpanTestPoint(float x, float y)
{
    /* Right-to-left: y ftol first, its eax is pushed, then x ftol, Contains. */
    return BrSpanContains(BrFtolArg(x * K_CELL_RECIP),
                          BrFtolArg(y * K_CELL_RECIP));
}

/* 0x1003A990 */
/* WHAT IT DOES: works out the coarse footprint of an eight-sided shape -- a
 * top point, a bottom point and a four-corner ring between them -- by wiping the
 * grid, drawing all twelve of its edges onto it, and then reducing each column
 * to the first and last row that the shape reaches. The result is a cheap
 * stand-in for the shape that later tests can be run against. */
/* @implements 0x1003A990 d3d BrSpanBuildHull */
/* NO ARGUMENTS, ABSOLUTE GLOBALS, AND THE TWELVE EDGES UNROLLED. The port
 * takes the volume and the point array as parameters and walks a static edge
 * table; the original reads both as absolute globals and emits twelve
 * separate calls, which is 86 of the 86 missing instructions.
 *
 *   0x106EA3A0  the six points, stride 12 (only x and y are read)
 *   0x10AC2F60  aRowHi[64]   0x10AC2E60  aRowLo[64]
 *   0x10AC2D60  aMax[64]     0x10AC2C60  aMin[64]
 *   0x10AC2C50  colHi   0x10AC2C54  rowHi
 *   0x10AC2C58  colLo   0x10AC2C5C  rowLo
 *
 * The two inner scans are UNBOUNDED in the original (`inc edx; jmp` /
 * `dec ecx; jmp`), so a column no row covers walks off both ends of the
 * 64-entry arrays. Reproduced; the port arm below keeps its bounds.
 *
 * Two source facts: the four scalar initialisers come BEFORE the four
 * fill loops (after them, VC5 pools 0x3F into edi across the twelve calls
 * and the clamps lose ebp/edi), and the TU is plain /O2 -- the original
 * uses ebp as a general register, which /Oy- forbids. */
extern float   g_aBrSpanPt[6][3];   /* 0x106EA3A0 */
extern int32_t g_aBrSpanRowHi[64];  /* 0x10AC2F60 */
extern int32_t g_aBrSpanRowLo[64];  /* 0x10AC2E60 */
extern int32_t g_aBrSpanMax[64];    /* 0x10AC2D60 */
extern int32_t g_aBrSpanMin[64];    /* 0x10AC2C60 */
extern int32_t g_brSpanColHi;       /* 0x10AC2C50 */
extern int32_t g_brSpanRowHiG;      /* 0x10AC2C54 */
extern int32_t g_brSpanColLo;       /* 0x10AC2C58 */
extern int32_t g_brSpanRowLoG;      /* 0x10AC2C5C */
extern void BrSpanAddLineG(float ax, float ay, float bx, float by);

#define BR_SPAN_EDGE(a, b)                                              \
    BrSpanAddLineG(g_aBrSpanPt[a][0], g_aBrSpanPt[a][1],                \
                   g_aBrSpanPt[b][0], g_aBrSpanPt[b][1])

void BrSpanBuildHull(void)
{
    int32_t i, col, lo, hi;

    g_brSpanColHi  = 0;
    g_brSpanRowHiG = 0;
    g_brSpanColLo  = 0x3F;
    g_brSpanRowLoG = 0x3F;

    for (i = 0; i < 64; i++) g_aBrSpanRowHi[i] = 0;
    for (i = 0; i < 64; i++) g_aBrSpanRowLo[i] = 64;
    for (i = 0; i < 64; i++) g_aBrSpanMax[i]   = 0;
    for (i = 0; i < 64; i++) g_aBrSpanMin[i]   = 64;

    BR_SPAN_EDGE(0, 1);
    BR_SPAN_EDGE(0, 2);
    BR_SPAN_EDGE(0, 3);
    BR_SPAN_EDGE(0, 4);
    BR_SPAN_EDGE(5, 1);
    BR_SPAN_EDGE(5, 2);
    BR_SPAN_EDGE(5, 3);
    BR_SPAN_EDGE(5, 4);
    BR_SPAN_EDGE(1, 2);
    BR_SPAN_EDGE(2, 3);
    BR_SPAN_EDGE(3, 4);
    BR_SPAN_EDGE(4, 1);

    if (g_brSpanColLo  < 0)  g_brSpanColLo  = 0;
    if (g_brSpanRowLoG < 0)  g_brSpanRowLoG = 0;
    if (g_brSpanColHi  >= 64) g_brSpanColHi  = 0x3F;
    if (g_brSpanRowHiG >= 64) g_brSpanRowHiG = 0x3F;

    for (col = g_brSpanColLo; col <= g_brSpanColHi; col++) {
        lo = g_brSpanRowLoG;
        while (col < g_aBrSpanMin[lo] || col > g_aBrSpanMax[lo])
            lo++;
        hi = g_brSpanRowHiG;
        while (col < g_aBrSpanMin[hi] || col > g_aBrSpanMax[hi])
            hi--;
        g_aBrSpanRowLo[col] = lo;
        g_aBrSpanRowHi[col] = hi;
    }
}
