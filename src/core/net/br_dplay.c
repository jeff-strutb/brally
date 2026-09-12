/* br_dplay.c -- net.
 *
 * The DirectPlay session: the receive pump and its background thread, the
 * housekeeping-message handler, start-up and shutdown, and the player-count
 * query the lobby shows.
 *
 * Filed out of the address batches: these functions were
 * matched first and grouped by what they are afterwards.
 * Every function carries its original address.
 */

#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef BR_MATCHING_BUILD
/* Header prototype is cdecl; the original is __stdcall. */
#define BrDPlayThreadProc  BrDPlayThreadProc_cdecl_hdr
#endif
#include "slice2_13.h"
#ifdef BR_MATCHING_BUILD
#undef BrDPlayThreadProc
uint32_t __stdcall BrDPlayThreadProc(void *pvCtx);
#endif
#include "slice1_03.h"   /* BrAppMsg, BrAppMsgDispatch (= 0x1000BEA0) */

/* ==========================================================================
 * Cross-slice declarations
 * ========================================================================== */

/* XSLICE 0x10071480 */
extern void BrSub10071480(uint32_t idPlayer);
/* XSLICE 0x10005FE0 */
extern void BrSub10005FE0(uint32_t idPlayer);
/* XSLICE 0x100360F0 */
extern void BrSub100360F0(void *pv1, uint32_t f0C, uint32_t f10,
                          uint32_t f08, uint32_t idTo);
/* XSLICE 0x1003CE80 */
extern void BrSub1003CE80(void);
/* 0x1000BAF0, the non-system message route. slice2_22 knows it as
 * APPMSG_HOSTSTARTED.
 * XSLICE 0x1000BAF0 */
extern void BrSub1000BAF0(void *pCtx, const void *pvData, uint32_t cbData,
                          uint32_t idFrom, uint32_t idTo);
/* 0x1003D0B0 -- "size it, allocate it, fill it" over state->pDPGlobal.
 * *ppvOut receives a GlobalAlloc'd + GlobalLock'ed record.
 * XSLICE 0x1003D0B0 */
extern int32_t BrSub1003D0B0(BrDPlay4Obj *pObj, void **ppvOut);

/* ==========================================================================
 * 3. DirectPlay
 * ========================================================================== */

static BrDPlayState g_BrDPlay;

BrDPlayState *BrDPlayGetState(void)
{
    return &g_BrDPlay;
}

#ifdef BR_MATCHING_BUILD
/* DirectPlay's Win32 imports are stdcall IAT calls (FF 15). The portable
 * BrDPlayOs function-pointer table is cdecl and cannot emit that sequence. */
__declspec(dllimport) void *__stdcall GlobalAlloc(unsigned uFlags, unsigned dwBytes);
__declspec(dllimport) void *__stdcall GlobalLock(void *hMem);
__declspec(dllimport) void *__stdcall GlobalHandle(void *pMem);
__declspec(dllimport) int   __stdcall GlobalUnlock(void *hMem);
__declspec(dllimport) void *__stdcall GlobalFree(void *hMem);
__declspec(dllimport) void  __stdcall InitializeCriticalSection(void *pCs);
__declspec(dllimport) void  __stdcall DeleteCriticalSection(void *pCs);
__declspec(dllimport) void *__stdcall CreateEventA(void *pSa, int fManual,
                                                   int fInit, const char *psz);
__declspec(dllimport) void *__stdcall CreateThread(void *pSa, unsigned cbStack,
    unsigned long (__stdcall *pfnStart)(void *), void *pvArg,
    unsigned uFlags, unsigned long *pid);
__declspec(dllimport) unsigned long __stdcall WaitForMultipleObjects(
    unsigned long n, void *const *ah, int fWaitAll, unsigned long ms);
__declspec(dllimport) unsigned long __stdcall WaitForSingleObject(
    void *h, unsigned long ms);
__declspec(dllimport) int  __stdcall CloseHandle(void *h);
__declspec(dllimport) int  __stdcall SetEvent(void *h);
__declspec(dllimport) void __stdcall ExitThread(unsigned long code);

/* 0x10273310 -- the original's CRITICAL_SECTION. */
static int g_BrDPlayCrit[6];

/* COM vtable slots are stdcall; slice2_13.h's BrDPlay4Vtbl is cdecl. */
typedef struct BrDPlay4VtblStd {
    void *aSlots00[2];
    int32_t (__stdcall *Release)(BrDPlay4Obj *pThis);
    void *aSlots03[1];
    int32_t (__stdcall *Close)(BrDPlay4Obj *pThis);
    void *aSlots05[4];
    int32_t (__stdcall *DestroyPlayer)(BrDPlay4Obj *pThis, uint32_t idPlayer);
    void *aSlots10[15];
    int32_t (__stdcall *Receive)(BrDPlay4Obj *pThis, uint32_t *pidFrom,
                                 uint32_t *pidTo, uint32_t dwFlags,
                                 void *pvData, uint32_t *pcbData);
} BrDPlay4VtblStd;
#endif

