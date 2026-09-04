/* Auto-generated from disassembly — 0x00401AE0
 * GetIniValue: walk section lines, split on '=', _stricmp the key.
 * Idiom: for (line = NextObj(); line; line = NextObj()) — not do-while.
 * do-while merges loop-exit `return 0` with the BindSection-fail xor
 * epilogue and emits `je fail; jmp loop` instead of orig `jne loop`. */
#ifdef BR_MATCHING_BUILD
/* WHAT IT DOES: read one named value out of a section of the settings file,
 * returning nothing if either the section or the key is absent. */
/* @implements 0x00401AE0 brally.exe GetIniValue */

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
INI *BindSection(Section *);
char *NextObj(INI *);
char *GetObj(Section *);
extern char gLineBuf[];


char *GetIniValue(Section *psec, char *key)
{
    INI *pini;
    char *line;
    char *eq;

    pini = BindSection(psec);
    if (pini != 0) {
        for (line = NextObj(pini); line != 0; line = NextObj(pini)) {
            strcpy(gLineBuf, line);
            eq = strchr(gLineBuf, '=');
            if (eq != 0) {
                *eq = 0;
                if (_stricmp(gLineBuf, key) == 0)
                    return eq + 1;
            } else {
                printf("Unable to parse %s in section %s.\n", gLineBuf, GetObj(psec));
                exit(1);
            }
        }
    }
    return 0;
}


#endif /* BR_MATCHING_BUILD */
