/* br_strres.c -- see br_strres.h.
 *
 * RESPONSIBILITY: locate, read and decode the game's own files.  BRString.dll
 * is the localised text; this is the loader that turns it into the pointer
 * table BrStrGet indexes.
 */
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
/* The original takes no argument: it calls the three Win32 imports itself.
 * The header's prototype is the port's (a BrStrResOps table). */
#define BrStrResLoad BrStrResLoad_port
#include "br_strres.h"
#undef BrStrResLoad
/* 64-bit core: declared once, by its definition's header */
/* 64-bit core: LoadStringA is declared by the platform headers */
/* 64-bit core: FreeLibrary is declared by the platform headers */
#include <string.h>

#include "slice4_52.h"   /* g_apBrStrTable, BR_STR_TABLE_COUNT -- the table
                          * this module fills.  It is NOT redefined here. */
#include "slice6_78.h"   /* BrChkFReadOpen / BrChkFileSize / BrChkFClose,
                          * D3D 0x10002FE0 / 0x10002F90 / 0x10003290 ==
                          * Glide 0x10003320 / 0x100032D0 / 0x100035E0 */

#include <stdlib.h>

/* ==========================================================================
 * Storage this module owns
 * ========================================================================== */

/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x1186C944 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* 0x1186C948 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* 0x1186C94C */

/* ==========================================================================
 * 0x1006D1BE .. 0x1006D1E1 -- how big is BRString.dll?
 * ==========================================================================
 *
 * Three calls and no error handling of its own: CHK_FReadOpen exits the
 * process if the file is missing, which is why there is nothing to check
 * here.  The handle is the original's two-field {FILE *, char *name} block
 * and is only ever passed straight back out; slice6_78.h explains why its
 * type is spelled `FILE **`. */
/* (port-only br_strres_measure removed) */


/* ==========================================================================
 * 0x1006D1A0 -- load the string resource
 * ========================================================================== */

/* WHAT IT DOES: loads the game's text -- every menu label, message and
 * prompt -- out of the resource module into one block of memory, and builds
 * the table that maps a string number to its text. It clears the table
 * first, and then does nothing at all if the text has already been loaded. */
/* @implements 0x1006D1A0 glide BrStrResLoad */
/* Hand-transcribed from the asm.  The clear is a 0x12E-dword memset from
 * &table[1] (`rep stosd`); the size comes from the CHK_ file helpers; the
 * walk is a plain index loop that VC5 strength-reduces to the table pointer
 * compared against &table[0x12F] (== &g_pBrStrResBlob). */
void BrStrResLoad(void)
{
    FILE **pFile;
    void  *hModule;
    int    id;

    memset(&g_apBrStrTable[1], 0, 0x12E * sizeof g_apBrStrTable[0]);
    if (g_pBrStrResBlob != NULL)
        return;

    pFile = BrChkFReadOpen(BR_STRRES_PATH);
    g_brStrResSize = BrChkFileSize(pFile);
    BrChkFClose(pFile);

    hModule = LoadLibraryA(BR_STRRES_PATH);
    if (hModule == NULL)
        return;

    /* Plain malloc: no diagnostic and no exit on failure. */
    g_pBrStrResBlob = (char *)malloc(g_brStrResSize);
    if (g_pBrStrResBlob != NULL) {
        for (id = 1; id < 0x12F; id++) {
            int n = LoadStringA(hModule, id, g_pBrStrResBlob + g_brStrResUsed,
                                g_brStrResSize - g_brStrResUsed);
            if (n != 0) {
                g_apBrStrTable[id] = g_pBrStrResBlob + g_brStrResUsed;
                g_brStrResUsed = g_brStrResUsed + n + 1;
            }
        }
    }
    FreeLibrary(hModule);
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
/* (port-only BrStrResResetForTest removed) */