/* -- 0x1000C000 ---------------------------------------------------------- */

/* WHAT IT DOES: reacts to the housekeeping messages the networking layer sends
 * about the multiplayer session itself. Only two matter: a player leaving,
 * which makes the game tear that player's presence down, and one further
 * message type that is handed straight on elsewhere. Everything else is
 * ignored. The leaving case is skipped entirely while the lobby log is
 * running, because the logging path does that clean-up instead. */
/* @implements 0x10009530 glide BrDPlaySysMsgDispatch */
void BrDPlaySysMsgDispatch(void *pv1, const BrDPlaySysMsg *pMsg,
                           uint32_t cbData, uint32_t idFrom, uint32_t idTo)
{
    uint32_t dwType = pMsg->dwType;

    (void)cbData;
    (void)idFrom;

    /* Empty DPSYS_* labels (same `ret` as default) keep the two-level jump
     * table 0x31..0x107. Folding them collapses it to a range check.
     * `return` (not `break`) so each label is its own group. */
    switch (dwType) {
    case 3:                         /* DPSYS_CREATEPLAYERORGROUP */
        return;
    case 5:                         /* DPSYS_DESTROYPLAYERORGROUP */
        if (g_BrDPlay.fLog == 0) {
            BrSub10071480(pMsg->f08);
            BrSub10005FE0(pMsg->f08);
        }
        return;
    case 0x21:                      /* DPSYS_DELETEPLAYERFROMGROUP */
        return;
    case 0x31:                      /* DPSYS_SESSIONLOST */
        return;
    case 0x101:                     /* DPSYS_HOST */
        return;
    case 0x102:                     /* DPSYS_SETPLAYERORGROUPDATA */
        return;
    case 0x103:                     /* DPSYS_SETPLAYERORGROUPNAME */
        return;
    case 0x107:                     /* DPSYS_CHAT */
        BrSub100360F0(pv1, pMsg->f0C, pMsg->f10, pMsg->f08, idTo);
        return;
    }
}

/* -- 0x1000C170 ---------------------------------------------------------- */

/* The two format strings at 0x100A6434 / 0x100A6478, and the debug line at
 * 0x100A644C. */
static const char g_szJoined[]  = "%s joined the game.\r\n";
static const char g_szLeft[]    = "%s left the game.\r\n";
static const char g_szDestroy[] = "Destroy Player message received, ID: %d\n";
static const char g_szUnknown[] = "unknown";   /* 0x100A648C */

#ifdef BR_MATCHING_BUILD
/* The three USER32/KERNEL32 imports this function adds; stdcall IAT calls
 * except wsprintfA, which is the one cdecl varargs export in USER32. */
__declspec(dllimport) int  __stdcall lstrlenA(const char *psz);
__declspec(dllimport) int  __cdecl   wsprintfA(char *pszOut, const char *pszFmt, ...);
__declspec(dllimport) void __stdcall OutputDebugStringA(const char *psz);
__declspec(dllimport) int  __stdcall PostMessageA(void *hWnd, unsigned uMsg,
                                                  unsigned wParam, long lParam);

/* WHAT IT DOES: routes one received session message -- to the housekeeping
 * handler when the context is in session mode, to the app handler otherwise --
 * and then, when the lobby log is running, turns a player join or leave into
 * a "<name> joined/left the game" line (GlobalAlloc'd, posted to the log
 * window as message 0x501, or freed when there is no window).  A leave also
 * clears the leaver's slot in the player table and prints a debug line; type
 * 0x104 kicks the shared handler 0x10036510 instead.
 *
 * RESIDUE (insn-exact 147/147, regnorm 0+0, 470 vs 469 B): pMsg sits in ebp
 * where the original has ebx (and the case-5 length temp takes the other),
 * so `mov eax,[ebp]` carries a disp8 byte the original's `mov eax,[ebx]`
 * does not -- whole-body ebx<->ebp transposition, the BrSelLookup class.
 * DEAD: the slot scan MUST be indexed `aSlots[i][0]` (an explicit cursor
 * pointer gets its first iteration peeled into a direct global load and the
 * loop rotated, +10 B); the scan compare must put the table element on the
 * left uncast (a uint32_t cast on it forces load+reg-compare where the
 * original has cmp [mem],reg); the do/while bound compare needs the (int)
 * casts for jl, but the for-i form supersedes it.
 * (thin pre-ledger pass, 4 probes, not counted: the DEAD list above) */
