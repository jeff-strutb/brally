/* Auto-generated from disassembly — 0x00401600
 * CHK_FreeMemory: free wrapper, cdecl add esp,4. */
#ifdef BR_MATCHING_BUILD
/* @implements 0x00401600 setvideo.exe CHK_FreeMemory */

/* SetVideo.exe is /ML (static CRT): CRT calls are E8, not FF 15. */
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
