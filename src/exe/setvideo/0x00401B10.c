/* Auto-generated from disassembly — 0x00401B10
 * GetCommentChar. Default global is '#' (0x23 at 0x40308c). */
#ifdef BR_MATCHING_BUILD
/* @implements 0x00401B10 setvideo.exe GetCommentChar */

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
extern char gCommentChar;


char GetCommentChar(void)
{
    return gCommentChar;
}


#endif /* BR_MATCHING_BUILD */
