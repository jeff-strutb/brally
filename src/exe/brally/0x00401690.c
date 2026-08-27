/* Auto-generated from disassembly — 0x00401690
 * PopInclude: return gIncludeStack[--gIncludeDepth]. */
#ifdef BR_MATCHING_BUILD
/* @implements 0x00401690 brally.exe PopInclude */

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
extern void *gIncludeStack[];


void *PopInclude(void)
{
    gIncludeDepth--;
    return gIncludeStack[gIncludeDepth];
}


#endif /* BR_MATCHING_BUILD */
