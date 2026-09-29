/* br_chkfile.c -- gamedata: the CHK_* file helpers.
 *
 * Open, size and close for the game's own data files, over a handle that
 * carries the file's name so a failure can report it. Nothing here recovers:
 * a file that will not open ends the process. Filed out of slice6_78.c
 * sections 2 and 4, which move together because the handle's pun helpers are
 * file-static and only these use them.
 *
 * See slice6_78.h for how the targets were chosen and the defects preserved.
 */
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "slice6_78.h"

/* slice1_01.h -- 0x10003390 CHK_AllocateMemory, and the 0x10220CE0 trace
 * flag both file helpers below read. */
extern void *BrChkAlloc(size_t size, const char *pWhat);
extern int   BrChkVerbose;

/* ==========================================================================
 * 2. The CHK_* file handle
 *
 * 0x10002FE0 allocates EIGHT bytes and uses them as two pointers: the FILE at
 * +0x00 and a private copy of the path at +0x04.  Both 0x10002F90 and
 * 0x10003290 read the second field.
 *
 * BYTE OFFSETS ARE 32-BIT-ONLY: on LP64 the name lands at +8.  The struct is
 * therefore private to this file and the public prototypes keep slice2_20.c's
 * `FILE **`, which is the original's own pun on field 0 and stays valid.
 * Nothing may index the returned pointer.
 * ========================================================================== */

typedef struct BrChkFile {
    FILE *pFile;      /* +0x00 */
    char *pszName;    /* +0x04 in the original */
} BrChkFile;

/* Pointer to a struct and pointer to its first member have the same value and
 * representation, so these two casts are the pun and not a reinterpretation. */
static BrChkFile *ChkFromPun(FILE **ppFile)
{
    return (BrChkFile *)(void *)ppFile;
}

static FILE **ChkToPun(BrChkFile *pf)
{
    return (FILE **)(void *)pf;
}


/* ==========================================================================
 * 4. The CHK_* file helpers
 * ========================================================================== */

/* 0x10002FE0  CHK_FReadOpen.
 *
 * DEVIATION (all three helpers): the original formats into a 0x400-byte stack
 * buffer and ships it to OutputDebugStringA.  Here the message goes straight
 * to stderr -- no fixed buffer, so the %s cases cannot overflow it, which the
 * original could.  slice1_01.c states the same deviation for its five.
 */
/* WHAT IT DOES: opens a game data file for reading and hands back a handle
 * that also remembers the file's name, so later messages can name it. If the
 * file will not open the game writes an error line to a log on the C drive,
 * echoes it, and quits outright -- there is no recovery path here. */
/* @implements 0x10002FE0 d3d BrChkFReadOpen */
__declspec(dllimport) void __stdcall OutputDebugStringA(const char *psz);

FILE **BrChkFReadOpen(const char *pPath)
{
    char       szMsg[0x400];
    BrChkFile *pf;

    pf = (BrChkFile *)BrChkAlloc(sizeof(BrChkFile), "CHK_FReadOpen():pfil");
    pf->pszName = (char *)BrChkAlloc(strlen(pPath) + 1u,
                                     "CHK_FReadOpen():szName");
    strcpy(pf->pszName, pPath);

    if (BrChkVerbose != 0) {
        sprintf(szMsg, "CHK_FReadOpen(%s)\n", pf->pszName);
        OutputDebugStringA(szMsg);
    }

    pf->pFile = fopen(pf->pszName, "rb");

    if (pf->pFile == NULL) {
        FILE *pLog = fopen("c:\\RallyError.txt", "w");

        sprintf(szMsg, "CHK_FReadOpen(): error opening file %s.\n",
                pf->pszName);
        fprintf(pLog, szMsg);
        OutputDebugStringA(szMsg);
        fclose(pLog);
        exit(1);
    }

    /* The pun is written out here, not through ChkToPun: a plain `static`
     * helper is not auto-inlined under /O2 (that needs /Ob2), and the
     * original's tail is one `mov eax,ebx`. */
    return (FILE **)(void *)pf;
}

/* 0x10002F90  CHK_FileSize.
 *
 * Note the round trip: the position is saved, the stream is seeked to the
 * end, measured, and put back.  Callers may therefore size a file they are
 * part-way through reading, and several do.
 *
 * No error is checked anywhere in the original -- a failing ftell returns -1
 * and that -1 is the answer. */
/* WHAT IT DOES: reports how big an open file is, by remembering where the
 * read position was, jumping to the end to measure, and putting the position
 * back. Because it restores the position, callers can measure a file they
 * are part-way through reading, and several do. Nothing checks for failure:
 * a failed measurement simply comes back as -1. */
/* @implements 0x100032D0 glide BrChkFileSize */
int BrChkFileSize(FILE **ppFile)
{
    long pos;
    long size;

    /* Orig re-derefs *ppFile at each CRT call (mov r,[esi]) and CSEs the
     * two IAT slots into edi/ebp. Caching FILE *f = *ppFile folds those. */
    pos = ftell(*ppFile);
    fseek(*ppFile, 0, SEEK_END);
    size = ftell(*ppFile);
    fseek(*ppFile, pos, SEEK_SET);
    return (int)size;
}


