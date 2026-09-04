/* Auto-generated from disassembly — 0x00401220
 * NextObj: advance INI cursor; stop on EOF or a '[' section header. */
#ifdef BR_MATCHING_BUILD
/* WHAT IT DOES: step a settings-file cursor to the next entry, stopping at
 * end of file or at the '[' that begins the next section. */
/* @implements 0x00401220 brally.exe NextObj */

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


char *NextObj(INI *p)
{
    int i;
    char *s;

    if (p == 0)
        return 0;
    i = p->index;
    if (i < 0)
        return 0;
    i = i + 1;
    p->index = i;
    if (i >= p->list->n) {
        p->index = -1;
        return 0;
    }
    s = p->list->rgsz[i];
    if (s[0] == '[') {
        p->index = -1;
        return 0;
    }
    return s;
}


#endif /* BR_MATCHING_BUILD */
