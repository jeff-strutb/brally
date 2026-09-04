/* br_strget.c -- menus: fetch one piece of on-screen wording by number
 * (0x10074030). Every menu caption, button label and message comes through
 * here. The table itself is still defined in slice4_52.c.
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
 * 0x10074030  BrStrGet
 * ========================================================================== */

/* WHAT IT DOES: fetches one of the game's pieces of on-screen wording by
 * number -- every menu caption, button label and message comes through here.
 * A number that is not in the table gives nothing back rather than an error. */
/* @implements 0x10074030 d3d BrStrGet */
const char *BrStrGet(int id)
{
    /* br_bits.h's BrHandleLookup IS this function with the table address
     * turned into an argument; the original INLINES it here (byte shape:
     * both range tests jump to one shared `return NULL`).  The range test
     * is unsigned, which is what makes a negative id fall out as NULL. */
    if ((uint32_t)id >= 1u && (uint32_t)id < (uint32_t)BR_STR_TABLE_COUNT) {
        return (const char *)g_apBrStrTable[id];
    }
    return NULL;
}
