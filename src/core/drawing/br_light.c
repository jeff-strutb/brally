/* br_light.c -- drawing: the direction pair the lighting hardware is given.
 *
 * RESPONSIBILITY: drawing/ -- turn geometry and images into pixels.
 *
 * Filed out of slice2_17.c, an address batch and not a module.  The hardware
 * takes its two light directions as signed bytes, so the camera basis has to
 * be built and then squeezed down: 0x1002A4D0 does the building, 0x1002A490
 * is the squeeze, and they are 0x40 bytes apart in the original.
 *
 * slice2_17.c's preamble is carried over verbatim.  An include set that
 * looks redundant has already been shown elsewhere in this module to move
 * VC5's register allocation (see br_rdpmode.c), so nothing is trimmed here
 * on the grounds that it is unused.
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

/* 0x10030E20 */
/* WHAT IT DOES: builds a camera transform and then squeezes two of its axes
 * down into the byte-sized direction pair the lighting hardware wants, so the
 * scene is lit relative to how it is being viewed. */
/* @implements 0x1002A4D0 glide BrLightDirsFromLookAt */
/* @implements 0x10030E20 d3d BrLightDirsFromLookAt */
void BrLightDirsFromLookAt(BrMat4 *pM, BrLightPair *pLights,
                           float xEye, float yEye, float zEye,
                           float xAt,  float yAt,  float zAt,
                           float xUp,  float yUp,  float zUp)
{
    /* Orig inlines the six pack calls (186 B). A static helper stays a
     * CALL and the body collapses to 75 B. */
    BrMat4LookAt(pM, xEye, yEye, zEye, xAt, yAt, zAt, xUp, yUp, zUp);
    pLights->dir0[0] = BrPackNormalByte((double)pM->m[0][0]);
    pLights->dir0[1] = BrPackNormalByte((double)pM->m[1][0]);
    pLights->dir0[2] = BrPackNormalByte((double)pM->m[2][0]);
    pLights->dir1[0] = BrPackNormalByte((double)pM->m[0][1]);
    pLights->dir1[1] = BrPackNormalByte((double)pM->m[1][1]);
    pLights->dir1[2] = BrPackNormalByte((double)pM->m[2][1]);
}

#ifdef BR_MATCHING_BUILD

extern double _DAT_10077488;
extern double _DAT_10077490;

/* WHAT IT DOES: convert a floating-point value into the signed byte the
 * hardware wants, scaling and flipping it and then clamping into -128..127
 * so an out-of-range input saturates rather than wrapping. */
/* @implements 0x1002A490 glide FUN_1002a490 */
/* auto-filed from ghidra --refine; transforms: as-is */

int FUN_1002a490(double param_1)

{
  int iVar1;
  
  iVar1 = (int)floor(_DAT_10077490 - param_1 * _DAT_10077488);
  if (iVar1 < -0x80) {
    iVar1 = -0x80;
  }
  if (0x7f < iVar1) {
    iVar1 = 0x7f;
  }
  return iVar1;
}

#endif /* BR_MATCHING_BUILD */
