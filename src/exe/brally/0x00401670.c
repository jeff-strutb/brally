/* Auto-generated from disassembly — 0x00401670
 * PushInclude: gIncludeStack[gIncludeDepth++] = f. */
#ifdef BR_MATCHING_BUILD
/* WHAT IT DOES: push a file onto the include stack when a #include is
 * followed. */
/* @implements 0x00401670 brally.exe PushInclude */
/* @n64 0x80255D20 located */

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
