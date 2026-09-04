/* br_polydist.c -- drawing: how far a corner is from each screen edge.
 *
 * RESPONSIBILITY: drawing/ -- turn geometry and images into pixels.
 *
 * Filed out of slice4_52.c, an address batch and not a module.  Four
 * contiguous one-line routines the polygon trimmer calls, one per edge:
 *
 *     0x1000DEC0   x                  the LEFT edge
 *     0x1000DED0   constant - x       the RIGHT edge
 *     0x1000DEE0   y                  the TOP edge
 *     0x1000DEF0   constant - y       the BOTTOM edge
 *
 * The pairing is read off the offsets and the shared 0x1007720C constant,
 * and the four addresses being 0x10 apart is what says they are one
 * translation unit.  The port's BrPolyDistX / BrPolyDistY return float where
 * the matching twins return double; both spellings are kept.
 *
 * slice4_52.c's preamble is carried over verbatim.  An include set that
 * looks redundant has already been shown elsewhere in this module to move
 * VC5's register allocation (see br_rdpmode.c).
 */
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include "slice4_52.h"
#include "slice1_03.h"      /* BrComCallLocked68 (0x1000C4D0) */

#include "slice3_33.h"   /* BrUiScreen / BrUiCtl / BrUiPhase, BrOperatorNew,
                          * BrUiCtlCtor, BrErrShow  (pulls slice1_06.h)      */
#include "slice1_07.h"   /* BrTables64Clear                                  */
#include "slice3_39.h"   /* g_BrDikState / g_BrDikEdge / g_BrDikPrev,
                          * g_pBrAA2E80                                      */
#include "slice2_22.h"   /* BrDPlayRandStep, BrDPlaySendTag3, BrDPlayLink    */
#include "slice2_14.h"   /* BrScrPt                                          */
#include "slice1_01.h"   /* BrAdler32                                        */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ==========================================================================
 * 0x10010960 / 0x10010980  BrPolyDistX / BrPolyDistY
 * ========================================================================== */

/* WHAT IT DOES: tells the shape-trimming code how far a corner lies from the
 * left edge of the screen -- which is just its horizontal position, so a
 * negative answer means the corner is off to the left. Its neighbour below does
 * the same for the top edge. */
/* @implements 0x1000DEC0 glide BrPolyDistX */
float BrPolyDistX(const struct BrScrPt *pPt)
{
    return ((const BrScrPt *)pPt)->f0C;
}

float BrPolyDistY(const struct BrScrPt *pPt)
{
    return ((const BrScrPt *)pPt)->f10;
}

#ifdef BR_MATCHING_BUILD
extern int DAT_117a5f28;
extern float _DAT_1007720c;
int FUN_10069A80();
int FUN_10069a80();

/* WHAT IT DOES: return the float at offset +0x10 in a struct, cast to double. */
/* @implements 0x1000DEE0 glide BrGetFieldFloat */

double BrGetFieldFloat(int param_1)

{
  return (double)*(float *)(param_1 + 0x10);
}

/* WHAT IT DOES: return (constant at 0x1007720C) minus the float at +0xC, as double. */
/* @implements 0x1000DED0 glide BrGetFieldFloatSubC */

double BrGetFieldFloatSubC(int param_1)

{
  return (double)_DAT_1007720c - (double)*(float *)(param_1 + 0xc);
}

/* WHAT IT DOES: return (constant at 0x1007720C) minus the float at +0x10, as double. */
/* @implements 0x1000DEF0 glide BrGetFieldFloatSub10 */

double BrGetFieldFloatSub10(int param_1)

{
  return (double)_DAT_1007720c - (double)*(float *)(param_1 + 0x10);
}

#endif /* BR_MATCHING_BUILD */
