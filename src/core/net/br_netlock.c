/* br_netlock.c -- net.
 *
 * One mutex-guarded flag of the network layer, and the mutex that guards it.
 *
 * Filed out of the address batches: these functions were
 * matched first and grouped by what they are afterwards.
 * Every function carries its original address.
 */

#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include <stdint.h>

/* ==========================================================================
 * 0x10004C20
 * ========================================================================== */

#ifdef BR_MATCHING_BUILD
/* KERNEL32. dllimport emits `call dword ptr [IAT]` rather than a thunk.
 * Timeout is `(unsigned long)-1` so the push is `6A FF` (INFINITE). */
__declspec(dllimport) unsigned long __stdcall WaitForSingleObject(
    void *hHandle, unsigned long dwMilliseconds);
__declspec(dllimport) int __stdcall ReleaseMutex(void *hMutex);

void    *g_brH220DDC;  /* 0x10220DDC -- mutex that guards g_br221314 */
int32_t  g_br221314;   /* 0x10221314 */

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
#endif

/* ==========================================================================
 * 0x10005D90 -- the free-slot stack
 * ========================================================================== */

/* DEVIATION -- slice1_02.h. The port routes the identical
 * WaitForSingleObject(h, INFINITE) / ReleaseMutex(h) pattern through these
 * two hooks; the matching build reaches KERNEL32 directly, as the original
 * does. */
#ifndef BR_MATCHING_BUILD
extern void BrNetMutexLock(void *hMutex);
extern void BrNetMutexUnlock(void *hMutex);
#endif

#ifdef BR_MATCHING_BUILD
__declspec(dllimport) unsigned long __stdcall WaitForSingleObject(void *, unsigned long);
__declspec(dllimport) int __stdcall ReleaseMutex(void *);

extern void    *g_h1022AF30;
extern int32_t  g_a10221288[];
extern int32_t  g_i10221318;
#else
void    *g_h1022AF30;
int32_t  g_a10221288[16];
int32_t  g_i10221318;
#endif

/* WHAT IT DOES: pops the top free slot number from 0x10221288 under mutex. */
/* @implements 0x10005D90 d3d BrNetStackPop221288 */
int32_t BrNetStackPop221288(void)
{
    int32_t v;

#ifdef BR_MATCHING_BUILD
    WaitForSingleObject(g_h1022AF30, (unsigned long)-1);
#else
    BrNetMutexLock(g_h1022AF30);
#endif
    if (g_i10221318 >= 0) {
        v = g_a10221288[g_i10221318];
        g_i10221318 = g_i10221318 - 1;
    } else {
        v = -1;
    }
#ifdef BR_MATCHING_BUILD
    ReleaseMutex(g_h1022AF30);
#else
    BrNetMutexUnlock(g_h1022AF30);
#endif
    return v;
}
