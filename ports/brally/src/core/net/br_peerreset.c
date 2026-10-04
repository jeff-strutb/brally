/* br_peerreset.c -- net.
 *
 * Resetting the networking message tables: every peer record and every one of
 * its message slots is guarded by its own mutex, so clearing the tables means
 * taking each record's mutex in turn.
 */
/* The original is /MD: CRT calls go through the import
 * table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include "br_coretypes.h"   /* br_globals: its objects */
#include <stdint.h>

#include <windows.h>

/* BrPeerRec: br_coretypes.h */

/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* WHAT IT DOES: reset the whole networking message system. It clears the two
 * global "something is pending" flags, then walks all 16 peers and, for each,
 * all 16 of that peer's message slots. Every record is touched under its own
 * mutex: the four-word queue header at the tail and the state word are zeroed,
 * and each message slot is additionally stamped with the current tick so the
 * timeout logic restarts from now. */
/* @implements 0x1006A330 glide FUN_1006a330 */

void FUN_1006a330(void)
{
    int i;
    int j;

    DAT_117b3250 = 0;
    DAT_11849e58 = 0;
    for (i = 0; i < 16; i++) {
        WaitForSingleObject(g_aBrPeer71[i].hMutex, 0xffffffff);
        g_aBrPeer71[i].f95C = 0;
        g_aBrPeer71[i].f960 = 0;
        g_aBrPeer71[i].f964 = 0;
        g_aBrPeer71[i].f968 = 0;
        g_aBrPeer71[i].f02C = 0;
        ReleaseMutex(g_aBrPeer71[i].hMutex);
        for (j = 0; j < 16; j++) {
            WaitForSingleObject(g_aBr178FEF8[j][i].hMutex, 0xffffffff);
            g_aBr178FEF8[j][i].f008 = DAT_117b324c;
            g_aBr178FEF8[j][i].f95C = 0;
            g_aBr178FEF8[j][i].f960 = 0;
            g_aBr178FEF8[j][i].f964 = 0;
            g_aBr178FEF8[j][i].f968 = 0;
            g_aBr178FEF8[j][i].f02C = 0;
            ReleaseMutex(g_aBr178FEF8[j][i].hMutex);
        }
    }
}

