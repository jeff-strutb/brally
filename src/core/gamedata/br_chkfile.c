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
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
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
#ifdef BR_MATCHING_BUILD
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
#else
FILE **BrChkFReadOpen(const char *pPath)
{
    BrChkFile *pf;
    size_t     cb;

    /* DEVIATION: the original allocates the literal 8.  On LP64 two pointers
     * do not fit in 8 bytes, so this uses sizeof -- the rule CONVENTIONS
     * states for exactly this shape of allocation.  The `what` strings are
     * the originals, at 0x10094140 and 0x10094128. */
    pf = (BrChkFile *)BrChkAlloc(sizeof(BrChkFile), "CHK_FReadOpen():pfil");

    /* strlen + 1: the original's `repne scasb` count includes the NUL, and
     * the copy that follows moves exactly that many bytes. */
    cb = strlen(pPath) + 1u;
    pf->pszName = (char *)BrChkAlloc(cb, "CHK_FReadOpen():szName");
    memcpy(pf->pszName, pPath, cb);

    /* The trace reads the COPY, not the argument. */
    if (BrChkVerbose != 0) {
        fprintf(stderr, "CHK_FReadOpen(%s)\n", pf->pszName);
    }

    pf->pFile = fopen(pf->pszName, "rb");

    if (pf->pFile == NULL) {
        /* The original opens "c:\RallyError.txt" for writing, puts the
         * message in it, echoes it to the debugger, closes the log and
         * exits(1).  The path is a Windows absolute path and is kept
         * verbatim: on a POSIX host a backslash is an ordinary filename
         * character, so this lands in the working directory instead of on
         * drive C.  That is the faithful reading -- inventing a portable log
         * location would be a behaviour this binary does not have.
         *
         * DEVIATION: a NULL log FILE is skipped.  The original hands it
         * straight to the write and would fault. */
        FILE *pLog = fopen("c:\\RallyError.txt", "w");

        if (pLog != NULL) {
            fprintf(pLog, "CHK_FReadOpen(): error opening file %s.\n",
                    pf->pszName);
            fclose(pLog);
        }
        fprintf(stderr, "CHK_FReadOpen(): error opening file %s.\n",
                pf->pszName);
        exit(1);
    }

    return ChkToPun(pf);
}
#endif

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

/* 0x10003290  CHK_FClose. */
/* WHAT IT DOES: closes a game data file and releases the handle and the copy
 * of the name that went with it. A failed close is treated as fatal and the
 * game quits. */
/* port-only body; Glide match is src/core/generated/0x100035E0.c */
void BrChkFClose(FILE **ppFile)
{
    BrChkFile *pf = ChkFromPun(ppFile);

    if (BrChkVerbose != 0) {
        fprintf(stderr, "CHK_FClose(%s)\n", pf->pszName);
    }

    /* `cmp eax, -1` -- fclose's failure value, which is EOF. */
    if (fclose(pf->pFile) == EOF) {
        fprintf(stderr, "CHK_FClose(): error closing file %s.\n", pf->pszName);
        exit(1);
    }

    /* Order is the original's: the name first, then the handle.  Both reads
     * of pszName above happen before either free. */
    free(pf->pszName);
    free(pf);
}