/* @t4-pass 0x100096A0 1 2026-09-09 probes 10 bytes 470 insns 147 regions 8 rows 0 census no  (hand, fn.py variants: decl orders, name/buffer renames, literal spellings, all inert) */
/* @t4-pass 0x100096A0 2 2026-09-09 probes 10 bytes 470 insns 147 regions 8 rows 0 census yes  (hand, fn.py variants: cast/amp/comparison forms across the switch, all inert; corpus hit at +0x30 confirms the dispatch guard shape) */
/* @t3 0x100096A0 2026-09-09 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 470/469 insns 147/147 rows 0+0 regions 8 oracle UNCLASSIFIED
 * @t3-effort passes 2 zero-movement 1 2
 * residue is the whole-body ebx/ebp transposition of pMsg and the case-5
 * length temp (+1 B of disp8, the BrSelLookup class); insn-exact,
 * identical register-blind multiset.  Dead list in the RESIDUE block
 * above plus the two ledger lines.
 * Do not reopen before the end-grind (CLAUDE.md rule 12). */
/* @implements 0x100096A0 glide BrDPlaySysMsgLog */
void BrDPlaySysMsgLog(BrDPlayCtx *pCtx, const BrDPlaySysMsg *pMsg,
                      uint32_t cbData, uint32_t idFrom, uint32_t idTo)
{
    char        *pszB;
    const char  *pszName;
    int          cch1;
    int          cch2;
    int          i;
    char         szDbg[0x104];

    if (pCtx->f0C != 0)
        BrDPlaySysMsgDispatch(pCtx, pMsg, cbData, idFrom, idTo);
    else
        BrAppMsgDispatch(pCtx, (const BrAppMsg *)pMsg,
                         (void *)cbData, (void *)idFrom, (void *)idTo);

    if (g_BrDPlay.fLog != 0) {
        pszB = NULL;
        switch (pMsg->dwType) {
        case 3:
            pszName = (const char *)pMsg->pszName20;
            if (pszName == NULL)
                pszName = g_szUnknown;
            cch1 = lstrlenA(g_szJoined);
            cch2 = lstrlenA(pszName);
            pszB = (char *)GlobalLock(GlobalAlloc(0x42u, cch1 + 1 + cch2));
            if (pszB == NULL)
                return;
            wsprintfA(pszB, g_szJoined, pszName);
            break;
        case 5:
            pszName = (const char *)pMsg->pszName24;
            if (pszName == NULL)
                pszName = g_szUnknown;
            cch1 = lstrlenA(g_szLeft);
            cch2 = lstrlenA(pszName);
            pszB = (char *)GlobalLock(GlobalAlloc(0x42u, cch1 + 1 + cch2));
            if (pszB == NULL)
                return;
            wsprintfA(pszB, g_szLeft, pszName);
            for (i = 0; i < BR_DP_SLOTS; i++) {
                if (g_BrDPlay.aSlots[i][0] == (int)pMsg->f08) {
                    g_BrDPlay.aSlots[i][0] = -1;
                    g_BrDPlay.aSlots[i][1] = 0;
                    sprintf(szDbg, g_szDestroy, pMsg->f08);
                    OutputDebugStringA(szDbg);
                    break;
                }
            }
            break;
        case 0x104:
            BrSub1003CE80();
            break;
        }
        if (pszB != NULL) {
            if (g_BrDPlay.pWnd != NULL) {
                PostMessageA(g_BrDPlay.pWnd, 0x501u, 0u, (long)pszB);
                return;
            }
            GlobalUnlock(GlobalHandle(pszB));
            GlobalFree(GlobalHandle(pszB));
        }
    }
}
#else
/* GlobalAlloc(0x42) + GlobalLock + wsprintfA(buf, pszFmt, pszName), sized
 * exactly the way the original sizes it. Returns NULL if the allocation
 * failed, which is the original's only failure path here. */
static char *BrDPlayFormatLine(const char *pszFmt, const char *pszName)
{
    size_t cb   = strlen(pszFmt) + strlen(pszName) + 1;
    char  *pszB = (char *)g_BrDPlay.os.pfnAlloc((uint32_t)cb);

    if (pszB == NULL)
        return NULL;

    /* DEVIATION: wsprintfA has no bound. snprintf is used with the size the
     * original allocated, which is provably >= the formatted length. */
    snprintf(pszB, cb, pszFmt, pszName);
    return pszB;
}

