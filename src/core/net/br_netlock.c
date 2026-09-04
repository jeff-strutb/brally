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
