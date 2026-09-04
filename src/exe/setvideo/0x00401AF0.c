/* Auto-generated from disassembly — 0x00401AF0
 * PopInclude: return gIncludeStack[--gIncludeDepth]. */
#ifdef BR_MATCHING_BUILD
/* WHAT IT DOES: pop the previous file off the include stack when an included
 * file ends. */
/* @implements 0x00401AF0 setvideo.exe PopInclude */
/* @n64 0x80211934 located */

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


void *PopInclude(void)
{
    gIncludeDepth--;
    return gIncludeStack[gIncludeDepth];
}


#endif /* BR_MATCHING_BUILD */
