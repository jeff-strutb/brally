/* br_keyclear.c -- menus: forget the keyboard state when a screen changes, so
 * a key still held does not register again on the new screen (0x1005FF30).
 *
 * Filed out of slice4_52.c, whose preamble it keeps verbatim below so the
 * compiler's view of the body is unchanged.  The original banner follows.
 *
 * slice4_52.c -- BRD3D.dll, a later pass.  See slice4_52.h, especially the note
 * about the packet listing being mis-paired: everything below was decompiled
 * from asm/ at the address named on the `WANTED AS` line, not from the body
 * printed under it in work/slice4/agent52.asm.
 *
 * Float literals are the exact values of the 32-bit patterns the original
 * pushes (195.0f == 0x43430000, 460.0f == 0x43E60000, ...).
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
 * 0x1005FF30  BrMenuSub1005FF30
 * ========================================================================== */

/* WHAT IT DOES: forgets everything the game currently believes about the
 * keyboard -- which keys are held, which were just pressed, and what was held
 * last frame -- so that keys still down when a screen changes do not carry over
 * and register again. It only clears the first 64 entries of each table, not
 * all of them. */
/* @implements 0x1005FF30 d3d BrMenuSub1005FF30 */
/* @n64 0x8021E5C4 located */
void BrMenuSub1005FF30(void)
{
    /* Three inlined `rep stosd` of 0x40 dwords.  BrTables64Clear is the same
     * body as a callee; the original does not call it.  The size is a dword
     * count, not an element count of the two 256-entry int32 arrays. */
    memset(g_BrDikState, 0, BR_TABLE64_COUNT * sizeof(uint32_t));
    memset(g_BrDikEdge,  0, BR_TABLE64_COUNT * sizeof(uint32_t));
    memset(g_BrDikPrev,  0, BR_TABLE64_COUNT * sizeof(uint32_t));
}