void BrDPlaySysMsgLog(BrDPlayCtx *pCtx, const BrDPlaySysMsg *pMsg,
                      uint32_t cbData, uint32_t idFrom, uint32_t idTo)
{
    BrDPlayState *pSt  = &g_BrDPlay;
    char         *pszB = NULL;
    const char   *pszName;

    if (pCtx->f0C != 0)
        BrDPlaySysMsgDispatch(pCtx, pMsg, cbData, idFrom, idTo);
    else
        BrAppMsgDispatch(pCtx, (const BrAppMsg *)pMsg,
                         (void *)(uintptr_t)cbData,
                         (void *)(uintptr_t)idFrom,
                         (void *)(uintptr_t)idTo);

    if (pSt->fLog == 0)
        return;

    if (pMsg->dwType == 3u) {
        pszName = pMsg->pszName20 ? pMsg->pszName20 : g_szUnknown;
        pszB    = BrDPlayFormatLine(g_szJoined, pszName);
        if (pszB == NULL)
            return;
    } else if (pMsg->dwType == 5u) {
        int i;

        pszName = pMsg->pszName24 ? pMsg->pszName24 : g_szUnknown;
        pszB    = BrDPlayFormatLine(g_szLeft, pszName);
        if (pszB == NULL)
            return;

        for (i = 0; i < BR_DP_SLOTS; ++i) {
            if ((uint32_t)pSt->aSlots[i][0] == pMsg->f08) {
                char szDbg[0x104];

                pSt->aSlots[i][0] = -1;
                pSt->aSlots[i][1] = 0;
                /* DEVIATION: the original's sprintf into a 0x104-byte stack
                 * buffer is unbounded; snprintf with that same size here. */
                snprintf(szDbg, sizeof(szDbg), g_szDestroy, (int)pMsg->f08);
                pSt->os.pfnDebugOut(szDbg);
                break;
            }
        }
    } else if (pMsg->dwType == 0x104u) {
        BrSub1003CE80();
    }

    if (pszB == NULL)
        return;

    if (pSt->pWnd != NULL)
        pSt->os.pfnPost(pSt->pWnd, BR_DP_WM_LOGLINE, 0, pszB);
    else
        pSt->os.pfnFree(pszB);
}
#endif /* BR_MATCHING_BUILD */

/* -- 0x1000C350 ---------------------------------------------------------- */

#ifdef BR_MATCHING_BUILD
/* WHAT IT DOES: empties the network mailbox. It keeps asking for the next
 * waiting message until there are none left, growing its receive buffer
 * whenever a message turns out to be bigger than the buffer it has, and sends
 * each one to the right place: messages from the session itself go to the
 * housekeeping handler, messages from another player go to the game. */
/* @implements 0x10009880 glide BrDPlayPump */
int32_t BrDPlayPump(BrDPlayCtx *pCtx)
{
    void    *pvBuf = NULL;
    uint32_t cbBuf = 0;    /* zeroed ONCE -- see the GOTCHA in the header */
    uint32_t idFrom;
    uint32_t idTo;
    int32_t  hr;

    for (;;) {
        BrDPlay4Obj     *pDP   = pCtx->pDP;
        BrDPlay4VtblStd *pVtbl = (BrDPlay4VtblStd *)pDP->pVtbl;

        idFrom = 0;
        idTo   = 0;
        hr = pVtbl->Receive(pDP, &idFrom, &idTo, 1u, pvBuf, &cbBuf);

        if (hr == BR_DP_E_BUFFERTOOSMALL) {
            if (pvBuf != NULL) {
                GlobalUnlock(GlobalHandle(pvBuf));
                GlobalFree(GlobalHandle(pvBuf));
            }
            pvBuf = GlobalLock(GlobalAlloc(0x42u, cbBuf));
            if (pvBuf == NULL)
                hr = BR_DP_E_OUTOFMEMORY;
            if (hr == BR_DP_E_BUFFERTOOSMALL)
                continue;
        }

        if (hr >= 0) {
            if (cbBuf >= 4u) {
                if (idFrom == 0u)
                    BrDPlaySysMsgLog(pCtx, (const BrDPlaySysMsg *)pvBuf,
                                     cbBuf, 0u, idTo);
                else
                    BrSub1000BAF0(pCtx, pvBuf, cbBuf, idFrom, idTo);
            }
        }
        if (hr < 0)
            break;
    }

    if (pvBuf != NULL) {
        GlobalUnlock(GlobalHandle(pvBuf));
        GlobalFree(GlobalHandle(pvBuf));
    }

    return 0;
}
#else
/* WHAT IT DOES: empties the network mailbox. It keeps asking for the next
 * waiting message until there are none left, growing its receive buffer
 * whenever a message turns out to be bigger than the buffer it has, and sends
 * each one to the right place: messages from the session itself go to the
 * housekeeping handler, messages from another player go to the game. */
