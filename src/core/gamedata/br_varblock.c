/* br_varblock.c -- game-data snapshot: the variable-block save/restore pair.
 *
 * BrVarSave gathers a NULL-terminated table of scattered game variables into
 * one contiguous buffer (fatal on overflow, after the fact); BrVarLoad copies
 * such a snapshot back to where each variable lives.
 *
 * Filed out of the address batch slice3_41.c, with that file's preamble.
 */

/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
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
/* @t4-pass 0x100608F0 4 2026-09-10 probes 12 bytes 114 insns 48 regions 2 rows 1 census yes  (tools/crank.py) */
/* @implements 0x10067880 d3d BrVarSave */
/* @n64 0x8022ADCC located */
void BrVarSave(const BrVarBlock *pTable, void *pDst, int32_t cbAvail)
{
    /* The PARAMETER is the cursor and the start is saved in a local: VC5
     * homes the start in pDst's own (dead) argument slot, the original's
     * `mov [esp+0x68],eax`, and reads it back for the length. */
    uint8_t *pStart = (uint8_t *)pDst;
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
        memcpy(pDst, pTable[i].pData, (size_t)pTable[i].cb);
        pDst = (uint8_t *)pDst + pTable[i].cb;
    }

    cbUsed = (int32_t)((uint8_t *)pDst - pStart);
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
/* @implements 0x10067900 d3d BrVarLoad */
/* @n64 0x8022AE70 located */
void BrVarLoad(const BrVarBlock *pTable, const void *pSrc)
{
    int i;

    /* INDEXED, not a walked pointer -- the same lever as BrVarSave above: a
     * `pTable++` cursor makes VC5 advance first and read the size back through
     * a negative displacement (`mov R,[M-4]`), where the original keeps the
     * entry's own address live and reads it forward (`mov R,[M+4]`).  Spelling
     * the accesses indexed and letting strength reduction build the cursor
     * reproduces it: register-blind residue 5+1 -> 0+0, size and instruction
     * count exact. */
    for (i = 0; pTable[i].pData != NULL; i++) {
        /* pSrc itself is the cursor: VC5 then loads it inside the guard,
         * after the first entry test, as the original does. */
        memcpy(pTable[i].pData, pSrc, (size_t)pTable[i].cb);
        pSrc = (const uint8_t *)pSrc + pTable[i].cb;
    }
}
