/* Auto-generated from disassembly — 0x004016D0
 * ReadINI: alloc 8-byte cursor, temporarily set comment char to ';'. */
#ifdef BR_MATCHING_BUILD
/* @implements 0x004016D0 setvideo.exe ReadINI */

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
typedef struct ObjList {
    int n;
    int *rgi;
    char **rgsz;
} ObjList;
typedef struct INI {
    ObjList *list;
    int index;
} INI;
void *CHK_AllocateMemory(unsigned int, char *);
char GetCommentChar(void);
void SetCommentChar(char);
ObjList *ReadList(char *);


INI *ReadINI(char *path)
{
    INI *p;
    char saved;

    p = (INI *)CHK_AllocateMemory(8, "ReadINI():pini");
    saved = GetCommentChar();
    SetCommentChar(';');
    p->list = ReadList(path);
    SetCommentChar(saved);
    p->index = -1;
    return p;
}


#endif /* BR_MATCHING_BUILD */
