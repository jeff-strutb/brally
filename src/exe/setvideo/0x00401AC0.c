/* Auto-generated from disassembly — 0x00401AC0
 * IncludeStackEmpty: sete al after xor eax,eax. */
#ifdef BR_MATCHING_BUILD
/* @implements 0x00401AC0 setvideo.exe IncludeStackEmpty */
/* @n64 0x80268530 located */

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


int IncludeStackEmpty(void)
{
    return gIncludeDepth == 0;
}


#endif /* BR_MATCHING_BUILD */
