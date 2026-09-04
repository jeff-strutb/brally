/* br_peerslot.c -- net.
 *
 * Reading one peer record's status word under that record's own mutex.
 * Two tables, two record shapes, the same three-step lock/read/unlock.
 *
 * Filed out of the address batches: these functions were
 * matched first and grouped by what they are afterwards.
 * Every function carries its original address.
 */
#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import
 * table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include <stddef.h>
#include <stdint.h>

#ifdef BR_MATCHING_BUILD
/* ==========================================================================
 * 0x100714D0
 * ========================================================================== */

/* WHAT IT DOES: reads one peer-table record's status word under that
 * record's mutex. The index selects a 0x96C-byte slot in the table at
 * 0x11786828; the word returned is the dword at +0x2C. */

/* __declspec(dllimport) is what emits `call dword ptr [IAT]` rather than a
 * direct `call` thunk. Both KERNEL32 imports are stdcall. */
__declspec(dllimport) unsigned long __stdcall WaitForSingleObject(
    void *hHandle, unsigned long dwMilliseconds);
__declspec(dllimport) int __stdcall ReleaseMutex(void *hMutex);

typedef struct Br71Peer {
    void         *hMutex;            /* +0x000 */
    unsigned char pad004[0x28];
    uint32_t      f02C;              /* +0x02C */
    unsigned char pad030[0x93C];
} Br71Peer;

typedef char br71_assert_peer_stride[(sizeof(Br71Peer) == 0x96C) ? 1 : -1];
typedef char br71_assert_peer_f02c[
    (offsetof(Br71Peer, f02C) == 0x2C) ? 1 : -1];

/* XSLICE 0x11786828 -- sixteen records of 0x96C. Indexed, not pointed-to:
 * esi holds index*stride and the disp32 is the table base. */
extern Br71Peer g_aBrPeer71[];

/* WHAT IT DOES: under the peer mutex, read one word of that peer's
 * record and hand it back -- a live snapshot, not a stale copy. */
/* @implements 0x100714D0 d3d BrSub100714D0 */
uint32_t BrSub100714D0(int index)
{
    uint32_t v;

    /* GOTCHA: the original reloads hMutex for ReleaseMutex rather than
     * reusing the wait's handle, so the three field accesses stay as three
     * indexings of the global. Caching the handle would drop a load. */
    WaitForSingleObject(g_aBrPeer71[index].hMutex, (unsigned long)-1);
    v = g_aBrPeer71[index].f02C;
    ReleaseMutex(g_aBrPeer71[index].hMutex);
    return v;
}

/* ==========================================================================
 * 0x10071510
 * ==========================================================================
 *
 * Twin of 0x100714D0 (peer table at 0x11786828, stride 0x96C).  Same mutex
 * + status-word pair, different table.
 *
 * GOTCHA: WaitForSingleObject's result is discarded -- a failed wait still
 * reads +0x2C and still ReleaseMutex.
 *
 * lea arithmetic, confirmed against the dump:
 *   shl 0xa + add ecx     -> 1025*ecx
 *   lea [eax+eax*4]       -> 5125*ecx
 *   lea [ecx+eax*2]       -> 10251*ecx
 *   shl 2                 -> 41004*ecx == 0xA02C*ecx
 */
__declspec(dllimport) unsigned long __stdcall WaitForSingleObject(
    void *hHandle, unsigned long dwMilliseconds);
__declspec(dllimport) int __stdcall ReleaseMutex(void *hMutex);

typedef struct Br71510Rec {
    void    *hMutex;                 /* +0x00 = 0x1178FEF8 */
    char     pad04[0x28];
    int32_t  f2C;                    /* +0x2C = 0x1178FF24 */
    char     rest[0xA02C - 0x30];
} Br71510Rec;

typedef char Br71510RecSize[(sizeof(Br71510Rec) == 0xA02C) ? 1 : -1];

Br71510Rec g_aBr178FEF8[1];          /* 0x1178FEF8, stride 0xA02C */

/* WHAT IT DOES: reads one record's status word under that record's lock. */
/* @implements 0x10071510 d3d BrSub10071510 */
int32_t BrSub10071510(int32_t i)
{
    void    *h;
    int32_t  v;

    WaitForSingleObject(g_aBr178FEF8[i].hMutex, (unsigned long)-1);
    h = g_aBr178FEF8[i].hMutex;
    v = g_aBr178FEF8[i].f2C;
    ReleaseMutex(h);
    return v;
}

#endif /* BR_MATCHING_BUILD */
