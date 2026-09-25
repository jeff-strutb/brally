/* br_peerfind.c -- Network peer table: g_aBrPeers and BrPeerFind, which finds (or allocates)
 * the peer slot for a network player id under each slot's mutex.
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

/* ================================================================== */
/* 6. Peer table                                                      */
/* ================================================================== */

/* 0x10036030 */
/* WHAT IT DOES: finds which slot in the network player table belongs to a
 * given player. If that player is not there yet it gives back the first free
 * slot instead, so the same call both looks up and allocates; if the table is
 * full it reports failure. The local player is always slot zero. */
BrPeer g_aBrPeers[BR_PEER_COUNT];   /* Glide 0x117A9B88; loop 1 starts at [1] */

#ifdef BR_MATCHING_BUILD
/* The original probes every record under that record's own Win32 mutex:
 * WaitForSingleObject(h, INFINITE), read f04/f2C, ReleaseMutex(h) -- through
 * the import table (the Wait import is CSEd into ebp, Release stays a
 * memory call).  Each probe's verdict is computed between the reads and the
 * release, then tested after it. */
__declspec(dllimport) unsigned long __stdcall WaitForSingleObject(void *, unsigned long);
__declspec(dllimport) int __stdcall ReleaseMutex(void *);

/* WHAT IT DOES: find which peer slot a network id belongs to. Id 1 is always
 * slot 0 -- the local player -- and everything else is a linear scan. */
/* @implements 0x10036030 d3d BrPeerFind */
int BrPeerFind(uint32_t id)
{
    int i;

    if (id == 1)
        return 0;

    for (i = 1; i < BR_PEER_COUNT; ++i) {
        const BrPeer *p = &g_aBrPeers[i];
        uint32_t idv, st;

        WaitForSingleObject((void *)(uintptr_t)p->hMutex, 0xFFFFFFFFu);
        idv = p->f04;
        st  = p->f2C;
        ReleaseMutex((void *)(uintptr_t)p->hMutex);

        if ((st & BR_PEER_STATE_MASK) >= 1u && idv == id)
            return i;
    }

    for (i = 1; i < BR_PEER_COUNT; ++i) {
        const BrPeer *p = &g_aBrPeers[i];
        uint32_t st;

        WaitForSingleObject((void *)(uintptr_t)p->hMutex, 0xFFFFFFFFu);
        /* The AND is DWORD-width in source; the byte-cast-then-mask spelling
         * made VC5 compute in eax and copy to ebx (+1 insn) -- the dword
         * mask births the load in ebx and computes in place (cracked
         * 2026-09-09).  The byte cast in the `if` still gives `test bl,bl`. */
        st = p->f2C;
        st = (uint32_t)((st & (uint32_t)BR_PEER_STATE_MASK) == 0u);
        ReleaseMutex((void *)(uintptr_t)p->hMutex);

        if ((uint8_t)st)
            return i;
    }

    return -1;
}
#else
int BrPeerFind(uint32_t id)
{
    int i;

    if (id == 1)
        return 0;

    for (i = 1; i < BR_PEER_COUNT; ++i) {
        if ((g_aBrPeers[i].f2C & BR_PEER_STATE_MASK) != 0u &&
            g_aBrPeers[i].f04 == id)
            return i;
    }

    for (i = 1; i < BR_PEER_COUNT; ++i) {
        if ((g_aBrPeers[i].f2C & BR_PEER_STATE_MASK) == 0u)
            return i;
    }

    return -1;
}
#endif
