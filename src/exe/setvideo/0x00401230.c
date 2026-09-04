/* Auto-generated from disassembly — 0x00401230
 * CHK_FWriteOpen(path, mode). Same 8-byte wrapper as FReadOpen. */
#ifdef BR_MATCHING_BUILD
/* WHAT IT DOES: open a file for writing and abort with a message if it
 * cannot be created. */
/* @implements 0x00401230 setvideo.exe CHK_FWriteOpen */

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
void *CHK_AllocateMemory(unsigned int, char *);

typedef struct CHKFile {
    FILE *fp;
    char *name;
} CHKFile;


CHKFile *CHK_FWriteOpen(char *path, char *mode)
{
    char buf[0x400];
    CHKFile *p;

    p = (CHKFile *)CHK_AllocateMemory(8, "CHK_FWriteOpen():pfil");
    p->name = (char *)CHK_AllocateMemory(strlen(path) + 1, "CHK_FReadOpen():szName");
    strcpy(p->name, path);
    if (gChkVerbose != 0) {
        sprintf(buf, "CHK_FWriteOpen(%s)\n", p->name);
        OutputDebugStringA(buf);
    }
    p->fp = fopen(p->name, mode);
    if (p->fp == 0) {
        sprintf(buf, "CHK_FWriteOpen(): error creating file %s.\n", p->name);
        OutputDebugStringA(buf);
        exit(1);
    }
    return p;
}


#endif /* BR_MATCHING_BUILD */
