/* br_span.h -- 64-row span grid, decompiled from BRD3D.dll.
 *
 * A scanline-span accumulator: for each of 64 rows it tracks the minimum and
 * maximum column touched, then answers point-inside queries against those
 * spans. This is the classic shape of a coverage/culling structure.
 *
 * Recovered functions and the globals they use:
 *   0x1003A660  BrSpanAdd     mins at 0x10A9BBD0, maxs at 0x10A9BCD0
 *   0x1003A910  BrSpanTest    row bounds at 0x10A9BBCC / 0x10A9BBC4
 *   0x1003A4D0  BrSpanReset   also resets a 255-entry free list (see .c)
 *
 * Both coordinates are clamped to [0,63] on insert -- the original does this
 * explicitly rather than rejecting out-of-range input, so callers may rely on
 * the clamping behaviour.
 *
 * The exact purpose (track visibility, collision buckets) is not established;
 * the name describes the mechanism, not an assumed use.
 */
#ifndef BR_SPAN_H
#define BR_SPAN_H

#define BR_SPAN_ROWS 64

typedef struct BrSpanGrid {
    int aMin[BR_SPAN_ROWS];    /* 0x10A9BBD0 */
    int aMax[BR_SPAN_ROWS];    /* 0x10A9BCD0 */
    int rowLo;                 /* 0x10A9BBCC */
    int rowHi;                 /* 0x10A9BBC4 */
} BrSpanGrid;

/* 0x1003A660  clamp (col,row) to [0,63] and widen that row's span. */
void BrSpanAdd(BrSpanGrid *pGrid, int col, int row);

/* 0x1003A910  1 if (col,row) lies within the accumulated spans, else 0.
 * Note the original tests the row against rowLo/rowHi with jl/jg, so the
 * bounds are inclusive. */
int  BrSpanTest(const BrSpanGrid *pGrid, int col, int row);

/* Reset spans to empty. (The original 0x1003A4D0 does more than this; see
 * the note in br_span.c.) */
void BrSpanReset(BrSpanGrid *pGrid);

#endif /* BR_SPAN_H */
