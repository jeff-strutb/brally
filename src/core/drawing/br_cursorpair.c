/* br_cursorpair.c -- Buffer cursor pair: BrCursorPairSet points both cursors of a pair at one
 * place, rewinding the buffer they walk.
 *
 * Filed out of the address batch slice1_05.c; its preamble is carried verbatim.
 */

#ifdef BR_MATCHING_BUILD
/* The originals of the vtx-cache cluster take no BrVtxCache parameter --
 * state is loose globals -- and BrVtxExpand/Insert/Resolve have different
 * arities. Hide the header's port prototypes behind renames so the
 * matching twins can define the real symbols with the original
 * signatures; other TUs keep calling with the port signatures (cdecl, so
 * the extra leading argument is harmless at run time). */
#define BrVtxExpand       BrVtxExpand_hdr
#define BrVtxCacheInsert  BrVtxCacheInsert_hdr
#define BrVtxCacheResolve BrVtxCacheResolve_hdr
#define BrSelLookup       BrSelLookup_hdr
#define BrPtrListAdd      BrPtrListAdd_hdr
#define BrF3DVtxFixup     BrF3DVtxFixup_hdr
#include "slice1_05.h"
#include "br_gamestep.h"
#undef BrVtxExpand
#undef BrVtxCacheInsert
#undef BrVtxCacheResolve
#undef BrSelLookup
#undef BrPtrListAdd
#undef BrF3DVtxFixup
#else
#include "slice1_05.h"
#include "br_gamestep.h"   /* 0x10034C66/0x10034C73 == BRGlide 0x1002E317/0x1002E324 */
#endif

#include <stddef.h>

/* 0x1002B280 */
/* WHAT IT DOES: points both halves of a pair of cursors at the same place,
 * which is how a buffer gets rewound to its start. What the buffer holds is not
 * established here. */
/* @t4-pass 0x100182F0 1 2026-09-09 probes 12 bytes 15 insns 4 regions 1 rows 1 census yes  (hand, fn.py variants) */
/* @t4-pass 0x100182F0 2 2026-09-09 probes 23 bytes 15 insns 4 regions 1 rows 1 census yes  (hand, fn.py variants) */
/* @t3 0x100182F0 2026-09-09 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 15/18 insns 4/5 rows 1+0 regions 1 oracle UNCLASSIFIED
 * @t3-effort passes 2 zero-movement 1 2
 * residue is allocation/scheduling: 1+0 classified rows, 1 masked region, 3 B short;
 * every row pairs under t3.py's canonical classes.  Effort: 2 counted
 * @t4-pass passes (ledger lines above, zero movement on passes 1 and 2);
 * hand passes (tools/fnmatch/fn.py variants); the dead-probe list is in the
 * comment block above.  Do not reopen before the end-grind (CLAUDE.md rule 12). */
/* @implements 0x1002B280 d3d BrCursorPairSet */
#ifdef BR_MATCHING_BUILD
void *g_brCursor575510;   /* 0x10575510 */
void *g_brCursor575518;   /* 0x10575518 */

void BrCursorPairSet(void *pv)
{
    /* CLOSE, NOT MATCHING -- 15 bytes / 4 instructions against the original's
     * 18 / 5.  The one argument and the two absolute stores are right; the
     * whole residue is that the original keeps a second live copy:
     *
     *     8b c8            mov ecx, eax
     *     a3 <g1>          mov [g1], eax      (accumulator form, 5 bytes)
     *     89 0d <g2>       mov [g2], ecx      (6 bytes)
     *
     * against our `a3 <g1>` / `a3 <g2>`.  Note the original's encoding is
     * strictly WORSE -- one more instruction and one more byte -- so VC5 is
     * not choosing it for size; it is holding two live ranges where we have
     * one.  See the accumulator-encoding entry in docs/VC5-IDIOMS.md.
     *
     * PROBED AND DEAD, do not re-run -- ALL of these compile to the identical
     * 15 bytes: chained assignment (a = b = pv); either store order; a `void
     * *p = pv` local feeding one or both stores; and reading the first global
     * BACK for the second store (`g2 = g1;`), in both orders -- VC5 forwards
     * the store and folds the load away.  Register-allocation class; the
     * `VARIABLE IDENTITY IS INERT` entry says VC5 splits live ranges itself,
     * which is the same statement from the other side.
     *
     * DEAD 2026-09-09, all identical 15 B unless noted: returning pv (the
     * return-value lever that broke 0x1006CDA0); one or both stores through
     * an __inline helper, a pointer-taking helper, a helper returning pv;
     * pointer-to-int conversion on either global or chained through one;
     * an int-typed parameter; const/char-typed parameter; the globals as a
     * 2-element struct array or one struct; extern-only declarations;
     * volatile global (compile error); `*(&g)` stores; comma expression;
     * a static or volatile temp (+2 / +11 B); every slot in the TU (13).
     * Corpus MISS at +0x4 (len 7 and 12); no N64 twin. */
    g_brCursor575510 = pv;
    g_brCursor575518 = pv;
}
#else
void BrCursorPairSet(BrCursorPair *pPair, void *pv)
{
    pPair->f10 = pv;
    pPair->f18 = pv;
}
#endif
