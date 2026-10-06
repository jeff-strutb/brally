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
/* The original takes ONE float (see below); hide the header's pointer
 * prototype. */
#define BrCursorPairSet   BrCursorPairSet_hdr
#include "slice1_05.h"
#include "br_gamestep.h"
#undef BrVtxExpand
#undef BrVtxCacheInsert
#undef BrVtxCacheResolve
#undef BrSelLookup
#undef BrPtrListAdd
#undef BrF3DVtxFixup
#undef BrCursorPairSet

#include <stddef.h>

/* 0x1002B280 */
/* WHAT IT DOES: points both halves of a pair of cursors at the same place,
 * which is how a buffer gets rewound to its start. What the buffer holds is not
 * established here. */
/* @t4-pass 0x100182F0 1 2026-09-09 probes 12 bytes 15 insns 4 regions 1 rows 1 census yes  (hand, fn.py variants) */
/* @t4-pass 0x100182F0 2 2026-09-09 probes 23 bytes 15 insns 4 regions 1 rows 1 census yes  (hand, fn.py variants) */
/* @implements 0x1002B280 d3d BrCursorPairSet */
/* The pair is the fade's target and current value (glide 0x104B16B8 and
 * 0x104B16C0, br_fadewipe.c), and the argument is a FLOAT: copying one float
 * to two float globals is what makes VC5 keep the second live copy the
 * original has (`mov ecx,eax`, then the a3 and 89 0d stores).  The one
 * caller passes 0, i.e. it snaps the fade to fully clear. */
extern float g_brFadeTarget, g_brFadeValue;

void BrCursorPairSet(float v)
{
    g_brFadeTarget = v;
    g_brFadeValue  = v;
}