/* @implements 0x10009880 glide BrDPlayPump */
int32_t BrDPlayPump(BrDPlayCtx *pCtx)
{
    void    *pvBuf  = NULL;
    uint32_t cbBuf  = 0;   /* zeroed ONCE -- see the GOTCHA in the header */
    uint32_t idFrom;
    uint32_t idTo;         /* the reused parameter slot in the original */
    int32_t  hr;

    for (;;) {
        BrDPlay4Obj *pDP = pCtx->pDP;

        idFrom = 0;
        idTo   = 0;
        hr = pDP->pVtbl->Receive(pDP, &idFrom, &idTo, 1u, pvBuf, &cbBuf);

        if (hr == BR_DP_E_BUFFERTOOSMALL) {
            if (pvBuf != NULL)
                g_BrDPlay.os.pfnFree(pvBuf);
            pvBuf = g_BrDPlay.os.pfnAlloc(cbBuf);
            if (pvBuf == NULL)
                hr = BR_DP_E_OUTOFMEMORY;
            if (hr == BR_DP_E_BUFFERTOOSMALL)
                continue;
        }

        if (hr < 0)
            break;

        if (cbBuf >= 4u) {
            if (idFrom == 0u)
                BrDPlaySysMsgLog(pCtx, (const BrDPlaySysMsg *)pvBuf,
                                 cbBuf, 0u, idTo);
            else
                BrSub1000BAF0(pCtx, pvBuf, cbBuf, idFrom, idTo);
        }

        if (hr < 0)
            break;
    }

    if (pvBuf != NULL)
        g_BrDPlay.os.pfnFree(pvBuf);

    return 0;
}
#endif

/* -- 0x1000C440 ---------------------------------------------------------- */

#ifdef BR_MATCHING_BUILD
/* WHAT IT DOES: the background thread that keeps multiplayer traffic flowing.
 * It sleeps until either something arrives from the network or the game asks it
 * to stop; on traffic it empties the mailbox and goes back to sleep, and on the
 * stop request it ends the thread. */
/* @implements 0x10009970 glide BrDPlayThreadProc */
uint32_t __stdcall BrDPlayThreadProc(void *pvCtx)
{
    BrDPlayCtx *pCtx  = (BrDPlayCtx *)pvCtx;
    void       *hQuit = g_BrDPlay.hQuit;
    void       *ah[2];

    ah[0] = pCtx->hRecvEvent;
    ah[1] = hQuit;
    if (WaitForMultipleObjects(2u, ah, 0, 0xffffffffu) == 0u) {
        do {
            BrDPlayPump(pCtx);
        } while (WaitForMultipleObjects(2u, ah, 0, 0xffffffffu) == 0u);
    }
    ExitThread(0u);
    return 0;
}
#else
/* WHAT IT DOES: the background thread that keeps multiplayer traffic flowing.
 * It sleeps until either something arrives from the network or the game asks it
 * to stop; on traffic it empties the mailbox and goes back to sleep, and on the
 * stop request it ends the thread. */
/* @implements 0x10009970 glide BrDPlayThreadProc */
uint32_t BrDPlayThreadProc(void *pvCtx)
{
    BrDPlayCtx *pCtx = (BrDPlayCtx *)pvCtx;
    void       *ah[2];

    ah[0] = pCtx->hRecvEvent;
    ah[1] = g_BrDPlay.hQuit;

    if (g_BrDPlay.os.pfnWaitMultiple(2u, ah) == 0u) {
        do {
            BrDPlayPump(pCtx);
        } while (g_BrDPlay.os.pfnWaitMultiple(2u, ah) == 0u);
    }

    g_BrDPlay.os.pfnExitThread(0u);
    return 0;   /* unreachable in the original */
}
#endif

/* -- 0x1000C510 ---------------------------------------------------------- */

#ifdef BR_MATCHING_BUILD
/* WHAT IT DOES: leaves the multiplayer game and closes the networking down --
 * asks the receiving thread to stop and waits for it, removes this machine's
 * player from the session, and lets go of everything the session was holding.
 * It is also the failure path for start-up, so it copes with any of those
 * pieces never having existed. */
