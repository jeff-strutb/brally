/* Auto-generated from disassembly — 0x004011C0
 * CHK_FreeMemory: free wrapper, cdecl add esp,4. */
#ifdef BR_MATCHING_BUILD
/* @implements 0x004011C0 brally.exe CHK_FreeMemory */

#define _CRTIMP __declspec(dllimport)
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifndef true
#define true 1
#define false 0
#endif

typedef int (*funcptr)();

/* Forward declarations for unknown functions/globals */


void CHK_FreeMemory(void *p)
{
    free(p);
}


#endif /* BR_MATCHING_BUILD */
