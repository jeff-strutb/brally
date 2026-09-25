/* br_varblock.c -- game-data snapshot: the variable-block save/restore pair.
 *
 * BrVarSave gathers a NULL-terminated table of scattered game variables into
 * one contiguous buffer (fatal on overflow, after the fact); BrVarLoad copies
 * such a snapshot back to where each variable lives.
 *
 * Filed out of the address batch slice3_41.c, with that file's preamble.
 */

#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "slice3_41.h"

/* =====================================================================
 * 2.  Variable-block save / restore
 * ===================================================================== */

/* 0x10067880 */
/* WHAT IT DOES: gathers a list of scattered game variables into one
 * contiguous block of memory -- the snapshot the replay and save-state code
 * works from. If the block turns out not to have been big enough it stops the
 * game with an error, but only after the overrun has already happened. */
/* @t4-pass 0x100608F0 1 2026-09-10 probes 40 bytes 114 insns 48 regions 2 rows 1 census yes  (tools/crank.py) */
/* @t4-pass 0x100608F0 2 2026-09-10 probes 40 bytes 114 insns 48 regions 2 rows 1 census yes  (tools/crank.py) */
/* @t4-pass 0x100608F0 3 2026-09-10 probes 40 bytes 114 insns 48 regions 2 rows 1 census yes  (tools/crank.py) */
/* @t3 0x100608F0 2026-09-10 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 114/118 insns 48/49 rows 1+0 regions 2 oracle UNCLASSIFIED
 * @t3-effort passes 4 zero-movement 3 4
 * residue after tools/crank.py: 40 compiles this pass, levers accepted: mut:addr_taken:i;
 * every candidate and score is in build/match/crank.log.
 * Do not reopen before the end-grind (CLAUDE.md rule 12). */
/* @t4-pass 0x100608F0 4 2026-09-10 probes 12 bytes 114 insns 48 regions 2 rows 1 census yes  (tools/crank.py) */
/* @implements 0x10067880 d3d BrVarSave */
/* @n64 0x8022ADCC located */
void BrVarSave(const BrVarBlock *pTable, void *pDst, int32_t cbAvail)
{
    uint8_t *pOut = (uint8_t *)pDst;
    int32_t  cbUsed;
    int      i;

    /* INDEXED, not a walked pointer.  The original keeps the entry's own
     * address live and re-reads the size through it at the bottom of the loop
     * (`mov ebx,edx` ... `mov edi,[ebx+4]`); a `pTable++` cursor makes VC5
     * advance first and read back through a negative displacement
     * (`add eax,8` ... `mov edi,[eax-4]`).  Spelling the accesses indexed and
     * letting strength reduction build the cursor is what reproduces it --
     * six loop shapes probed, this is the only one that lands
     * (register-blind residue 4+1 -> 0+1). */
    for (i = 0; pTable[i].pData != NULL; i++) {
        memcpy(pOut, pTable[i].pData, (size_t)pTable[i].cb);
        pOut += pTable[i].cb;
    }

    cbUsed = (int32_t)(pOut - (uint8_t *)pDst);
    if (cbUsed > cbAvail) {
        /* sprintf into an 80-byte stack buffer, as the original does --
         * confirmed against the N64 build (TGR USA 0x8022adcc), which
         * compiles the same source with plain sprintf + fatal. */
        char szMsg[0x50];
        sprintf(szMsg,
                "VAR SAVE OVERFLOW (%d avail, %d used)",
                (int)cbAvail, (int)cbUsed);
        BrFatal(szMsg);
    }
}

/* 0x10067900 */
/* WHAT IT DOES: puts a previously gathered snapshot back where it came from,
 * restoring every variable in the list. It trusts the buffer completely --
 * there is no length given and no check made. */
/* @t4-pass 0x10060970 1 2026-09-10 probes 40 bytes 60 insns 30 regions 4 rows 0 census yes  (tools/crank.py) */
/* @t4-pass 0x10060970 2 2026-09-10 probes 40 bytes 60 insns 30 regions 4 rows 0 census yes  (tools/crank.py) */
/* @t3 0x10060970 2026-09-10 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 60/60 insns 30/30 rows 0+0 regions 4 oracle UNCLASSIFIED
 * @t3-effort passes 2 zero-movement 1 2
 * residue after tools/crank.py: 40 compiles this pass, levers accepted: none;
 * every candidate and score is in build/match/crank.log.
 * Do not reopen before the end-grind (CLAUDE.md rule 12). */
/* @implements 0x10067900 d3d BrVarLoad */
/* @n64 0x8022AE70 located */
void BrVarLoad(const BrVarBlock *pTable, const void *pSrc)
{
    const uint8_t *pIn = (const uint8_t *)pSrc;
    int i;

    /* INDEXED, not a walked pointer -- the same lever as BrVarSave above: a
     * `pTable++` cursor makes VC5 advance first and read the size back through
     * a negative displacement (`mov R,[M-4]`), where the original keeps the
     * entry's own address live and reads it forward (`mov R,[M+4]`).  Spelling
     * the accesses indexed and letting strength reduction build the cursor
     * reproduces it: register-blind residue 5+1 -> 0+0, size and instruction
     * count exact. */
    for (i = 0; pTable[i].pData != NULL; i++) {
        memcpy(pTable[i].pData, pIn, (size_t)pTable[i].cb);
        pIn += pTable[i].cb;
    }
}
