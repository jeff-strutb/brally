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
    {   /* DIAGNOSTIC (permanent): the original formats this fatal message
         * and discards it, so a clean exit(1) leaves no trace of WHY.  Append
         * it to a log.  Strings are built on the stack (no new .rdata, which
         * the fixed image has no room for) and only already-imported CRT
         * calls are used.  The body is spilled into the annex by the image
         * builder (config/force_annex.csv) so growing it past its slot is
         * fine. */
        char  nm[12];
        char  md[2];
        void *fp;
        int   n;
        nm[0]='b'; nm[1]='r'; nm[2]='a'; nm[3]='l'; nm[4]='l'; nm[5]='y';
        nm[6]='.'; nm[7]='l'; nm[8]='o'; nm[9]='g'; nm[10]=0;
        md[0]='a'; md[1]=0;
        fp = fopen(nm, md);
        if (fp != (void *)0) {
            for (n = 0; pBuf[n] != 0; n++) { }
            pBuf[n] = '\n';
            fwrite(pBuf, 1, (size_t)(n + 1), fp);
            fclose(fp);
        }
    }
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
