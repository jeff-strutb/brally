/* Auto-generated from disassembly — 0x00401670
 * PushInclude: gIncludeStack[gIncludeDepth++] = f. */
#ifdef BR_MATCHING_BUILD
/* @implements 0x00401670 brally.exe PushInclude */

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


void PushInclude(void *f)
{
    gIncludeStack[gIncludeDepth] = f;
    gIncludeDepth++;
}


#endif /* BR_MATCHING_BUILD */
