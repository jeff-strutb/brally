/* br_sortcmp.c -- drawing: the comparator a draw list is sorted with.
 *
 * RESPONSIBILITY: drawing/ -- turn geometry and images into pixels.
 *
 * Filed out of slice2_13.c, an address batch and not a module.  What the
 * records being ordered are has NOT been established -- all that is read off
 * the original is the field compared and the three results -- so the file
 * name describes the shape and not an invented purpose.
 */
#include <stdint.h>

#ifdef BR_MATCHING_BUILD

/* WHAT IT DOES: qsort comparator on the int16 at +2: 1 / 0 / -1, first argument compared
 * on the left (jle / setge). */
/* @implements 0x1000E2F0 glide BrQsortCmpS2 */

int BrQsortCmpS2(int param_1,int param_2)

{
  if (*(short *)(param_1 + 2) > *(short *)(param_2 + 2)) {
    return 1;
  }
  return (*(short *)(param_1 + 2) >= *(short *)(param_2 + 2)) - 1;
}

#endif /* BR_MATCHING_BUILD */
