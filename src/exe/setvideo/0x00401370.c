/* Auto-generated from disassembly — 0x00401370
 * CHK_FClose. fclose; on EOF debug+exit; then free name and wrapper. */
#ifdef BR_MATCHING_BUILD
/* WHAT IT DOES: close a checked file and free the handle record and its copy
 * of the name. */
/* @implements 0x00401370 setvideo.exe CHK_FClose */

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

extern int gChkVerbose;

typedef struct CHKFile {
    FILE *fp;
    char *name;
} CHKFile;


void CHK_FClose(CHKFile *p)
{
    char buf[0x400];

    if (gChkVerbose != 0) {
        sprintf(buf, "CHK_FClose(%s)\n", p->name);
        OutputDebugStringA(buf);
    }
    if (fclose(p->fp) == -1) {
        sprintf(buf, "CHK_FClose(): error closing file %s.\n", p->name);
        OutputDebugStringA(buf);
        exit(1);
    }
    free(p->name);
    free(p);
}


#endif /* BR_MATCHING_BUILD */
