/* br_cursorpair.c -- Buffer cursor pair: BrCursorPairSet points both cursors of a pair at one
 * place, rewinding the buffer they walk.
 *
 * Filed out of the address batch slice1_05.c; its preamble is carried verbatim.
 */

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
 * hand passes (tools/brally/fnmatch/fn.py variants); the dead-probe list is in the
 * comment block above.  Do not reopen before the end-grind. */
/* @implements 0x1002B280 d3d BrCursorPairSet */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x10575510 */
/* 64-bit core: declared once, in br_globals.h or its struct's header */   /* 0x10575518 */

/* The pair is the fade's target and current value (0x104B16B8 and
 * 0x104B16C0, br_fadewipe.c), and the argument is a float.  The one caller
 * passes 0, i.e. it snaps the fade to fully clear. */
void BrCursorPairSet(float v)
{
    *(float *)&g_brCursor575510 = v;   /* the fade's target (0x104B16B8) */
    *(float *)&g_brCursor575518 = v;   /* and current value (0x104B16C0) */
}
