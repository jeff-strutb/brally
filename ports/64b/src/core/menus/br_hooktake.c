/* br_hooktake.c -- menus: the two front-end "take" hooks at 0x10034C88 and
 * 0x10034CA8 (BRGlide 0x1002E339 / 0x1002E359).
 *
 * Filed out of slice1_05.c, whose preamble it keeps verbatim below so the
 * compiler's view of these bodies is unchanged.  See slice1_05.h for the
 * per-function notes and gotchas. */

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


/* 0x10034C88 */
/* WHAT IT DOES: purpose unclear. Observably it raises one flag and hands back
 * the value of a counter, ignoring the pointer it is passed -- the routine the
 * original called with it does not read it either. */
/* @implements 0x10034C88 d3d BrHookTakeA */
/* Literal form: 3-arg call into 0x10030F40 (which ignores its arguments and
 * just raises the one-shot flag), then return the destination global.  The
 * copy therefore never happens in practice; the shape is the original's. */
/* FUN_10030f40: prototype in br_funcs.h */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
uint32_t BrHookTakeA(BrHooks *pH, const void *pSrc)
{
    FUN_10030f40();
    return DAT_106e79c8;
}

/* 0x10034CA8 */
/* WHAT IT DOES: the twin of the routine above -- same raised flag, same
 * ignored argument, but it returns a different counter. Purpose equally
 * unclear. */
/* @implements 0x10034CA8 d3d BrHookTakeB */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
uint32_t BrHookTakeB(BrHooks *pH, const void *pSrc)
{
    FUN_10030f40();
    return DAT_106ea390;
}
