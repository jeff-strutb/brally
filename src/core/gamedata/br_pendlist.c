/* br_pendlist.c -- Pending-work list: BrPendListAdd appends an item to a fixed-length queue,
 * counting drops when it is full.
 *
 * Filed out of the address batch slice1_06.c; its preamble is carried verbatim.
 */

#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
/* The original BrOptSave takes no arguments (loose globals in, packed
 * array out); hide the header's port prototype behind a rename so the
 * matching twin can define the real symbol -- the slice5_63.c caller keeps
 * the port signature (cdecl, extra args harmless at run time). */
#define BrOptSave   BrOptSave_hdr
#define BrOptAvailB BrOptAvailB_hdr
#ifdef BR_MATCHING_BUILD
/* The original BrNameListInit is a thiscall ctor with no stack args (vtbl
 * and fill string are fixed); hide the port's 3-arg prototype. */
#define BrNameListInit BrNameListInit_port
#include "slice1_06.h"
#undef BrNameListInit
#else
#include "slice1_06.h"
#endif
#undef BrOptSave
#undef BrOptAvailB
#else
#include "slice1_06.h"
#endif

#include <stdlib.h>
#include <string.h>

/* Layout facts the original's arithmetic depends on. */
typedef char br06_assert_pendlist[
    (offsetof(BrPendList, count) == BR_PENDLIST_MAX * sizeof(void *)) ? 1 : -1];
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
#ifdef BR_MATCHING_BUILD
/* Original layout from the context pointer at 0x106C7C3C: items at +4,
 * count at +0x7C. BrPendList in the header starts at the items, so matching
 * uses a wrapper with the leading dword the original addresses through. */
typedef struct BrPendCtx {
    uint32_t unused0;
    void    *apItems[BR_PENDLIST_MAX];
    int32_t  count;
} BrPendCtx;

typedef char br06_assert_pendctx[
    (offsetof(BrPendCtx, count) == 0x7C) ? 1 : -1];

BrPendCtx *g_brPendCtx;     /* 0x106C7C3C */
uint32_t   g_brPendDropped; /* 0x106C7C40 */

/* WHAT IT DOES: queue a piece of work to run later.  If the queue is
 * already full the item is dropped, but the counter still moves on. */
/* @t4-pass 0x100306D0 1 2026-09-10 probes 30 bytes 51 insns 16 regions 1 rows 0 census yes  (tools/crank.py) */
/* @t4-pass 0x100306D0 2 2026-09-10 probes 30 bytes 51 insns 16 regions 1 rows 0 census yes  (tools/crank.py) */
/* @t3 0x100306D0 2026-09-10 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 51/51 insns 16/16 rows 0+0 regions 1 oracle UNCLASSIFIED
 * @t3-effort passes 2 zero-movement 1 2
 * residue after tools/crank.py: 30 compiles this pass, levers accepted: none;
 * every candidate and score is in build/match/crank.log.
 * Do not reopen before the end-grind (CLAUDE.md rule 12). */
/* @implements 0x10037030 d3d BrPendListAdd */
void BrPendListAdd(BrPendList *pList, void *pItem, uint32_t *pcDropped)
{
    BrPendCtx *p = g_brPendCtx;
    int32_t n = p->count;

    if (n < BR_PENDLIST_MAX) {
        p->apItems[n] = pList;
        /* Reload 0x106C7C3C before incrementing -- the original does.
         * RESIDUE (0+0 regnorm, T3a): pure eax/ecx rotation from the first
         * instruction -- the original loads the ctx into ecx (6-byte form)
         * and the count into eax; every spelling probed (direct derefs, CSE
         * count, cached p) loads ctx into eax.  30 masked diff bytes. */
        g_brPendCtx->count++;
    } else {
        g_brPendDropped++;
        p->count++;
    }
}
#else
/* WHAT IT DOES: queue a piece of work to run later.  If the queue is
 * already full the item is dropped, but the counter still moves on. */
/* port-only variant of BrPendListAdd (matching build uses the #ifdef branch above) */
void BrPendListAdd(BrPendList *pList, void *pItem, uint32_t *pcDropped)
{
    if (pList->count < BR_PENDLIST_MAX) {
        pList->apItems[pList->count] = pItem;
        /* The original re-loads the context pointer from 0x106C7C3C here
         * before incrementing -- irrelevant unless the callee moved it, and
         * there is no callee. */
        pList->count++;
        return;
    }

    /* Over capacity: drop the item, bump the global at 0x106C7C40, and STILL
     * increment the counter. */
    if (pcDropped != NULL) {          /* DEVIATION: the original's counter is
                                       * a fixed global and is never NULL. */
        (*pcDropped)++;
    }
    pList->count++;
}
#endif
