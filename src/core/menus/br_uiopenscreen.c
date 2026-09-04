/* br_uiopenscreen.c -- menus: the two menu-row handlers that open a screen --
 * build it if it is not there, make it current, run its builder, and wire the
 * new screen's back row (0x100458A0 and 0x10045880).
 *
 * Filed out of slice7_81.c, whose preamble it keeps verbatim below so the
 * compiler's view of these bodies is unchanged.  The original banner follows.
 *
 * slice7_81.c -- the screen-entry and phase-changing control hooks, over
 * br_ui.h's struct model.
 *
 * See slice7_81.h for what this module is, which addresses it duplicates, why
 * a second transcription exists at all, and the four conflicts it reports.
 * The short version: every body here also exists in port/src/slice3_31.c (and
 * one in port/src/slice2_26.c) over slice2_25.h's byte-offset `BrGameObj`, and
 * the argument these hooks receive is a `BrUiCtl_`, whose fields have moved
 * under LP64.
 *
 * Transcribed from orig/BRGlide.dll and cross-checked against orig/BRD3D.dll.
 */
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include "slice7_81.h"

#include "slice5_61.h"   /* BrSub1003E510 (0x1003E510), g_br0AB3F4 (0x100AB3F4) */
#include "slice5_62.h"   /* BrExt_100419D0 (0x100419D0)                         */
#include "slice6_70.h"   /* BrExt_1003E680 (0x1003E680)                         */

#include <stddef.h>
#include <string.h>

/* WHAT IT DOES: the handler on a menu row that opens one particular screen:
 * it builds that screen if it does not exist yet, makes it current, runs its
 * builder, and then wires the new screen's back row so it knows how to get
 * out again. The argument -- the row that was chosen -- is pushed and then
 * never looked at. */
/* @implements 0x100458A0 d3d BrUiHook81_100458A0 */
int32_t BrUiHook81_100458A0(BrUiCtl_ *pCtl)
{
#ifdef BR_MATCHING_BUILD
    /* Orig pushes the unused pCtl, then stores +0x08 unguarded. */
    ((int32_t (*)(BrUiCtl_ *))BrUiHook81Activate_10045BC0)(pCtl);
    g_br73.pAA29F4->pfn08 = BrUiHook81_10046B10;
    return 1;
#else
    BrUiCtl_ *pBack;

    (void)pCtl;                       /* pushed, ignored by the callee */
    (void)BrUiHook81Activate_10045BC0();

    pBack = g_br73.pAA29F4;           /* 0x10AA29F4 -- a CONTROL */
    if (pBack != NULL)                /* DEVIATION: guarded */
        pBack->pfn08 = BrUiHook81_10046B10;
    return 1;
#endif
}

/* WHAT IT DOES: the same open-a-screen handler for a different screen. It
 * nudges one shared string first, builds and enters the screen, and wires
 * that screen's back row to the matching leave routine. Note the screen this
 * one opens is not transcribed anywhere in this tree yet, so following it
 * today lands on an empty screen. */
/* @implements 0x10045880 d3d BrUiHook81_10045880 */
int32_t BrUiHook81_10045880(BrUiCtl_ *pCtl)
{
#ifdef BR_MATCHING_BUILD
    /* Orig pushes the unused pCtl, then stores +0x08 unguarded. */
    ((int32_t (*)(BrUiCtl_ *))BrUiHook81Activate_100451E0)(pCtl);
    g_br73.pAA29C8->pfn08 = BrUiHook81_10046AD0;
    return 1;
#else
    BrUiCtl_ *pBack;

    (void)pCtl;
    (void)BrUiHook81Activate_100451E0();

    pBack = g_br73.pAA29C8;           /* 0x10AA29C8 -- a CONTROL */
    if (pBack != NULL)                /* DEVIATION: guarded */
        pBack->pfn08 = BrUiHook81_10046AD0;
    return 1;
#endif
}
