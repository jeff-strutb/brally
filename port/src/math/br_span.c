/* br_span.c -- span grid decompiled from BRD3D.dll. See br_span.h.
 *
 * NOTE on BrSpanReset: the original at 0x1003A4D0 resets this grid *and*
 * rebuilds an unrelated 255-entry free list (words at 0x10A99BF4, stride
 * 0x20, filled 2,3,4,... which is a next-index chain) and zeroes a run of
 * 0x2B68-byte buffers. Those belong to allocator/pool modules that are not
 * decompiled yet, so only the span-grid half is reproduced here. This is a
 * deliberate partial transcription, not an oversight -- do not treat
 * BrSpanReset as a complete port of 0x1003A4D0.
 */
#include "br_span.h"


static int clamp63(int v)
{
    /* The original: `test/jge` against 0 then `cmp 0x40 / jl` else 0x3F. */
    if (v < 0)
        return 0;
    if (v >= BR_SPAN_ROWS)
        return BR_SPAN_ROWS - 1;
    return v;
}

void BrSpanAdd(BrSpanGrid *pGrid, int col, int row)
{
    col = clamp63(col);
    row = clamp63(row);

    if (col < pGrid->aMin[row])
        pGrid->aMin[row] = col;
    if (col > pGrid->aMax[row])
        pGrid->aMax[row] = col;
}

int BrSpanTest(const BrSpanGrid *pGrid, int col, int row)
{
    if (row < pGrid->rowLo || row > pGrid->rowHi)
        return 0;
    if (col < pGrid->aMin[row] || col > pGrid->aMax[row])
        return 0;
    return 1;
}

void BrSpanReset(BrSpanGrid *pGrid)
{
    int i;
    for (i = 0; i < BR_SPAN_ROWS; i++) {
        pGrid->aMin[i] = BR_SPAN_ROWS;   /* 64 == empty, per 0x1003A990 */
        pGrid->aMax[i] = 0;
    }
    pGrid->rowLo = 0;
    pGrid->rowHi = BR_SPAN_ROWS - 1;
}
