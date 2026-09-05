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
    uint32_t      f004;              /* +0x004  the peer's DirectPlay id */
    unsigned char pad008[0x24];
    int32_t       f02C;              /* +0x02C  status word; low 6 bits = state */
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

/* ==========================================================================
 * 0x1006B0E0 (D3D 0x10072170)
 * ========================================================================== */

__declspec(dllimport) unsigned long __stdcall WaitForMultipleObjects(
    unsigned long nCount, void **pHandles, int bWaitAll,
    unsigned long dwMilliseconds);
__declspec(dllimport) void __stdcall ExitThread(unsigned long dwExitCode);

/* 0x11849E60 -- the networking thread's quit event (slice8_86.h names it
 * g_hBrSndWake86; br_peer.c reads it as DAT_11849e60). */
extern void *DAT_11849e60;

/* 0x11849F30 -- sixteen outgoing bit streams, one per peer slot, 0x214
 * apart: br_bitstream.c's five-dword header over a 0x200-byte buffer.
 * br_objlife.c constructs the array in place (g_1826BD0, its D3D name). */
typedef struct BrNetSendBs {
    int32_t        readBit;      /* +0x00 */
    int32_t        readByte;     /* +0x04 */
    int32_t        writeBit;     /* +0x08 */
    int32_t        writeByte;    /* +0x0C */
    unsigned char *pBuf;         /* +0x10 */
    unsigned char  buf[0x200];   /* +0x14 */
} BrNetSendBs;
typedef char br71_assert_sendbs_stride[(sizeof(BrNetSendBs) == 0x214) ? 1 : -1];
extern BrNetSendBs g_aBrNetSendBs[16];

/* br_state.c -- 0x1006D180 and 0x1006D190, both thiscall on the stream:
 * bytes written (a partial byte counts as one) and the buffer pointer. */
int   __fastcall BrCountedTotal(const void *pObj);
void *__fastcall BrStateGetField10(const void *pObj);

/* 0x100038F0 -- delivers peer 0's packet locally (br_appmsg.c's name). */
extern void BrSub10003580(void *pv1, void *pData, int32_t cbData,
                          int32_t idFrom, int32_t a5);
/* br_dplaysend.c -- 0x10009A00, IDirectPlay4A::Send under the lock. */
extern int BrDPlayRawSend(void *pIface, uint32_t idFrom, uint32_t idTo,
                          uint32_t flags, void *pData, int32_t cbData);

/* WHAT IT DOES: one pass of the networking thread's send loop.  For each of
 * the sixteen peer slots it takes that peer's mutex (leaving the thread if
 * the quit event fires first), and if the peer is live and its outgoing
 * stream holds more than a bare header, ships the stream: slot 0 is the
 * local player, whose packet goes straight to the message handler; every
 * other slot goes out over DirectPlay to that peer's id.  Reports -1 if any
 * DirectPlay send failed, else 0.
 *
 * PARKED 2026-09-04 at 9 diff bytes, regnorm 0+0, same size: one region.
 * In the DirectPlay send's argument list the original loads *ppDp (arg 1)
 * into ecx right after the buffer-pointer call and BEFORE pushing that
 * call's result, then loads the peer id (arg 3) into eax after the push;
 * this build hoists BOTH loads above the push (id -> ecx, *ppDp -> edx).
 * Dead probes: do-while vs for; a named Br71Peer *pPeer (worse: indexed
 * loads become [R*K+A]); a named pDp local (worse: ppDp leaves ebp); a
 * named pBs stream pointer (kept, inert); status word signed (needed for
 * the `jl`).  corpus.py has no witness for the call-load-push run. */
/* @implements 0x1006B0E0 glide BrNetPeerSendPass */
int32_t BrNetPeerSendPass(void **ppDp)
{
    int32_t      result = 0;
    void        *h[2];
    int32_t      i;
    BrNetSendBs *pBs;

    for (i = 0; i < 16; i++) {
        h[0] = DAT_11849e60;
        h[1] = g_aBrPeer71[i].hMutex;
        if (WaitForMultipleObjects(2, h, 0, (unsigned long)-1) == 0)
            ExitThread(0);
        /* `and ecx,0x3f; cmp cl,1; jl` -- a SIGNED compare, so the status word
         * is an int here (br_peer.c's `cmp cl,5; jge` is the same shape). */
        if ((g_aBrPeer71[i].f02C & 0x3F) >= 1) {
            pBs = &g_aBrNetSendBs[i];
            if (BrCountedTotal(pBs) > 3) {
                if (i == 0) {
                    BrSub10003580(ppDp,
                                  BrStateGetField10(&g_aBrNetSendBs[0]),
                                  BrCountedTotal(&g_aBrNetSendBs[0]), 1, 1);
                } else if (BrDPlayRawSend(*ppDp, 1, g_aBrPeer71[i].f004, 0,
                                          BrStateGetField10(pBs),
                                          BrCountedTotal(pBs)) != 0) {
                    result = -1;
                }
            }
        }
        ReleaseMutex(g_aBrPeer71[i].hMutex);
    }
    return result;
}

#endif /* BR_MATCHING_BUILD */
