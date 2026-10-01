/* br_netshutdown.c -- net.
 *
 * The teardown counterpart of BrNetMutexInit (0x10005E80): stop the audio
 * thread if it is running, close the DirectPlay session, then close and
 * clear every Win32 mutex handle the network layer created -- the loose
 * ones first, then one per player slot.
 *
 * Every function carries its original address.
 */

/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include "slice1_02.h"   /* br_globals: its objects */
#include "slice2_25.h"   /* br_globals: its objects */
#include <stdint.h>

/* ==========================================================================
 * 0x10005F50
 * ========================================================================== */

/* KERNEL32. dllimport emits `call dword ptr [IAT]`; with ten-plus call sites
 * VC5 hoists the IAT slot into edi once, which is what the original does. */
/* 64-bit core: CloseHandle is declared by the platform headers */

/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x10226A48 -- net mode; >1 means a live session */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* the loose mutexes, in the order the original */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* closes them -- NOT the order MutexInit made    */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* them in (0x10226A34)                           */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* 0x1021C90C */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* 0x1021CE54 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */    /* 0x10226A60 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* slot[0].hMutex; 16 slots of 0x978 bytes */
/* 64-bit core: declared once, in br_globals.h or its struct's header */  /* 0x10273328 -- the DirectPlay context pointer */

/* BrSndThreadStop: prototype in br_funcs.h */
/* 64-bit core: declared once, by its definition's header */    /* 0x10009A40 */

/* WHAT IT DOES: shuts the multiplayer layer down. Stops the sound thread if a
 * networked session was running, tears down the DirectPlay session, then walks
 * every mutex handle the net layer owns -- ten loose ones plus one per player
 * slot -- closing each and writing the handle back to NULL so a later start-up
 * cannot double-close it. Reports success as "the DirectPlay shutdown returned
 * zero", so the caller sees 1 on a clean teardown. */
/* @implements 0x10005F50 glide BrNetShutdown */
int BrNetShutdown(void)
{
    int ok;
    int i;

    if (DAT_10226a48 > 1) {
        BrSndThreadStop();
    }
    /* The `== 0` belongs HERE, not on the return. Written as `return hr == 0;`
     * VC5 keeps the raw result in ebp and spends the epilogue on
     * `xor eax,eax / cmp ebp,ebx / sete`; comparing at the call site gives the
     * original's `neg ebp / sbb ebp,ebp / inc ebp` right after the call and a
     * bare `mov eax,ebp` at the end.  That one move was the whole diff. */
    ok = (BrDPlayShutdown(&g_brP277B40) == 0);

    if (DAT_1021c81c != 0) { CloseHandle((void *)DAT_1021c81c); DAT_1021c81c = 0; }
    if (DAT_1021ce4c != 0) { CloseHandle((void *)DAT_1021ce4c); DAT_1021ce4c = 0; }
    if (g_brH22AF04 != 0) { CloseHandle((void *)g_brH22AF04); g_brH22AF04 = 0; }
    if (g_brH220DDC != 0) { CloseHandle((void *)g_brH220DDC); g_brH220DDC = 0; }
    if (g_brH221324 != 0) { CloseHandle((void *)g_brH221324); g_brH221324 = 0; }
    if (DAT_10226a64 != 0) { CloseHandle((void *)DAT_10226a64); DAT_10226a64 = 0; }
    if (DAT_10226a5c != 0) { CloseHandle((void *)DAT_10226a5c); DAT_10226a5c = 0; }
    if (g_h1022AF30 != 0) { CloseHandle((void *)g_h1022AF30); g_h1022AF30 = 0; }
    if (DAT_10226a58 != 0) { CloseHandle((void *)DAT_10226a58); DAT_10226a58 = 0; }
    if (DAT_10226a54 != 0) { CloseHandle((void *)DAT_10226a54); DAT_10226a54 = 0; }

    for (i = 0; i < 16; i++) {
        if (g_aBrNetSlot[i].hMutex != 0) {
            CloseHandle(g_aBrNetSlot[i].hMutex);
            g_aBrNetSlot[i].hMutex = 0;
        }
    }

    return ok;
}
