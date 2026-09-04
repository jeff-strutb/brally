/* br_uinameedit.c -- menus: cancel a driver-name edit -- put the name that was
 * set aside back on the record and clear the edit box (0x1003BA30).
 *
 * Filed out of slice6_73.c, whose preamble it keeps verbatim below so the
 * compiler's view of the body is unchanged.  The commit half of the pair is
 * still there.  The original banner follows.
 *
 * slice6_73.c -- BRD3D.dll, packet 73 (slice 6).  See slice6_73.h for the
 * scope, the conflicts and the reasons the other ten addresses are absent.
 *
 * The six screen builders are transcriptions: there is no algorithm in them,
 * and the value is entirely in the coordinates, string ids, flag words, hook
 * addresses and ordering, plus the handful of places the pattern breaks.
 *
 * Float literals are the exact values of the 32-bit patterns the original
 * pushes (195.0f == 0x43430000, 10.0f == 0x41200000, ...).  The row offsets
 * +19/+38/+57/+76/+95/+114/+133/+33 come from the NEGATIVE .rdata constants
 * at 0x1008F680..0x1008F69C, every one of which is used as an `fsub` and
 * therefore ADDS its magnitude; they were read out of orig/BRD3D.dll with
 * tools/pe.py, not assumed.
 *
 * The C++ exception frames the originals set up (`push -1 / push <funclet> /
 * fs:[0]`, plus the state variable each keeps at [esp+0x18] or [esp+0x24])
 * have no observable effect on any path that returns, and are not reproduced.
 */

#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include <string.h>
#include <stdio.h>
#include <stddef.h>

#include "slice6_73.h"

/* --- DUPLICATE OWNERSHIP (host link only) -------------------------------
 * slice6_73 and slice6_70 each independently ported 0x1003E680. Both bodies
 * are faithful; the duplication is a coordination artefact of parallel
 * passes, not a disagreement about behaviour.
 *
 * slice6_70 is the OWNER. Under BR_HOST_LINK this module's copies are renamed
 * so the full-program link has exactly one definition of each. The renaming
 * covers this file's internal calls too, so the module stays self-consistent
 * and its own test binary -- which links this .o alone -- is unaffected.
 *
 * This is a stopgap. The duplicate bodies should be deleted and the callers
 * pointed at slice6_70's, once someone has diffed the two transcriptions
 * line-by-line and confirmed they agree. Until that diff is done, deleting the
 * wrong one would silently discard the better transcription.
 * ------------------------------------------------------------------------ */
#ifdef BR_HOST_LINK
#define BrSub1003E680 BrSub1003E680_dup_73
#define BrExt_1003E680 BrExt_1003E680_dup_73
#endif


/* Br73Rec is copied from slice6_73.c verbatim -- a static, so each file
 * keeps its own; the other half of the commit/restore pair still needs it. */
/* Both reach a record through the SAME index math the original uses:
 * n*3 -> *5 -> *9 -> *8, i.e. a 0x438 stride, and both reach the flag at
 * +0x44C from the same base, which is 0x14 bytes past the end of record n.
 * Reproduced, not corrected -- see slice5_61.h, which found the same thing on
 * the sibling array. */
static unsigned char *Br73Rec(unsigned char *pBase, int32_t n)
{
    /* g_br0AB3F4 is signed and IS set to -1 by the name-reset paths; the
     * original does not guard against it.  Same arithmetic slice5_61.c uses
     * on the sibling array. */
    return pBase + (ptrdiff_t)n * (ptrdiff_t)BR61_REC29D0_STRIDE;
}

/* WHAT IT DOES: cancels a name edit, putting the name that was set aside back
 * on the record and clearing the edit box. It looks at the flag the commit
 * half above set but writes into a DIFFERENT array of records than that half
 * read from -- an asymmetry that is the original's, not a slip here. */
/* @implements 0x1003BA30 glide BrExt_100424D0 */
int32_t BrExt_100424D0(void *pArg)
{
#ifdef BR_MATCHING_BUILD
    /* Orig pushes esi/edi only on the strcpy path (after the two early
     * returns), so do not keep named locals that force a prologue save. */
    *(int32_t *)(*(char **)((char *)pArg + 0x2ae8) + 0x70) = 0;
    g_br73.nAA28EC = 0;
    if (g_brAA28D8 != 0 && g_aBrA9D078 != 0) {
        strcpy((char *)g_brPAA29D0 + g_br0AB3F4 * (int32_t)BR61_REC29D0_STRIDE
               + (int32_t)BR61_REC29D0_OFF_NAME, g_aBrA9D078);
        strcpy(g_aBrA9D078, g_aBr39B720);
    }
    return 1;
#else
    unsigned char *pRec;
    char          *pszName;

    if (g_br73.pfnClearSub70 != NULL) {     /* see BrExt_10041A00 */
        g_br73.pfnClearSub70(pArg);
    }

    g_br73.nAA28EC = 0;

    if (g_brAA28D8 == 0) {
        return 1;
    }
    /* the original tests the ADDRESS 0x10A9D078 against zero here; it is a
     * literal, so the branch is dead.  Kept as an always-true condition. */

    if (g_brPAA29D0 == NULL) {
        return 1;
    }
    pRec    = Br73Rec(g_brPAA29D0, g_br0AB3F4);
    pszName = (char *)pRec + BR61_REC29D0_OFF_NAME;

    strcpy(pszName, g_aBrA9D078);
    strcpy(g_aBrA9D078, g_aBr39B720);
    return 1;
#endif
}
