/* Auto-generated from disassembly — 0x00401050
 * CHK_FileExists. Verbose path uses OutputDebugStringA, not fprintf. */
#ifdef BR_MATCHING_BUILD
/* WHAT IT DOES: test whether a file exists, optionally reporting the check
 * to the debugger. The launcher's guard before it tries to open anything. */
/* @implements 0x00401050 brally.exe CHK_FileExists */

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
extern int gChkVerbose;


int CHK_FileExists(char *path)
{
    char buf[0x400];
    FILE *f;

    if (gChkVerbose != 0) {
        sprintf(buf, "CHK_FileExists(%s)\n", path);
        OutputDebugStringA(buf);
    }
    f = fopen(path, "rb");
    if (f == 0)
        return 0;
    fclose(f);
    return 1;
}


#endif /* BR_MATCHING_BUILD */
