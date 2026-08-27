/* Auto-generated from disassembly — 0x004016B0
 * GetCommentChar. Default global is '#' (0x23 at 0x40308c). */
#ifdef BR_MATCHING_BUILD
/* @implements 0x004016B0 brally.exe GetCommentChar */

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
extern char gCommentChar;


char GetCommentChar(void)
{
    return gCommentChar;
}


#endif /* BR_MATCHING_BUILD */
