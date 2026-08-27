/* Auto-generated from disassembly — 0x004012C0
 * ReadList: two-pass (count, then fill rgsz/rgi). Opens "rt". */
#ifdef BR_MATCHING_BUILD
/* @implements 0x004012C0 brally.exe ReadList */

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
void ResetIncludeStack(void);
FILE *ReadListLine(char *, int, FILE *);
void *CHK_AllocateMemory(unsigned int, char *);
extern char s_rt[];
extern char s_ReadListOpenErr[];
extern char s_PRJ[];
extern char s_rgiObj[];
extern char s_rgszObj[];
extern char s_szObji[];


ObjList *ReadList(char *path)
{
    char buf[0x8000];
    FILE *fp;
    FILE *cur;
    int n;
    int i;
    ObjList *p;
    char *nl;

    n = 0;
    ResetIncludeStack();
    fp = fopen(path, s_rt);
    if (fp == 0) {
        printf(s_ReadListOpenErr, path);
        exit(1);
    }
    cur = ReadListLine(buf, 0x7fff, fp);
    while (cur != 0) {
        fp = cur;
        if (strlen(buf) > 1)
            n++;
        cur = ReadListLine(buf, 0x7fff, cur);
    }
    fclose(fp);

    p = (ObjList *)CHK_AllocateMemory(0xc, s_PRJ);
    p->n = n;
    p->rgi = (int *)CHK_AllocateMemory(n * 4, s_rgiObj);
    p->rgsz = (char **)CHK_AllocateMemory(p->n * 4, s_rgszObj);

    fp = fopen(path, s_rt);
    if (fp == 0) {
        printf(s_ReadListOpenErr, path);
        exit(1);
    }
    i = 0;
    if (p->n > 0) {
        do {
            cur = ReadListLine(buf, 0x7fff, fp);
            fp = cur;
            if (strlen(buf) > 1) {
                nl = strrchr(buf, '\n');
                if (nl != 0)
                    *nl = 0;
                p->rgsz[i] = (char *)CHK_AllocateMemory(strlen(buf) + 1, s_szObji);
                strcpy(p->rgsz[i], buf);
                p->rgi[i] = i;
                i++;
            }
        } while (i < p->n);
    }
    fclose(fp);
    return p;
}


#endif /* BR_MATCHING_BUILD */
