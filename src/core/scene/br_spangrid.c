/* br_spangrid.c -- testing a world point against the coarse span grid.
 *
 * RESPONSIBILITY: what is in the world and where -- the cheap grid that
 * stands in for a shape's footprint, asked whether a point lands on it.
 *
 * Moved here out of src/core/slice2_21.c (an address batch, not a module).
 */
#ifdef BR_MATCHING_BUILD
/* The original takes the two coordinates only; the port's prototype leads
 * with the volume.  Hide it so the matching body can carry the real shape. */
#define BrSpanTestPoint BrSpanTestPoint_port
#endif
#include "slice2_21.h"
#ifdef BR_MATCHING_BUILD
#undef BrSpanTestPoint
int  BrSpanTestPoint(float x, float y);
int  BrSpanContains(int param_1, int param_2);
#endif

/* 0x1008F61C / 0x1008F608 -- the cell size reciprocal. */
#define K_CELL_RECIP   0.03125f

/* 0x1003A950 */
/* WHAT IT DOES: asks whether a point falls inside the covered area, by
 * dropping it into the coarse grid and checking that cell. */
/* @implements 0x1003A950 d3d BrSpanTestPoint */
/* @implements 0x10033FD0 glide BrSpanTestPoint */
#ifdef BR_MATCHING_BUILD
int BrSpanTestPoint(float x, float y)
{
    /* Right-to-left: y ftol first, its eax is pushed, then x ftol, Contains. */
    return BrSpanContains(BrFtolArg(x * K_CELL_RECIP),
                          BrFtolArg(y * K_CELL_RECIP));
}
#else
int BrSpanTestPoint(const BrSpanVolume *pVol, float x, float y)
{
    return BrSpanTest(&pVol->grid, BrFtolArg(x * K_CELL_RECIP),
                                   BrFtolArg(y * K_CELL_RECIP));
}
#endif
