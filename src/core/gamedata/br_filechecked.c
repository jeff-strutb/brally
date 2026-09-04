/* br_filechecked.c -- gamedata: the BrFile*Checked helpers.
 *
 * Not to be confused with br_chkfile.c, which holds the CHK_* family over a
 * named-file handle; these four are the plain __stdcall open/read/write pair
 * that abort on failure.
 *
 * Open, read and write that abort the game with a message instead of
 * returning a failure, so callers of the game's own file readers never test a
 * return value. Filed out of slice2_13.c section 1.
 *
 * See slice2_13.h for the identification notes.
 */
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef BR_MATCHING_BUILD
/* Header prototype is cdecl; the original is __stdcall. */
#define BrFileWriteChecked BrFileWriteChecked_cdecl
#endif
#include "slice2_13.h"
#ifdef BR_MATCHING_BUILD
#undef BrFileWriteChecked
#endif

/* 0x10008CC0 -- the printf-style error reporter both file helpers call.
 * It is in no packet in this slice.
 * XSLICE 0x10008CC0 */
extern void BrErrorf(const char *pszFmt, ...);

#ifdef BR_MATCHING_BUILD
/* Both checked-IO twins are __stdcall (ret 0xC) and both report through the
 * fatal printf at 0x10008EC0 with the READ string -- the fwrite one too. */
extern void BrLogFatalPrintf(const char *pFmt, ...);

/* The checked-open twins fatal-report with strerror; the errno read is
 * the CRT's _errno() CALL through the import table (FF 15), not a
 * variable load. */
_CRTIMP int *__cdecl _errno(void);

/* WHAT IT DOES: open a file for WRITING, and abort the game with a message
 * naming the file and the C-library reason if it cannot be created. The
 * "Checked" family exists so callers never test a return value: anything that
 * fails here is a broken install or a full disk, not a recoverable case. */
/* @implements 0x10008DC0 glide BrFileCreateChecked */
FILE *__stdcall BrFileCreateChecked(char *pszPath)
{
    FILE *pFile;

    pFile = fopen(pszPath, "wb");
    if (pFile == 0) {
        BrLogFatalPrintf("Error opening %s: %s", pszPath,
                         strerror(*_errno()));
    }
    return pFile;
}

/* WHAT IT DOES: the read-only twin of BrFileCreateChecked -- open an existing
 * file for READING, or abort naming the file and the reason. */
/* @implements 0x10008E10 glide BrFileOpenChecked */
FILE *__stdcall BrFileOpenChecked(char *pszPath)
{
    FILE *pFile;

    pFile = fopen(pszPath, "rb");
    if (pFile == 0) {
        BrLogFatalPrintf("Error opening %s: %s", pszPath,
                         strerror(*_errno()));
    }
    return pFile;
}

/* WHAT IT DOES: read exactly cbData bytes, aborting if the file was short.
 * A truncated read means corrupt game data, so there is nothing to recover. */
/* @implements 0x10008E60 glide BrFileReadChecked */
void __stdcall BrFileReadChecked(FILE *pFile, void *pvData, unsigned int cbData)
{
    if (fread(pvData, 1, cbData, pFile) != cbData) {
        BrLogFatalPrintf("File read failure");
    }
}

/* WHAT IT DOES: write exactly cbData bytes, aborting if the write was short.
 * Its failure message says "File read failure" -- that typo is in the shipped
 * game and is reproduced deliberately; do not fix it. */
/* @implements 0x10008E90 glide BrFileWriteChecked */
void __stdcall BrFileWriteChecked(FILE *pFile, const void *pvData,
                                  unsigned int cbData)
{
    if (fwrite(pvData, 1, cbData, pFile) != cbData) {
        BrLogFatalPrintf("File read failure");   /* sic -- an fwrite */
    }
}
#else
void BrFileWriteChecked(FILE *pFile, const void *pvData, uint32_t cbData)
{
    if (fwrite(pvData, 1, cbData, pFile) != (size_t)cbData)
        BrErrorf("File read failure");   /* sic -- this is an fwrite */
}
#endif
