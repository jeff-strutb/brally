/* br_fatal.c -- startup: report a fatal condition and die.
 *
 * Filed out of the address batches slice4_52.c (0x10008EC0) and slice3_39.c
 * (0x100590A0); slice3_33.h is the header the first batch reached
 * BrOperatorNew through, and the second declared its two callees itself.
 */
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)


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
#if defined(BR_FATAL_LOG)
    /* Compiled out of the byte-exact build (the T4 image gate grades this
     * function against the original); the T3 play image's force-annex
     * compile defines BR_FATAL_LOG (tools/brally/image_build_t3.py, mode 'log'). */
    {   /* DIAGNOSTIC (permanent): the original formats this fatal message
         * and discards it, so a clean exit(1) leaves no trace of WHY.  Append
         * it to a log.  Strings are built on the stack (no new .rdata, which
         * the fixed image has no room for) and only already-imported CRT
         * calls are used.  The body is spilled into the annex by the image
         * builder (config/brally/force_annex.csv) so growing it past its slot is
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
#endif
    exit(1);
}


/* ---- from slice3_39.c ---------------------------------------------- */

/* 64-bit core: MessageBoxA is declared by the platform headers */
/* BrStrGet: prototype in br_funcs.h */

/* WHAT IT DOES: MessageBox the given text with string-table entry 0xAA as
 * the caption; the middle argument is never read. */
/* @implements 0x100590A0 glide BrMsgBoxAA */
void BrMsgBoxAA(void *hWnd, int unused, const char *pText)
{
    MessageBoxA(hWnd, pText, BrStrGet(0xaa), 0);
}


/* 0x100ABE00: the Glide copy of the nine-entry error table (BrErrEnt in
 * slice1_06.h; the D3D twin of this function is 0x1003E260). */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* WHAT IT DOES: show one of the game's numbered error messages in a Windows
 * message box, looking the text up in the string table so it appears in the
 * player's language (its first character is skipped), and quitting the game
 * afterwards if that error is marked fatal.  Only the upper bound is tested:
 * a negative number reads before the table, as in the original. */
/* @implements 0x100378C0 glide FUN_100378c0 */
void FUN_100378c0(int iErr)
{
  const char *pText;

  if (iErr <= 8) {
    pText = BrStrGet(DAT_100abe00[iErr].idText);
    MessageBoxA((void *)g_brOwner5BC72C, pText + 1, BrStrGet(0xaa), 0);
    if (DAT_100abe00[iErr].fFatal != 0) {
      exit(1);
    }
  }
}
