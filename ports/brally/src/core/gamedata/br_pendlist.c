/* br_pendlist.c -- Pending-work list: BrPendListAdd appends an item to a fixed-length queue,
 * counting drops when it is full.
 *
 * Filed out of the address batch slice1_06.c; its preamble is carried verbatim.
 */

/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
/* The original BrOptSave takes no arguments (loose globals in, packed
 * array out); hide the header's port prototype behind a rename so the
 * matching twin can define the real symbol -- the slice5_63.c caller keeps
 * the port signature (cdecl, extra args harmless at run time). */
#define BrOptSave   BrOptSave_hdr
#define BrOptAvailB BrOptAvailB_hdr
/* The original BrNameListInit is a thiscall ctor with no stack args (vtbl
 * and fill string are fixed); hide the port's 3-arg prototype. */
#define BrNameListInit BrNameListInit_port
#include "slice1_06.h"
#undef BrNameListInit
#undef BrOptSave
#undef BrOptAvailB

#include <stdlib.h>
#include <string.h>

/* Layout facts the original's arithmetic depends on. */
/* BR_PENDLIST_MAX: br_coretypes.h */
typedef char br06_assert_namelist[
    (BR_NAMELIST_COUNT * BR_NAMELIST_STRIDE == 0x1964 * 4) ? 1 : -1];

/* ==========================================================================
 * 0x10037030
 * ========================================================================== */

/* WHAT IT DOES: puts one more item on a fixed-length waiting list. When the
 * list is already full the item is simply thrown away and a "dropped" tally is
 * bumped -- but the length still counts up, so once it overflows it never
 * agrees with what is actually stored again. What the items are is not
 * established here. */
/* Original layout from the context pointer at 0x106C7C3C: items at +4,
 * count at +0x7C. BrPendList in the header starts at the items, so matching
 * uses a wrapper with the leading dword the original addresses through. */
/* BrPendCtx: br_coretypes.h */


/* 64-bit core: declared once, in br_globals.h or its struct's header */     /* 0x106C7C3C */
/* 64-bit core: declared once, in br_globals.h or its struct's header */ /* 0x106C7C40 */

/* WHAT IT DOES: queue a piece of work to run later.  If the queue is
 * already full the item is dropped, but the counter still moves on. */
/* @t4-pass 0x100306D0 1 2026-09-10 probes 30 bytes 51 insns 16 regions 1 rows 0 census yes  (tools/brally/crank.py) */
/* @t4-pass 0x100306D0 2 2026-09-10 probes 30 bytes 51 insns 16 regions 1 rows 0 census yes  (tools/brally/crank.py) */
/* @t3 0x100306D0 2026-09-10 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 51/51 insns 16/16 rows 0+0 regions 1 oracle UNCLASSIFIED
 * @t3-effort passes 2 zero-movement 1 2
 * residue after tools/brally/crank.py: 30 compiles this pass, levers accepted: none;
 * every candidate and score is in build/brally/win32/match/crank.log.
 * Do not reopen before the end-grind. */
/* @implements 0x10037030 d3d BrPendListAdd */
void BrPendListAdd(int32_t id)
{
    BrPendCtx *p = (*(BrPendCtx * *)&g_brP6EECCC);
    int32_t n = p->count;

    if (n < BR_PENDLIST_MAX) {
        p->aItems[n] = id;
        /* Reload 0x106C7C3C before incrementing -- the original does.
         * RESIDUE (0+0 regnorm, T3a): pure eax/ecx rotation from the first
         * instruction -- the original loads the ctx into ecx (6-byte form)
         * and the count into eax; every spelling probed (direct derefs, CSE
         * count, cached p) loads ctx into eax.  30 masked diff bytes. */
        (*(BrPendCtx * *)&g_brP6EECCC)->count++;
    } else {
        g_brPendDropped++;
        p->count++;
    }
}
