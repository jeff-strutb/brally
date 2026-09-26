/* br_span.c -- audio.
 *
 * Filed out of the address batches: these functions were
 * matched first and grouped by what they are afterwards.
 * Every function carries its original address.
 */
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import
 * table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include <stdint.h>

#ifdef BR_MATCHING_BUILD

extern int DAT_10ac2c54;
extern int DAT_10ac2c5c;
extern int DAT_10ac2c60;
extern int DAT_10ac2d60;

/* WHAT IT DOES: 1 if row `param_2` is inside the span band [0x10AC2C5C, 0x10AC2C54] and
 * `param_1` lies within that row's [lo, hi] from the two 256-entry tables. Params sit on
 * the LEFT of every comparison (cmp reg,mem order). */
/* @implements 0x10033F90 glide BrSpanContains */

int BrSpanContains(int param_1,int param_2)

{
  if ((((param_2 >= DAT_10ac2c5c) && (param_2 <= DAT_10ac2c54)) &&
      (param_1 >= (int)(&DAT_10ac2c60)[param_2])) && (param_1 <= (int)(&DAT_10ac2d60)[param_2])) {
    return 1;
  }
  return 0;
}


/* WHAT IT DOES: clamp (x, row) into 0..0x3F and widen row's [lo, hi] span to
 * include x.  The upper clamp is spelled `>= 0x40` (cmp 0x40 / jl), not
 * `> 0x3f`; params sit on the LEFT of both table compares. */
/* @implements 0x10033CE0 glide BrSpanExtend */

void BrSpanExtend(int param_1,int param_2)

{
  if (param_1 < 0) {
    param_1 = 0;
  }
  if (param_1 >= 0x40) {
    param_1 = 0x3f;
  }
  if (param_2 < 0) {
    param_2 = 0;
  }
  if (param_2 >= 0x40) {
    param_2 = 0x3f;
  }
  if (param_1 < (int)(&DAT_10ac2c60)[param_2]) {
    (&DAT_10ac2c60)[param_2] = param_1;
  }
  if (param_1 > (int)(&DAT_10ac2d60)[param_2]) {
    (&DAT_10ac2d60)[param_2] = param_1;
  }
  return;
}

extern int DAT_10ac2c50;
extern int DAT_10ac2c58;
int  BrFtolTrunc(float f);

/* WHAT IT DOES: marks a straight line's footprint onto the coarse grid of
 * 32-unit cells: both its endpoints, the column and row ranges it spans,
 * and then every cell the line passes through as it climbs from one row to
 * the next, including the cells either side when it runs close to a cell
 * boundary.  This is how a shape's outline becomes a set of covered cells.
 * The row loop runs on its own counter (a copy of the first row): looping
 * on the row variable itself stores it for the fild at the wrong point.
 * The column pair is swapped with the three-xor idiom, as the original. */
/* @implements 0x10033D30 glide BrSpanAddLineG */
void BrSpanAddLineG(float x0, float y0, float x1, float y1)
{
    int lo, hi, row, rowEnd, col, i;
    float dy, y, x;

    BrSpanExtend(BrFtolTrunc(x0 * 0.03125f), BrFtolTrunc(y0 * 0.03125f));
    BrSpanExtend(BrFtolTrunc(x1 * 0.03125f), BrFtolTrunc(y1 * 0.03125f));
    lo = BrFtolTrunc(x0 * 0.03125f);
    hi = BrFtolTrunc(x1 * 0.03125f);
    if (lo > hi) {
        hi ^= lo;
        lo ^= hi;
        hi ^= lo;
    }
    if (lo < DAT_10ac2c58)
        DAT_10ac2c58 = lo;
    if (hi > DAT_10ac2c50)
        DAT_10ac2c50 = hi;
    if (y0 > y1) {
        float t;
        t = x0; x0 = x1; x1 = t;
        t = y0; y0 = y1; y1 = t;
    }
    dy = y1 - y0;
    if (dy == 0.0f)
        return;
    row = BrFtolTrunc(y0 * 0.03125f);
    rowEnd = BrFtolTrunc(y1 * 0.03125f);
    if (row < DAT_10ac2c5c)
        DAT_10ac2c5c = row;
    if (rowEnd > DAT_10ac2c54)
        DAT_10ac2c54 = rowEnd;
    for (i = row; i <= rowEnd; i++) {
        y = (float)i * 32.0f;
        if (y < y0)
            continue;
        if (y > y1)
            continue;
        x = ((x1 - x0) * (y - y0)) / dy + x0;
        col = BrFtolTrunc(x * 0.03125f);
        BrSpanExtend(col, i - 1);
        BrSpanExtend(col, i);
        if (x <= (float)col * 32.0f) {
            BrSpanExtend(col - 1, i - 1);
            BrSpanExtend(col - 1, i);
        }
        if (x >= (float)(col + 1) * 32.0f) {
            BrSpanExtend(col + 1, i - 1);
            BrSpanExtend(col + 1, i);
        }
    }
}

#endif /* BR_MATCHING_BUILD */
