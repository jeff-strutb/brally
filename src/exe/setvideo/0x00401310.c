/* Auto-generated from disassembly — 0x00401310
 * CHK_FPutS(str, CHKFile *). fputs; on EOF debug+exit. */
#ifdef BR_MATCHING_BUILD
/* @implements 0x00401310 setvideo.exe CHK_FPutS */

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

typedef struct CHKFile {
    FILE *fp;
    char *name;
} CHKFile;


void CHK_FPutS(char *s, CHKFile *p)
{
    char buf[0x400];

    if (fputs(s, p->fp) == -1) {
        sprintf(buf, "CHK_FPutS(): error writing file %s.\n", p->name);
        OutputDebugStringA(buf);
        exit(1);
    }
}


#endif /* BR_MATCHING_BUILD */
