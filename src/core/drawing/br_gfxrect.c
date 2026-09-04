/* br_gfxrect.c -- drawing: the flat-colour fills.
 *
 * RESPONSIBILITY: drawing/ -- turn geometry and images into pixels.
 *
 * Filed out of slice2_17.c, an address batch and not a module.  This is the
 * home of the fill family -- 0x1002AB99 (clear the screen to one colour) and
 * 0x1002AD39 (fill a rectangle) -- and of the two empty functions that sit
 * between them in the original at 0x1002AB8F and 0x1002AB94.
 *
 * ONLY THE TWO EMPTY ONES ARE HERE YET.  The clear and the fill both write
 * through slice2_17.c's file-static g_s17 state block, which some thirty
 * other functions in that file still share, so moving them would mean two
 * copies of one state block and a silently broken port.  They belong here
 * and follow when that state has one owner.
 */
#ifdef BR_MATCHING_BUILD
/* slice2_17.h prototypes a list pointer the original never takes. */
#define BrPtrListContains BrPtrListContains_port
#endif
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include "slice2_17.h"
#ifdef BR_MATCHING_BUILD
#undef BrPtrListContains
#endif

#include <math.h>
#include <stdio.h>
#include <string.h>

#ifdef BR_MATCHING_BUILD

/* WHAT IT DOES: empty function (/Od frame, nothing else). */
/* @implements 0x1002AB8F glide BrNop_1002AB8F */

void BrNop_1002AB8F(void)

{
  return;
}

/* WHAT IT DOES: empty function (/Od frame, nothing else). */
/* @implements 0x1002AB94 glide BrNop_1002AB94 */

void BrNop_1002AB94(void)

{
  return;
}

#endif /* BR_MATCHING_BUILD */
