/* Auto-generated from disassembly — 0x00401050
 * CHK_FReadOpen. 8-byte {FILE*, name} wrapper. Error path writes
 * c:\RallyError.txt then OutputDebugStringA + exit(1). */
#ifdef BR_MATCHING_BUILD
/* WHAT IT DOES: open a file for reading and abort with a message if it is
 * not there. The settings tool reads its lists this way so a missing file is
 * loud, not silent. */
/* @implements 0x00401050 setvideo.exe CHK_FReadOpen */

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
void *CHK_AllocateMemory(unsigned int, char *);
int fputs_fp(FILE *, char *);

typedef struct CHKFile {
    FILE *fp;
    char *name;
} CHKFile;


CHKFile *CHK_FReadOpen(char *path)
{
    char buf[0x400];
    CHKFile *p;
    FILE *err;

    p = (CHKFile *)CHK_AllocateMemory(8, "CHK_FReadOpen():pfil");
    p->name = (char *)CHK_AllocateMemory(strlen(path) + 1, "CHK_FReadOpen():szName");
    strcpy(p->name, path);
    if (gChkVerbose != 0) {
        sprintf(buf, "CHK_FReadOpen(%s)\n", p->name);
        OutputDebugStringA(buf);
    }
    p->fp = fopen(p->name, "rb");
    if (p->fp == 0) {
        err = fopen("c:\\RallyError.txt", "w");
        sprintf(buf, "CHK_FReadOpen(): error opening file %s.\n", p->name);
        /* FILE* first: orig push buf, push fp → cdecl (fp, buf). */
        fputs_fp(err, buf);
        OutputDebugStringA(buf);
        fclose(err);
        exit(1);
    }
    return p;
}


#endif /* BR_MATCHING_BUILD */
