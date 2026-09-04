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

#endif /* BR_MATCHING_BUILD */
