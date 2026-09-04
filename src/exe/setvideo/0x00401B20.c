/* Auto-generated from disassembly — 0x00401B20
 * SetCommentChar. Byte store of a char argument. */
#ifdef BR_MATCHING_BUILD
/* WHAT IT DOES: change the character that starts a comment, so a settings
 * file can use ';' while a list file uses '#'. */
/* @implements 0x00401B20 setvideo.exe SetCommentChar */

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


void SetCommentChar(char c)
{
    gCommentChar = c;
}


#endif /* BR_MATCHING_BUILD */