/* @implements 0x10009A40 glide BrDPlayShutdown */
int32_t BrDPlayShutdown(BrDPlayCtx *pCtx)
{
    /* The critical section really does go first -- see the GOTCHA. */
    if (g_BrDPlay.fCritInit != 0) {
        DeleteCriticalSection(g_BrDPlayCrit);
        g_BrDPlay.fCritInit = 0;
    }

    if (g_BrDPlay.hThread != NULL) {
        SetEvent(g_BrDPlay.hQuit);
        WaitForSingleObject(g_BrDPlay.hThread, 0xffffffffu);
        CloseHandle(g_BrDPlay.hThread);
        g_BrDPlay.hThread = NULL;
    }

    if (g_BrDPlay.hQuit != NULL) {
        CloseHandle(g_BrDPlay.hQuit);
        g_BrDPlay.hQuit = NULL;
    }

    if (pCtx != NULL) {
        if (pCtx->pDP != NULL) {
            if (pCtx->idPlayer != 0u) {
                ((BrDPlay4VtblStd *)pCtx->pDP->pVtbl)->DestroyPlayer(
                    pCtx->pDP, pCtx->idPlayer);
                pCtx->idPlayer = 0u;
            }
            ((BrDPlay4VtblStd *)pCtx->pDP->pVtbl)->Close(pCtx->pDP);
            ((BrDPlay4VtblStd *)pCtx->pDP->pVtbl)->Release(pCtx->pDP);
            pCtx->pDP = NULL;
        }
    }
    /* Re-test: a nested `if (pCtx != NULL && ...)` inside the block above
     * is deleted as dead. A sibling if keeps the second cmp/je. */
    if (pCtx != NULL && pCtx->hRecvEvent != NULL) {
        CloseHandle(pCtx->hRecvEvent);
        pCtx->hRecvEvent = NULL;
    }

    return 0;
}
#else
/* WHAT IT DOES: leaves the multiplayer game and closes the networking down --
 * asks the receiving thread to stop and waits for it, removes this machine's
 * player from the session, and lets go of everything the session was holding.
 * It is also the failure path for start-up, so it copes with any of those
 * pieces never having existed. */
/* @implements 0x10009A40 glide BrDPlayShutdown */
int32_t BrDPlayShutdown(BrDPlayCtx *pCtx)
{
    BrDPlayState *pSt = &g_BrDPlay;

    /* The critical section really does go first -- see the GOTCHA. */
    if (pSt->fCritInit != 0) {
        pSt->os.pfnDeleteCrit();
        pSt->fCritInit = 0;
    }

    if (pSt->hThread != NULL) {
        pSt->os.pfnSetEvent(pSt->hQuit);
        pSt->os.pfnWaitSingle(pSt->hThread);
        pSt->os.pfnCloseHandle(pSt->hThread);
        pSt->hThread = NULL;
    }

    if (pSt->hQuit != NULL) {
        pSt->os.pfnCloseHandle(pSt->hQuit);
        pSt->hQuit = NULL;
    }

    if (pCtx != NULL) {
        if (pCtx->pDP != NULL) {
            BrDPlay4Obj *pDP = pCtx->pDP;

            if (pCtx->idPlayer != 0u) {
                pDP->pVtbl->DestroyPlayer(pDP, pCtx->idPlayer);
                pCtx->idPlayer = 0u;
            }
            pDP->pVtbl->Close(pDP);
            pDP->pVtbl->Release(pDP);
            pCtx->pDP = NULL;
        }
        /* The original re-tests pCtx here; kept. */
        if (pCtx != NULL && pCtx->hRecvEvent != NULL) {
            pSt->os.pfnCloseHandle(pCtx->hRecvEvent);
            pCtx->hRecvEvent = NULL;
        }
    }

    return 0;
}
#endif

/* -- 0x1000C5D0 ---------------------------------------------------------- */

#ifdef BR_MATCHING_BUILD
/* WHAT IT DOES: gets multiplayer networking ready -- clears the session out,
 * creates the signals the receiving side waits on, and starts the background
 * thread that will collect incoming traffic. If any step fails it undoes the
 * lot and reports that it ran out of memory. */
/* @t4-pass 0x10009B00 1 2026-09-07 probes 46 bytes 140 insns 56 regions 4 rows 28 census yes  (tools/crank.py) */
/* @t4-pass 0x10009B00 2 2026-09-07 probes 46 bytes 140 insns 56 regions 4 rows 28 census yes  (tools/crank.py) */
/* @implements 0x10009B00 glide BrDPlayStartup */
int32_t BrDPlayStartup(BrDPlayCtx *pCtx)
{
    if (g_BrDPlay.fCritInit == 0) {
        InitializeCriticalSection(g_BrDPlayCrit);
        g_BrDPlay.fCritInit = 1;
    }

    pCtx->pDP        = 0;
    pCtx->hRecvEvent = 0;
    pCtx->idPlayer   = 0;
    pCtx->f0C        = 0;
    pCtx->f10        = 0;
    pCtx->hRecvEvent = CreateEventA(0, 0, 0, 0);
    if (pCtx->hRecvEvent != NULL) {
        g_BrDPlay.hQuit = CreateEventA(0, 0, 0, 0);
        if (g_BrDPlay.hQuit != NULL) {
            g_BrDPlay.hThread = CreateThread(0, 0, BrDPlayThreadProc, pCtx,
                                             0, &g_BrDPlay.idThread);
            if (g_BrDPlay.hThread != NULL)
                return 0;
        }
    }

    BrDPlayShutdown(pCtx);
    return BR_DP_E_OUTOFMEMORY;
}
#else
/* WHAT IT DOES: gets multiplayer networking ready -- clears the session out,
 * creates the signals the receiving side waits on, and starts the background
 * thread that will collect incoming traffic. If any step fails it undoes the
 * lot and reports that it ran out of memory. */
