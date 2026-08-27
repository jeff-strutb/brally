/* Auto-generated from disassembly — 0x00401200
 * BindSection: copy section index onto the INI cursor, return the INI.
 * Re-deref p->pini for the return (do not reuse the loaded pointer). */
#ifdef BR_MATCHING_BUILD
/* @implements 0x00401200 brally.exe BindSection */

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


INI *BindSection(Section *p)
{
    if (p != 0) {
        if (p->pini != 0) {
            if (p->index != -1) {
                p->pini->index = p->index;
                return p->pini;
            }
        }
    }
    return 0;
}


#endif /* BR_MATCHING_BUILD */
