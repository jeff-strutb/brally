/* Auto-generated from disassembly — 0x00401130
 * SetSubstituteDir: alloc 8-byte section cursor, strcmp-scan for name. */
#ifdef BR_MATCHING_BUILD
/* @implements 0x00401130 brally.exe SetSubstituteDir */

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
typedef struct Section {
    INI *pini;
    int index;
} Section;
void *CHK_AllocateMemory(unsigned int, char *);


Section *SetSubstituteDir(INI *pini, char *name)
{
    Section *p;
    int i;
    ObjList *list;

    p = (Section *)CHK_AllocateMemory(8, "SetSubstituteDir():pins");
    p->pini = pini;
    p->index = -1;
    if (pini != 0) {
        list = pini->list;
        for (i = 0; i < list->n; i++) {
            if (strcmp(list->rgsz[i], name) == 0) {
                p->index = i;
                break;
            }
        }
    }
    return p;
}


#endif /* BR_MATCHING_BUILD */
