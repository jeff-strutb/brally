/* Auto-generated from disassembly — 0x00401250
 * ReadINI: alloc 8-byte cursor, temporarily set comment char to ';'. */
#ifdef BR_MATCHING_BUILD
/* @implements 0x00401250 brally.exe ReadINI */

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
