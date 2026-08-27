/* Auto-generated from disassembly — 0x004014A0
 * ResetIncludeStack. */
#ifdef BR_MATCHING_BUILD
/* @implements 0x004014A0 brally.exe ResetIncludeStack */

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
extern int gIncludeDepth;


void ResetIncludeStack(void)
{
    gIncludeDepth = 0;
}


#endif /* BR_MATCHING_BUILD */
