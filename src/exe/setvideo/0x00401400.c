/* Auto-generated from disassembly — 0x00401400
 * CHK_FileExists. Verbose path uses OutputDebugStringA, not fprintf. */
#ifdef BR_MATCHING_BUILD
/* @implements 0x00401400 setvideo.exe CHK_FileExists */

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
