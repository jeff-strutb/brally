/* Auto-generated from disassembly — 0x00401660
 * IncludeStackEmpty: sete al after xor eax,eax. */
#ifdef BR_MATCHING_BUILD
/* @implements 0x00401660 brally.exe IncludeStackEmpty */
/* @n64 0x80268520 located */

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


int IncludeStackEmpty(void)
{
    return gIncludeDepth == 0;
}


#endif /* BR_MATCHING_BUILD */