/* 0x100034C0  CHK_FRead. */
/* WHAT IT DOES: read from a file and abort the whole game if the read comes
 * up short, printing how many bytes it wanted to the debugger. The engine
 * treats a short read as corrupt game data, so there is no recoverable case;
 * the destination comes back unchanged for chaining. */
int BrFChkFRead(void *pDst, size_t size, size_t count, FILE **ppFile);  /* 0x10003430 */

/* @implements 0x100034C0 glide BrChkFRead */
void *BrChkFRead(void *pDst, size_t size, size_t count, FILE **ppFile)
{
    char szMsg[0x400];

    if (BrFChkFRead(pDst, size, count, ppFile) == 0) {
        sprintf(szMsg, "CHK_FRead(): trying to read %u bytes, but got EOF.\n",
                count * size);
        OutputDebugStringA(szMsg);
        exit(1);
    }
    return pDst;
}

/* 0x100035E0  CHK_FClose (D3D 0x10003290). */
/* WHAT IT DOES: closes a game data file and releases the handle and the copy
 * of the name that went with it. A failed close is treated as fatal and the
 * game quits. */
/* @implements 0x100035E0 glide BrChkFClose */
void BrChkFClose(FILE **ppFile)
{
    BrChkFile *pf = (BrChkFile *)(void *)ppFile;
    char       szMsg[0x400];

    if (BrChkVerbose != 0) {
        sprintf(szMsg, "CHK_FClose(%s)\n", pf->pszName);
        OutputDebugStringA(szMsg);
    }
    if (fclose(pf->pFile) == -1) {
        sprintf(szMsg, "CHK_FClose(): error closing file %s.\n", pf->pszName);
        OutputDebugStringA(szMsg);
        exit(1);
    }
    free(pf->pszName);
    free(pf);
}

/* 0x10003680  CHK_FileExists. */
/* WHAT IT DOES: reports whether a file can be opened for reading, and when
 * file tracing is on also prints CHK_FileExists(path) to the debugger. */
/* @implements 0x10003680 glide BrChkFileExists */
int BrChkFileExists(const char *pPath)
{
    FILE *pFile;
    char  szMsg[0x400];

    if (BrChkVerbose != 0) {
        sprintf(szMsg, "CHK_FileExists(%s)\n", pPath);
        OutputDebugStringA(szMsg);
    }
    pFile = fopen(pPath, "rb");
    if (pFile == NULL) {
        return 0;
    }
    fclose(pFile);
    return 1;
}

/* 0x100036F0  CHK_AllocateMemory. */
/* WHAT IT DOES: allocate memory or abort the game, naming the allocation in
 * the out-of-memory message. A zero-byte request returns null rather than
 * aborting. */
/* @implements 0x100036F0 glide BrChkAlloc */
void *BrChkAlloc(size_t size, const char *pWhat)
{
    void *p;
    char  szMsg[0x400];

    if (size == 0) {
        return NULL;
    }
    p = malloc(size);
    if (p == NULL) {
        sprintf(szMsg, "CHK_AllocateMemory(): Out of memory: couldn't allocate %s\n",
                pWhat);
        OutputDebugStringA(szMsg);
        exit(1);
    }
    return p;
}

/* 0x10003760  CHK_ReAllocateMemory. */
/* WHAT IT DOES: grow or shrink an allocation or abort the game, naming the
 * allocation. GOTCHA: it calls realloc BEFORE testing for a zero size, so a
 * zero-size request still reallocates and then returns null, leaking
 * whatever realloc handed back. That is the original's behaviour, kept. */
/* @implements 0x10003760 glide BrChkRealloc */
void *BrChkRealloc(void *pMem, size_t size, const char *pWhat)
{
    void *p;
    char  szMsg[0x400];

    p = realloc(pMem, size);
    if (size == 0) {
        return NULL;
    }
    if (p == NULL) {
        sprintf(szMsg,
                "CHK_ReAllocateMemory(): Out of memory: couldn't reallocate %s\n",
                pWhat);
        OutputDebugStringA(szMsg);
        exit(1);
    }
    return p;
}

extern char s_File__s_missing_100aa318[];   /* "File %s missing" */
void BrLogPrint(const void *p);

/* WHAT IT DOES: read a whole file into a caller-supplied buffer: warns to
 * the log if it is missing, opens it, reads either the requested number of
 * bytes or the entire file when a negative length is passed, and closes it.
 * Every step uses the abort-on-failure file helpers above, so a real failure
 * ends the game rather than returning. */
/* @implements 0x10030F50 glide BrFileReadInto */
void BrFileReadInto(void *pDst, const char *pPath, int cb)
{
  FILE **ppFile;
  char szMsg[512];

  if (BrChkFileExists(pPath) == 0) {
    sprintf(szMsg, s_File__s_missing_100aa318, pPath);
    BrLogPrint(szMsg);
  }
  ppFile = BrChkFReadOpen(pPath);
  if (cb < 0) {
    cb = BrChkFileSize(ppFile);
  }
  BrChkFRead(pDst, 1, cb, ppFile);
  BrChkFClose(ppFile);
}
