/* Auto-generated from disassembly — 0x00401910
 * ResetIncludeStack. */
#ifdef BR_MATCHING_BUILD
/* WHAT IT DOES: empty the include stack, so a fresh list read starts with no
 * nesting. */
/* @implements 0x00401910 setvideo.exe ResetIncludeStack */

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
extern int gIncludeDepth;


void ResetIncludeStack(void)
{
    gIncludeDepth = 0;
}


#endif /* BR_MATCHING_BUILD */
