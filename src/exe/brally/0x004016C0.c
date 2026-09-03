/* Auto-generated from disassembly — 0x004016C0
 * SetCommentChar. Byte store of a char argument. */
#ifdef BR_MATCHING_BUILD
/* @implements 0x004016C0 brally.exe SetCommentChar */
/* @n64 0x8022F4EC located */

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


void SetCommentChar(char c)
{
    gCommentChar = c;
}


#endif /* BR_MATCHING_BUILD */