/* @implements 0x10009B00 glide BrDPlayStartup */
int32_t BrDPlayStartup(BrDPlayCtx *pCtx)
{
    BrDPlayState *pSt = &g_BrDPlay;

    if (pSt->fCritInit == 0) {
        pSt->os.pfnInitCrit();
        pSt->fCritInit = 1;
    }

    pCtx->pDP        = NULL;
    pCtx->hRecvEvent = NULL;
    pCtx->idPlayer   = 0u;
    pCtx->f0C        = 0;
    pCtx->f10        = 0;

    pCtx->hRecvEvent = pSt->os.pfnCreateEvent();
    if (pCtx->hRecvEvent != NULL) {
        pSt->hQuit = pSt->os.pfnCreateEvent();
        if (pSt->hQuit != NULL) {
            pSt->hThread = pSt->os.pfnCreateThread(BrDPlayThreadProc, pCtx,
                                                   &pSt->idThread);
            if (pSt->hThread != NULL)
                return 0;
        }
    }

    BrDPlayShutdown(pCtx);
    return BR_DP_E_OUTOFMEMORY;
}
#endif

/* -- 0x1000C670 ---------------------------------------------------------- */

/* WHAT IT DOES: asks how many players are in the multiplayer session at this
 * moment, so the lobby can show it. If the question cannot be answered it
 * returns 0xFFFF rather than a count. */
#ifdef BR_MATCHING_BUILD
/* __declspec(dllimport) emits `call dword ptr [IAT]` (and, for GlobalHandle,
 * a register-held IAT load because it is used twice). The portable pfnFree
 * is a cdecl function pointer and cannot produce that sequence. */
__declspec(dllimport) void *__stdcall GlobalHandle(void *pMem);
__declspec(dllimport) int   __stdcall GlobalUnlock(void *hMem);
__declspec(dllimport) void *__stdcall GlobalFree(void *hMem);

/* WHAT IT DOES: ask how many players are in the multiplayer session
 * right now, so the lobby can show it.  0xFFFF means "could not tell". */
/* @implements 0x1000C670 d3d BrDPlayGetCurrentPlayers */
uint32_t BrDPlayGetCurrentPlayers(void)
{
    void    *pv = NULL;
    uint32_t n;

    if (BrSub1003D0B0(g_BrDPlay.pDPGlobal, &pv) < 0)
        return 0xFFFFu;

    n = *(uint32_t *)((char *)pv + 0x2C);
    GlobalUnlock(GlobalHandle(pv));
    GlobalFree(GlobalHandle(pv));
    return n;
}
#else
/* WHAT IT DOES: the same player-count query, reading the count
 * byte-wise so the port is not tied to a struct overlay. */
/* port-only variant of BrDPlayGetCurrentPlayers (matching build uses the #ifdef branch above) */
uint32_t BrDPlayGetCurrentPlayers(void)
{
    void    *pv = NULL;
    uint32_t n;

    if (BrSub1003D0B0(g_BrDPlay.pDPGlobal, &pv) < 0)
        return 0xFFFFu;

    /* DEVIATION: the original reads *(uint32_t *)((char *)pv + 0x2C) by
     * struct overlay. Read byte-wise so the port is endian-agnostic, which
     * this project requires. */
    {
        const unsigned char *pb = (const unsigned char *)pv + 0x2C;

        n = (uint32_t)pb[0]
          | ((uint32_t)pb[1] << 8)
          | ((uint32_t)pb[2] << 16)
          | ((uint32_t)pb[3] << 24);
    }

    g_BrDPlay.os.pfnFree(pv);
    return n;
}
#endif

/* ==========================================================================
 * 0x10036E50 -- create the DirectPlay object and query the wanted iface.
 * Filed from ghidra_batch.c 2026-09-09 (byte-exact; early-out guard shape).
 * ========================================================================== */

typedef struct BrIUnk BrIUnk;
struct BrIUnk {
    struct {
        int (__stdcall *QueryInterface)(BrIUnk *, void *, void **);
        int (__stdcall *AddRef)(BrIUnk *);
        int (__stdcall *Release)(BrIUnk *);
    } *vt;
};
int __stdcall FUN_10072960(int, BrIUnk **, int, int, int);
void FUN_10036f40(int, BrIUnk *);
extern int DAT_100788e8;
extern int DAT_105bc72c;

