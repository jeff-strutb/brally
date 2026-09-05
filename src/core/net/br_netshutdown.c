/* br_netshutdown.c -- net.
 *
 * The teardown counterpart of BrNetMutexInit (0x10005E80): stop the audio
 * thread if it is running, close the DirectPlay session, then close and
 * clear every Win32 mutex handle the network layer created -- the loose
 * ones first, then one per player slot.
 *
 * Every function carries its original address.
 */

#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include <stdint.h>

/* ==========================================================================
 * 0x10005F50
 * ========================================================================== */

#ifdef BR_MATCHING_BUILD
/* KERNEL32. dllimport emits `call dword ptr [IAT]`; with ten-plus call sites
 * VC5 hoists the IAT slot into edi once, which is what the original does. */
__declspec(dllimport) int __stdcall CloseHandle(void *hObject);

extern int DAT_10226a48;   /* 0x10226A48 -- net mode; >1 means a live session */
extern int DAT_1021c81c;   /* the loose mutexes, in the order the original */
extern int DAT_1021ce4c;   /* closes them -- NOT the order MutexInit made    */
extern int g_brH22AF04;    /* them in (0x10226A34)                           */
extern int g_brH220DDC;    /* 0x1021C90C */
extern int g_brH221324;    /* 0x1021CE54 */
extern int DAT_10226a64;
extern int DAT_10226a5c;
extern int g_h1022AF30;    /* 0x10226A60 */
extern int DAT_10226a58;
extern int DAT_10226a54;
extern int DAT_1021ce58;   /* slot[0].hMutex; 16 slots of 0x978 bytes */
extern void *g_brP277B40;  /* 0x10273328 -- the DirectPlay context pointer */

extern void BrSndThreadStop(void);          /* 0x1006B1E0 */
extern int  BrDPlayShutdown(void *pCtx);    /* 0x10009A40 */

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
    int *p;

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

    /* Same walk as BrNetMutexInit's: the slot pointer is the loop variable and
     * the bound is the literal end address, so the test is `cmp esi, END`. */
    p = &DAT_1021ce58;
    do {
        if (*p != 0) {
            CloseHandle((void *)*p);
            *p = 0;
        }
        p += 0x25e;
    } while ((int)p < 0x102265d8);

    return ok;
}
#endif
