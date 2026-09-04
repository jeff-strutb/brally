/* br_uiwindow.c -- menus: hand the main window over to the UI root object
 * (0x10060260). Both operands are globals; the declared parameter is
 * discarded.
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
 * 0x10060260  BrSub10060260
 * ========================================================================== */

/* WHAT IT DOES: purpose unclear. Observably it ignores whatever it is handed
 * and calls one other routine with two fixed globals -- the input object and a
 * window handle -- so it exists to supply that pair rather than to do anything
 * itself. What the routine it calls is for is not established here. */
/* 0x100603A0 is __thiscall in the original -- `this` in ecx, the one stack
 * argument cleaned by the callee (`ret 4`); see slice5_60.h.  VC5's C compiler
 * has no __thiscall keyword, but __fastcall places the first REGISTER-ELIGIBLE
 * argument in ecx, and a struct is never register-eligible, so a 4-byte struct
 * in second position is forced onto the stack.  That reproduces thiscall's
 * register/stack split and its callee-cleanup exactly. */
#ifdef BR_MATCHING_BUILD
typedef struct { void *p; } BrSub603A0Arg;
typedef void(__fastcall *BrSub603A0ThisCall)(void *pThis, BrSub603A0Arg arg);
#endif

/* WHAT IT DOES: hand the main window over to the UI root object. GOTCHA: the
 * declared parameter has no counterpart in the original and is DISCARDED --
 * both operands come from globals. */
/* @implements 0x10060260 d3d BrSub10060260 */
void BrSub10060260(void *pThis)
{
    /* Both operands come from globals.  The declared parameter has no
     * counterpart in the original and is discarded -- see the header. */
    (void)pThis;
#ifdef BR_MATCHING_BUILD
    {
        BrSub603A0Arg arg;
        arg.p = g_brP680584;
        ((BrSub603A0ThisCall)BrSub100603A0)((void *)g_pBrAA2E80, arg);
    }
#else
    BrSub100603A0((void *)g_pBrAA2E80, g_brP680584);
#endif
}