/* WHAT IT DOES: creates a DirectPlay object, queries the wanted interface
 * and hands it back, releasing the original on success or both on failure. */
/* @t4-pass 0x10036E50 1 2026-09-07 probes 73 bytes 158 insns 58 regions 1 rows 2 census yes  (tools/crank.py) */
/* @t4-pass 0x10036E50 2 2026-09-07 probes 73 bytes 158 insns 58 regions 1 rows 2 census yes  (tools/crank.py) */
/* @implements 0x10036E50 glide BrDpCreateIface */
int BrDpCreateIface(BrIUnk **out)
{
    BrIUnk *a;
    BrIUnk *b;
    int hr;

    a = 0;
    b = 0;
    hr = FUN_10072960(0, &a, 0, 0, 0);
    /* Early-out, not an enclosing `if (hr >= 0)` block: that shape flips
     * the first branch to jl where the original has jge (cracked 2026-09-09,
     * the only residue row). */
    if (hr < 0)
        goto fail;
    hr = a->vt->QueryInterface(a, &DAT_100788e8, (void **)&b);
    if (hr < 0) {
        goto fail;
    }
    a->vt->Release(a);
    a = 0;
    FUN_10036f40(DAT_105bc72c, b);
    *out = b;
    return 0;
fail:
    if (a != 0) {
        a->vt->Release(a);
    }
    if (b != 0) {
        b->vt->Release(b);
    }
    return hr;
}


/* ------------------------------------------------------------------ */
/* 0x10036510 -- pull the session's settings into the globals          */
/* ------------------------------------------------------------------ */

extern void *g_brP277B40;            /* the DirectPlay object, 0x10273328 */
extern int   DAT_100abde8;           /* race options, from desc+0x40      */
extern int   DAT_100b3014;
extern int   DAT_10ac5d58;           /* from desc+0x44                    */
extern int   DAT_10226e80;
extern int   DAT_10ac5d70;           /* from desc+0x48                    */
extern int   DAT_100bcbe8;           /* from desc+0x4c                    */
extern int   DAT_100abdf8;
extern int   DAT_100abdf4;           /* the current option slot           */
extern char  DAT_10ac40a8[];         /* the session name                  */
int  FUN_10036740(void *pIface, void **ppDesc);
void BrSub10044540(void);            /* 0x1003DA90 */
int  BrOptAvailB(int n);             /* 0x10038860 */

/* WHAT IT DOES: after joining a game, copies the host's choices out of the
 * received session description into this machine's own settings -- the race
 * options, the four setting words, and the session's name. Then, if the
 * currently selected option slot is not available in this session, walks
 * forward (wrapping at 32) until one is. The description buffer DirectPlay
 * lent us is unlocked and freed on every path out. */
/* @implements 0x10036510 glide BrNetSessionApply */
int BrNetSessionApply(void)
{
    void *pDesc;
    int   desc[20];
    int   r;
    int   start;
    char *pszName;

    pDesc = 0;
    if (g_brP277B40 == 0) {
        return (int)0x88770082;
    }
    memset(desc, 0, 0x50);
    r = FUN_10036740(g_brP277B40, &pDesc);
    if (r < 0) {
        if (pDesc != 0) {
            GlobalUnlock(GlobalHandle(pDesc));
            GlobalFree(GlobalHandle(pDesc));
        }
        return r;
    }
    DAT_100abde8 = *(int *)((char *)pDesc + 0x40);
    DAT_100b3014 = DAT_100abde8;
    DAT_10ac5d58 = *(int *)((char *)pDesc + 0x44);
    DAT_10226e80 = DAT_10ac5d58;
    DAT_10ac5d70 = *(int *)((char *)pDesc + 0x48);
    DAT_100bcbe8 = *(int *)((char *)pDesc + 0x4c);
    DAT_100abdf8 = DAT_100bcbe8;
    BrSub10044540();
    start = DAT_100abdf4;
    if (BrOptAvailB(DAT_100abdf4) == 0) {
        for (;;) {
            DAT_100abdf4 = DAT_100abdf4 + 1;
            if (DAT_100abdf4 > 0x1f) {
                DAT_100abdf4 = 0;
            }
            if (DAT_100abdf4 == start) {
                break;
            }
            if (BrOptAvailB(DAT_100abdf4) != 0) {
                break;
            }
        }
    }
    pszName = *(char **)((char *)pDesc + 0x30);
    if (pszName != 0) {
        strcpy(DAT_10ac40a8, pszName);
    }
    GlobalUnlock(GlobalHandle(pDesc));
    GlobalFree(GlobalHandle(pDesc));
    return 0;
}
