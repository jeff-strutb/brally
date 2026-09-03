/* br_strres.c -- see br_strres.h.
 *
 * RESPONSIBILITY: locate, read and decode the game's own files.  BRString.dll
 * is the localised text; this is the loader that turns it into the pointer
 * table BrStrGet indexes.
 */
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include "br_strres.h"

#include "slice4_52.h"   /* g_apBrStrTable, BR_STR_TABLE_COUNT -- the table
                          * this module fills.  It is NOT redefined here. */
#include "slice6_78.h"   /* BrChkFReadOpen / BrChkFileSize / BrChkFClose,
                          * D3D 0x10002FE0 / 0x10002F90 / 0x10003290 ==
                          * Glide 0x10003320 / 0x100032D0 / 0x100035E0 */

#include <stdlib.h>

/* ==========================================================================
 * Storage this module owns
 * ========================================================================== */

char   *g_pBrStrResBlob;   /* 0x1186C944 */
int32_t g_brStrResUsed;    /* 0x1186C948 */
int32_t g_brStrResSize;    /* 0x1186C94C */

/* ==========================================================================
 * 0x1006D1BE .. 0x1006D1E1 -- how big is BRString.dll?
 * ==========================================================================
 *
 * Three calls and no error handling of its own: CHK_FReadOpen exits the
 * process if the file is missing, which is why there is nothing to check
 * here.  The handle is the original's two-field {FILE *, char *name} block
 * and is only ever passed straight back out; slice6_78.h explains why its
 * type is spelled `FILE **`. */
static int32_t br_strres_measure(const char *pszPath)
{
    FILE **pFile = BrChkFReadOpen(pszPath);
    int32_t cb   = (int32_t)BrChkFileSize(pFile);
    BrChkFClose(pFile);
    return cb;
}

/* ==========================================================================
 * 0x1006D1A0 -- load the string resource
 * ========================================================================== */

/* WHAT IT DOES: loads the game's text -- every menu label, message and
 * prompt -- out of the resource module into one block of memory, and builds
 * the table that maps a string number to its text. It clears the table
 * first, and then does nothing at all if the text has already been loaded. */
/* @implements 0x1006D1A0 glide BrStrResLoad */
void BrStrResLoad(const BrStrResOps *pOps)
{
    void   *hModule;
    int32_t id;
    int32_t used;

    /* 0x1006D1A3:  mov ecx, 0x12E / rep stosd  at 0x1186C48C.
     *
     * The count is 0x12E DWORDS -- 302 pointers, not 302 bytes -- and the
     * destination is &table[1], because 0x1186C488 is &table[0] and BrStrGet
     * rejects id 0.  Slot 0 is neither cleared nor ever written.
     *
     * The block ends at 0x1186C944, which is where g_pBrStrResBlob lives, so
     * the store at 0x1006D208 lands exactly ONE PAST the cleared region and
     * the two do not overlap.  Worth stating because a `rep stos` and a
     * later store into the same neighbourhood have aliased here before. */
    for (id = 1; id < BR_STR_TABLE_COUNT; id++) {
        g_apBrStrTable[id] = NULL;
    }

    /* 0x1006D1B1.  AFTER the clear -- see the re-entry note in br_strres.h. */
    if (g_pBrStrResBlob != NULL) {
        return;
    }

    if (pOps == NULL || pOps->pfnLoadModule == NULL ||
        pOps->pfnLoadString == NULL || pOps->pfnFreeModule == NULL) {
        /* Not in the original, which has the three imports linked in.  A
         * caller with no platform gets no strings rather than a fabricated
         * table: the clear above has already run, which is the original's
         * state after a LoadLibraryA failure too. */
        return;
    }

    g_brStrResSize = br_strres_measure(BR_STRRES_PATH);

    hModule = pOps->pfnLoadModule(BR_STRRES_PATH);
    if (hModule == NULL) {
        return;                                  /* 0x1006D1F3 */
    }

    /* 0x1006D1F5:  malloc(size).  Plain malloc, not CHK_AllocateMemory --
     * so no diagnostic and no exit on failure, unlike everything else this
     * boot path allocates. */
    g_pBrStrResBlob = (char *)malloc((size_t)g_brStrResSize);
    if (g_pBrStrResBlob == NULL) {
        pOps->pfnFreeModule(hModule);            /* 0x1006D266 */
        return;
    }

    /* 0x1006D210:  eax = g_brStrResUsed, once, before the walk.  Both arms
     * of the loop re-load it from the global afterwards, so `used` and the
     * global never diverge -- kept as a local because that is what the
     * register is, and because it makes the "not found does not advance"
     * arm visible.
     *
     * NOTE it is not zeroed.  A load after a BrStrResFree starts at 0
     * because Free zeroes it; a load with a stale non-zero cursor would
     * write past the head of a fresh blob.  Nothing does that today. */
    used = g_brStrResUsed;

    /* 0x1006D220 .. 0x1006D263.  esi walks &table[1] up to (not including)
     * 0x1186C944 == &table[0x12F], and edi is the id, so 1..0x12E. */
    for (id = 1; id < BR_STR_TABLE_COUNT; id++) {
        int cch = pOps->pfnLoadString(hModule, (uint32_t)id,
                                      g_pBrStrResBlob + used,
                                      (int)(g_brStrResSize - used));

        if (cch != 0) {
            /* 0x1006D23E.  The pointer stored is blob + the cursor as it was
             * BEFORE this call, and the cursor advances by cch + 1 -- the
             * terminator LoadStringA wrote.  Consecutive strings are packed
             * with no padding and no alignment. */
            used = g_brStrResUsed;
            g_apBrStrTable[id] = g_pBrStrResBlob + used;
            used = used + cch + 1;
            g_brStrResUsed = used;
        } else {
            /* 0x1006D254.  Slot stays NULL, cursor does not move, so the
             * next id reuses the same bytes. */
            used = g_brStrResUsed;
        }
    }

    pOps->pfnFreeModule(hModule);                /* 0x1006D266 */
}

/* ==========================================================================
 * 0x1006D2A0 -- release it
 * ========================================================================== */

/* WHAT IT DOES: frees the block of game text. Note the lookup table is
 * deliberately not cleared, so all three hundred entries are left pointing
 * into memory that has just been given back -- that is the original's
 * behaviour. */
/* @implements 0x1006D2A0 glide BrStrResFree */
void BrStrResFree(void)
{
    if (g_pBrStrResBlob == NULL) {
        return;                                  /* 0x1006D2A7 */
    }
    free(g_pBrStrResBlob);
    g_pBrStrResBlob = NULL;
    g_brStrResUsed  = 0;
    g_brStrResSize  = 0;
    /* g_apBrStrTable is deliberately NOT cleared: the original leaves all
     * 302 pointers aimed at the freed block. */
}

/* @n64 0x80255E64 located */
void BrStrResResetForTest(void)
{
    g_pBrStrResBlob = NULL;
    g_brStrResUsed  = 0;
    g_brStrResSize  = 0;
}
