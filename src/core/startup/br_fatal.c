/* br_fatal.c -- startup: die with a formatted message.
 *
 * Filed out of the address batch slice4_52.c; slice3_33.h is the header that
 * batch reached BrOperatorNew through.
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

#endif /* BR_MATCHING_BUILD */
