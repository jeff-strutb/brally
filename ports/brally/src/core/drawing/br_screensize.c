/* br_screensize.c -- drawing: pushing the screen size out to its consumers.
 *
 * RESPONSIBILITY: drawing/ -- turn geometry and images into pixels.
 *
 * Filed out of slice2_17.c, an address batch and not a module.  Six one-line
 * routines, called when the resolution is chosen or changed, so that nothing
 * has to read the live size every frame.
 *
 * slice2_17.c's preamble is carried over verbatim.  An include set that
 * looks redundant has already been shown elsewhere in this module to move
 * VC5's register allocation (see br_rdpmode.c), so nothing is trimmed here
 * on the grounds that it is unused.
 */
/* slice2_17.h prototypes a list pointer the original never takes. */
#define BrPtrListContains BrPtrListContains_port
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include "slice2_17.h"
#undef BrPtrListContains

#include <math.h>
#include <stdio.h>
#include <string.h>


/* ==========================================================================
 * Screen-size fan-out: 0x1001E1E0 - 0x1001E2B0
 *
 * WHAT THEY DO (as a family): six one-line routines that push the current
 * screen size out to the six places that keep their own copy of it. Three
 * take the size itself, three take HALF of it as a float -- that is the
 * screen CENTRE, which is what the projection and HUD code actually wants.
 * They are called when the resolution is chosen or changed, so nothing has to
 * read the live size on every frame.
 *
 * These are also the six functions that carry a 16-byte link-stage preamble
 * (jmp +0x0b, then nops) recorded in config/preambles.csv -- see
 * tools/image_build.py, which has to lay that preamble down verbatim or the
 * whole function reads as differing.
 * ========================================================================== */

/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* WHAT IT DOES: copy the screen HEIGHT to the consumer at 0x105CCFE0. */
/* @implements 0x1001E200 glide THUNK_1001E200 */
/* auto-filed from ghidra --refine; transforms: as-is */

void THUNK_1001E200(void)

{
  (*(int *)&BrGlClipMaxY) = BrGbiRectG_A7518;
  return;
}


/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* WHAT IT DOES: copy the screen WIDTH to the consumer at 0x105D17B8. */
/* @implements 0x1001E1E0 glide THUNK_1001E1E0 */
/* auto-filed from ghidra --refine; transforms: as-is */

void THUNK_1001E1E0(void)

{
  (*(int *)&BrGlClipMaxX) = BrGbiRectG_A7514;
  return;
}


/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* WHAT IT DOES: half the screen width, as a float -- the horizontal
 * centre -- for the consumer at 0x105CCD48. */
/* @implements 0x1001E220 glide THUNK_1001E220 */
/* auto-filed from ghidra --refine; transforms: as-is */

void THUNK_1001E220(void)

{
  DAT_105ccd48 = (float)(BrGbiRectG_A7514 / 2);
  return;
}


/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* WHAT IT DOES: half the screen height, as a float -- the vertical
 * centre -- for the consumer at 0x105CCFDC. */
/* @implements 0x1001E250 glide THUNK_1001E250 */
/* auto-filed from ghidra --refine; transforms: as-is */

void THUNK_1001E250(void)

{
  DAT_105ccfdc = (float)(BrGbiRectG_A7518 / 2);
  return;
}


/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* WHAT IT DOES: the horizontal centre again, for a SECOND consumer at
 * 0x105CD9F8. Two subsystems keep their own copy. */
/* @implements 0x1001E280 glide THUNK_1001E280 */
/* auto-filed from ghidra --refine; transforms: as-is */

void THUNK_1001E280(void)

{
  DAT_105cd9f8 = (float)(BrGbiRectG_A7514 / 2);
  return;
}


/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* WHAT IT DOES: the vertical centre again, for the same second consumer
 * pair, at 0x105CD9FC. */
/* @implements 0x1001E2B0 glide THUNK_1001E2B0 */
/* auto-filed from ghidra --refine; transforms: as-is */

void THUNK_1001E2B0(void)

{
  DAT_105cd9fc = (float)(BrGbiRectG_A7518 / 2);
  return;
}


/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include <string.h>

#include "slice1_07.h"   /* BrDevSlot -- see the note in slice3_39.h */
/* Header prototype is cdecl; matching needs thiscall.  Rename the cdecl
 * declaration so the definition below can wear a different convention. */
#define BrTextBoxDeleteDtor BrTextBoxDeleteDtor_cdecl
#define BrTextBoxMeasureA  BrTextBoxMeasureA_cdecl
#define BrTextBoxMeasureB  BrTextBoxMeasureB_cdecl
#define BrTextBoxInit BrTextBoxInit_port
#include "slice3_39.h"
#undef BrTextBoxInit
#undef BrTextBoxDeleteDtor
#undef BrTextBoxMeasureA
#undef BrTextBoxMeasureB

/* WHAT IT DOES: records the screen's width and height and works out its
 * centre point, then clears seven other numbers. Its argument is never
 * looked at. */
/* @implements 0x10060210 d3d BrFn10060210 */
int32_t __stdcall BrFn10060210(void)
{
    int i;


    BrGlNavMaxX = (*(int32_t *)&BrGbiRectG_A7514);
    g_pBrAA2E80->x = (*(int32_t *)&BrGbiRectG_A7514) / 2;   /* cdq/sub/sar: toward zero */

    BrGlNavMaxY = (*(int32_t *)&BrGbiRectG_A7518);
    g_pBrAA2E80->y = (*(int32_t *)&BrGbiRectG_A7518) / 2;

    for (i = 0; i < 7; ++i) {
        g_BrAA3398[i] = 0;
    }
    return 1;
}
