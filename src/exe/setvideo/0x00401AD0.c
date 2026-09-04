/* Auto-generated from disassembly — 0x00401AD0
 * PushInclude: gIncludeStack[gIncludeDepth++] = f. */
#ifdef BR_MATCHING_BUILD
/* WHAT IT DOES: push a file onto the include stack when a #include is
 * followed. */
/* @implements 0x00401AD0 setvideo.exe PushInclude */

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
extern void *gIncludeStack[];


void PushInclude(void *f)
{
    gIncludeStack[gIncludeDepth] = f;
    gIncludeDepth++;
}


#endif /* BR_MATCHING_BUILD */
