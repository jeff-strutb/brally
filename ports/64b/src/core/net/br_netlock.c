/* br_netlock.c -- net.
 *
 * One mutex-guarded flag of the network layer, and the mutex that guards it.
 *
 * Filed out of the address batches: these functions were
 * matched first and grouped by what they are afterwards.
 * Every function carries its original address.
 */

/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include <stdint.h>

/* ==========================================================================
 * 0x10004C20
 * ========================================================================== */

/* KERNEL32. dllimport emits `call dword ptr [IAT]` rather than a thunk.
 * Timeout is `(unsigned long)-1` so the push is `6A FF` (INFINITE). */
/* 64-bit core: declared once, by its definition's header */
/* 64-bit core: ReleaseMutex is declared by the platform headers */

/* 64-bit core: declared once, in br_globals.h or its struct's header */  /* 0x10220DDC -- mutex that guards g_br221314 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x10221314 */

/* WHAT IT DOES: takes the network mutex at 0x10220DDC, turns the flag at
 * 0x10221314 on if it is currently off, then releases the mutex. Always
 * returns 1; the previous value of the flag is thrown away. */
/* @implements 0x10004C20 d3d BrNetLockSetIfZero221314 */
int32_t BrNetLockSetIfZero221314(void)
{
    WaitForSingleObject(g_brH220DDC, (unsigned long)-1);
    if (g_br221314 == 0) {
        g_br221314 = 1;
    }
    ReleaseMutex(g_brH220DDC);
    return 1;
}

/* ==========================================================================
 * 0x10005D90 -- the free-slot stack
 * ========================================================================== */

/* DEVIATION -- slice1_02.h. The port routes the identical
 * WaitForSingleObject(h, INFINITE) / ReleaseMutex(h) pattern through these
 * two hooks; the matching build reaches KERNEL32 directly, as the original
 * does. */

/* 64-bit core: WaitForSingleObject is declared by the platform headers */
/* 64-bit core: ReleaseMutex is declared by the platform headers */

/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* WHAT IT DOES: pops the top free slot number from 0x10221288 under mutex. */
/* @implements 0x10005D90 d3d BrNetStackPop221288 */
int32_t BrNetStackPop221288(void)
{
    int32_t v;

    WaitForSingleObject(g_h1022AF30, (unsigned long)-1);
    if (g_i10221318 >= 0) {
        v = g_a10221288[g_i10221318];
        g_i10221318 = g_i10221318 - 1;
    } else {
        v = -1;
    }
    ReleaseMutex(g_h1022AF30);
    return v;
}
