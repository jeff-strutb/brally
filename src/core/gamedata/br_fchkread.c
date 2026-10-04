/* br_fchkread.c -- gamedata: FCHK_FRead, the checked read under every data
 * file the game loads.
 *
 * Filed out of the address batch slice1_01.c; the CHK_* allocation and
 * existence helpers stay there as port-only bodies (their Glide matches are
 * in src/core/generated/). The preamble is slice1_01.c's, carried whole.
 */

/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#include "slice1_01.h"

#include <stdlib.h>
#include <stdio.h>

/* 0x100030E0  FCHK_FRead.
 *
 * GOTCHA: the fourth argument is a FILE **. The original does
 * `mov ecx, [eax]` on it before pushing it to fread.
 *
 * The byte count is a 32-bit `imul`, so it wraps; kept in uint32_t so a
 * wrapping size*count still produces the original's early-out and the
 * original's message.
 */
/* WHAT IT DOES: reads from a file and insists on getting everything asked
 * for. Reading nothing at all is reported to the caller as a plain failure --
 * that is how the game detects the end of a file -- but a short read, where
 * some but not all of the data arrived, is treated as the file being damaged
 * and kills the game with a message. */
/* @t4-pass 0x10003430 1 2026-09-09 probes 11 bytes 140 insns 48 regions 3 rows 0 census yes  (fn.py variants: multiply operand order, outer/inner casts, local caching, check reorder) */
/* @t4-pass 0x10003430 2 2026-09-09 probes 12 bytes 140 insns 48 regions 3 rows 0 census yes  (fn.py variants: paren grouping, size/count temps, alternate zero tests, fail-product spellings) */
/* @implements 0x100030E0 d3d BrFChkFRead */
#include <windows.h>
/* The short-read report is a guarded arm and all the successes share one
 * `return 1`; that is what homes `size` in ebx and `count` in edi as the
 * original does (an early `return 1` for got == count swapped them, and no
 * operand order moved it).  The message goes through the CRT's sprintf
 * (import 0x118F0570), not user32's wsprintfA.
 * DEAD 2026-09-09, all identical: named uint32 locals in both orders;
 * wanted after got; a single (uint32_t) cast on the product; uint32
 * params; casts at the fread site; a FILE* local; !wanted; reversed
 * compares; braceless returns; message operand order; uint32 got; buf
 * spelled 1024; char* pDst; every slot in the TU (5).  Corpus MISS on
 * the mov/imul opening.
 * @t4-pass 0x10003430 3 2026-09-09 probes 10 bytes 140 insns 48 regions 3 rows 0 census yes  (hand, fn.py variants + corpus)
 * @t4-pass 0x10003430 4 2026-09-09 probes 15 bytes 140 insns 48 regions 3 rows 0 census yes  (hand, fn.py variants + position sweep) */
int BrFChkFRead(void *pDst, size_t size, size_t count, FILE **ppFile)
{
    /* The original formats the failure message into a 0x400-byte stack buffer
     * (allocated in the prologue) and ships it to OutputDebugStringA. */
    char buf[0x400];
    uint32_t wanted = (uint32_t)size * (uint32_t)count;
    size_t   got;

    if (wanted == 0u)
        return 1;
    got = fread(pDst, size, count, *ppFile);
    if (got == 0u)
        return 0;
    /* The short-read report is the guarded arm and every success falls to
     * ONE `return 1`: an early `return 1` for got == count weights the
     * callee-saved registers the other way (size in ebx, count in edi). */
    if (got != count) {
        sprintf(buf,
                  "FCHK_FRead(): trying to read %d bytes, but got only %d bytes.\n",
                  (int)wanted, (int)((uint32_t)got * (uint32_t)size));
        OutputDebugStringA(buf);
        exit(1);
    }
    return 1;
}
