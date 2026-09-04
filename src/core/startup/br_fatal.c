/* br_fatal.c -- startup: report a fatal condition and die.
 *
 * Filed out of the address batches slice4_52.c (0x10008EC0) and slice3_39.c
 * (0x100590A0); slice3_33.h is the header the first batch reached
 * BrOperatorNew through, and the second declared its two callees itself.
 */
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif

#ifdef BR_MATCHING_BUILD

#include "slice3_33.h"      /* BrOperatorNew (0x1007DFE0) */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

/* WHAT IT DOES: formats a fatal message into a fresh 0x400-byte buffer and
 * exits with code 1.  The buffer is never printed or freed -- the original
 * really does allocate, format, and die. */
/* @implements 0x10008EC0 glide BrLogFatalPrintf */
void BrLogFatalPrintf(const char *pFmt, ...)
{
    va_list ap;
    char   *pBuf;

    pBuf = (char *)BrOperatorNew(0x400);
    va_start(ap, pFmt);
    vsprintf(pBuf, pFmt, ap);
    exit(1);
}


/* ---- from slice3_39.c ---------------------------------------------- */

__declspec(dllimport) int __stdcall MessageBoxA(void *hWnd, const char *pText,
                                                const char *pCaption,
                                                unsigned int uType);
const char *BrStrGet(int id);

/* WHAT IT DOES: MessageBox the given text with string-table entry 0xAA as
 * the caption; the middle argument is never read. */
/* @implements 0x100590A0 glide BrMsgBoxAA */
void BrMsgBoxAA(void *hWnd, int unused, const char *pText)
{
    MessageBoxA(hWnd, pText, BrStrGet(0xaa), 0);
}

#endif /* BR_MATCHING_BUILD */
