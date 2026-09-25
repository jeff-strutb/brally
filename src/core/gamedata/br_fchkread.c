/* br_fchkread.c -- gamedata: FCHK_FRead, the checked read under every data
 * file the game loads.
 *
 * Filed out of the address batch slice1_01.c; the CHK_* allocation and
 * existence helpers stay there as port-only bodies (their Glide matches are
 * in src/core/generated/). The preamble is slice1_01.c's, carried whole.
 */

#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include "slice1_01.h"

#include <stdlib.h>

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
/* @t3 0x10003430 2026-09-09 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 140/140 insns 48/48 rows 0+0 regions 3 oracle UNCLASSIFIED
 * @t3-effort passes 4 zero-movement 3 4
 * Residue is register allocation: the original homes `size` in ebx and
 * `count` in edi; this build homes them the other way round, which flips the
 * two pushes and the imul operands (8 masked diffs).  Every instruction is
 * the original's; the dossier is the RESIDUE comment below.  Passes 1-2
 * (ledger above) moved nothing at 140/48/3/0 -- operand order, casts, local
 * caching and check reorder are all codegen-identical; only `wanted <= 0u`
 * went worse.  Do not reopen before the end-grind (CLAUDE.md rule 12). */
/* @implements 0x100030E0 d3d BrFChkFRead */
#ifdef BR_MATCHING_BUILD
#include <windows.h>
#endif
/* RESIDUE (8 masked diffs, REGNORM 0+0): the original homes `size` in
 * ebx and `count` in edi; this build homes them the other way round, which
 * flips the two `push`es and the `imul` operands. Every instruction is the
 * original's. Writing the product `count * size` instead of `size * count`
 * changes nothing -- VC5 canonicalises the multiply the same way it does a
 * commutative add.
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
#ifdef BR_MATCHING_BUILD
    /* The original formats the failure message into a 0x400-byte stack buffer
     * (allocated in the prologue) and ships it to OutputDebugStringA. */
    char buf[0x400];
#endif
    uint32_t wanted = (uint32_t)size * (uint32_t)count;
    size_t   got;

    if (wanted == 0u) {
        return 1;
    }

    got = fread(pDst, size, count, *ppFile);

    if (got == 0u) {
        return 0;
    }
    if (got == count) {
        return 1;
    }

#ifdef BR_MATCHING_BUILD
    wsprintfA(buf,
              "FCHK_FRead(): trying to read %d bytes, but got only %d bytes.\n",
              (int)wanted, (int)((uint32_t)got * (uint32_t)size));
    OutputDebugStringA(buf);
    exit(1);
#else
    fprintf(stderr,
            "FCHK_FRead(): trying to read %d bytes, but got only %d bytes.\n",
            (int)wanted, (int)((uint32_t)got * (uint32_t)size));
    exit(1);
#endif

    return 1;   /* the original falls through to the `mov eax,1` tail */
}
