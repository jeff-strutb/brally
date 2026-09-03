/* slice2_13.c -- decompiled from BRD3D.dll, pass-13 packet
 * (0x10008B90 - 0x100109A0). See slice2_13.h for the identification notes
 * and every GOTCHA; this file carries the DEVIATION notes.
 */

#ifdef BR_MATCHING_BUILD
/* The original is /MD: CRT calls go through the import table (FF 15). */
#define _CRTIMP __declspec(dllimport)
#endif
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef BR_MATCHING_BUILD
/* Header prototype is cdecl; the original is __stdcall. */
#define BrFileWriteChecked BrFileWriteChecked_cdecl
#define BrDPlayThreadProc  BrDPlayThreadProc_cdecl_hdr
#endif
#include "slice2_13.h"
#ifdef BR_MATCHING_BUILD
#undef BrFileWriteChecked
#undef BrDPlayThreadProc
uint32_t __stdcall BrDPlayThreadProc(void *pvCtx);
#endif
#include "slice1_03.h"   /* BrAppMsg, BrAppMsgDispatch (= 0x1000BEA0) */

/* ==========================================================================
 * Cross-slice declarations
 * ========================================================================== */

/* 0x10008CC0 -- the printf-style error reporter both file helpers call.
 * It is in no packet in this slice.
 * XSLICE 0x10008CC0 */
extern void BrErrorf(const char *pszFmt, ...);

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
 * 1. 0x10008B90 / 0x10008BE0 / 0x10008C90 -- file and path helpers
 * ========================================================================== */

void BrPathBaseName(const char *pszPath, char *pszDst)
{
    size_t      cch = strlen(pszPath);
    const char *p;

    /* DEVIATION: the original computes `p = pszPath + strlen - 1` and only
     * skips the backward scan when that lands exactly on pszPath. For an
     * EMPTY string it lands on pszPath-1, the scan then reads pszPath[-2]
     * and walks down through memory until it happens to find a 0x5C or
     * faults. The empty string is guarded here instead. */
    if (cch == 0) {
        pszDst[0] = '\0';
        return;
    }

    p = pszPath + cch - 1;
    while (p != pszPath && p[-1] != '\\')
        --p;

    memcpy(pszDst, p, strlen(p) + 1);
}

/* @n64 0x80200154 located */
FILE *BrFileOpenWrite(const char *pszPath)
{
    FILE *pFile = fopen(pszPath, "wb");

    if (pFile == NULL)
        BrErrorf("Error opening %s: %s", pszPath, strerror(errno));

    return pFile;
}

#ifdef BR_MATCHING_BUILD
/* Both checked-IO twins are __stdcall (ret 0xC) and both report through the
 * fatal printf at 0x10008EC0 with the READ string -- the fwrite one too. */
extern void BrLogFatalPrintf(const char *pFmt, ...);

/* The checked-open twins fatal-report with strerror; the errno read is
 * the CRT's _errno() CALL through the import table (FF 15), not a
 * variable load. */
_CRTIMP int *__cdecl _errno(void);

/* @implements 0x10008DC0 glide BrFileCreateChecked */
FILE *__stdcall BrFileCreateChecked(char *pszPath)
{
    FILE *pFile;

    pFile = fopen(pszPath, "wb");
    if (pFile == 0) {
        BrLogFatalPrintf("Error opening %s: %s", pszPath,
                         strerror(*_errno()));
    }
    return pFile;
}

/* @implements 0x10008E10 glide BrFileOpenChecked */
FILE *__stdcall BrFileOpenChecked(char *pszPath)
{
    FILE *pFile;

    pFile = fopen(pszPath, "rb");
    if (pFile == 0) {
        BrLogFatalPrintf("Error opening %s: %s", pszPath,
                         strerror(*_errno()));
    }
    return pFile;
}

/* @implements 0x10008E60 glide BrFileReadChecked */
void __stdcall BrFileReadChecked(FILE *pFile, void *pvData, unsigned int cbData)
{
    if (fread(pvData, 1, cbData, pFile) != cbData) {
        BrLogFatalPrintf("File read failure");
    }
}

/* @implements 0x10008E90 glide BrFileWriteChecked */
void __stdcall BrFileWriteChecked(FILE *pFile, const void *pvData,
                                  unsigned int cbData)
{
    if (fwrite(pvData, 1, cbData, pFile) != cbData) {
        BrLogFatalPrintf("File read failure");   /* sic -- an fwrite */
    }
}
#else
void BrFileWriteChecked(FILE *pFile, const void *pvData, uint32_t cbData)
{
    if (fwrite(pvData, 1, cbData, pFile) != (size_t)cbData)
        BrErrorf("File read failure");   /* sic -- this is an fwrite */
}
#endif

/* ==========================================================================
 * 2. 0x1000BA70 -- index walker
 * ========================================================================== */

static BrCursorState g_BrCursor;

BrCursorState *BrCursorGetState(void)
{
    return &g_BrCursor;
}

void BrCursorAdvance(void)
{
    BrCursorState *pSt  = &g_BrCursor;
    int32_t        step = pSt->step;

    if (step == 0)
        return;

    for (;;) {
        int32_t pos = pSt->pos + step;
        int32_t i;

        pSt->pos = pos;
        if (pos >= pSt->limit) {
            pos      = 0;
            pSt->pos = 0;
        }
        if (pos < 0) {
            pos      = pSt->limit - 1;
            pSt->pos = pos;
        }
        if (pos == 0)
            break;

        for (i = 0; i < pSt->cStops; ++i) {
            if (pos == (int32_t)pSt->aStops[i])
                goto stop;
        }
    }

stop:
    pSt->step = 0;
}

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

/* -- 0x1000C350 ---------------------------------------------------------- */

/* WHAT IT DOES: empties the network mailbox. It keeps asking for the next
 * waiting message until there are none left, growing its receive buffer
 * whenever a message turns out to be bigger than the buffer it has, and sends
 * each one to the right place: messages from the session itself go to the
 * housekeeping handler, messages from another player go to the game. */
#ifdef BR_MATCHING_BUILD
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

/* WHAT IT DOES: the background thread that keeps multiplayer traffic flowing.
 * It sleeps until either something arrives from the network or the game asks it
 * to stop; on traffic it empties the mailbox and goes back to sleep, and on the
 * stop request it ends the thread. */
#ifdef BR_MATCHING_BUILD
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

/* WHAT IT DOES: leaves the multiplayer game and closes the networking down --
 * asks the receiving thread to stop and waits for it, removes this machine's
 * player from the session, and lets go of everything the session was holding.
 * It is also the failure path for start-up, so it copes with any of those
 * pieces never having existed. */
#ifdef BR_MATCHING_BUILD
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

/* WHAT IT DOES: gets multiplayer networking ready -- clears the session out,
 * creates the signals the receiving side waits on, and starts the background
 * thread that will collect incoming traffic. If any step fails it undoes the
 * lot and reports that it ran out of memory. */
#ifdef BR_MATCHING_BUILD
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
 * 4. The 0x102E54C0 clip pool
 * ========================================================================== */

static BrLerpNode g_aBrPolyPool[BR_POLY_POOL_NODES];

BrLerpNode *BrPolyPoolBase(void)
{
    return g_aBrPolyPool;
}

void BrPolyPoolInit(void)
{
    int i;

    /* The original walks DOWN from the highest node, so the head ends up on
     * the lowest one and next always points at the node above. */
    g_aBrPolyPool[BR_POLY_POOL_NODES - 1].pNext = NULL;
    for (i = BR_POLY_POOL_NODES - 2; i >= 0; --i)
        g_aBrPolyPool[i].pNext = &g_aBrPolyPool[i + 1];

    g_pBrLerpFree = &g_aBrPolyPool[0];
}

int BrPolyPoolCount(void)
{
    BrLerpNode *p = g_pBrLerpFree;
    int         n = 0;

    while (p != NULL) {
        ++n;
        p = p->pNext;
    }
    return n;
}

BrLerpNode *BrPolyPoolAlloc(void)
{
    BrLerpNode *p = g_pBrLerpFree;

    if (p != NULL)
        g_pBrLerpFree = p->pNext;

    /* DEVIATION: the original returns the null head and every caller then
     * dereferences it. NULL is returned here and every caller guards. */
    return p;
}

/* The pool bounds test, spelled the way the original spells it:
 * 0x102E54C0 <= p < 0x102E5EC0. */
static int BrPolyInPool(const BrLerpNode *p)
{
    return p >= &g_aBrPolyPool[0] && p < &g_aBrPolyPool[BR_POLY_POOL_NODES];
}

void BrPolyPoolFree(BrLerpNode *pNode)
{
    if (!BrPolyInPool(pNode))
        return;   /* silently dropped -- this is the original's behaviour */

    pNode->pNext  = g_pBrLerpFree;
    g_pBrLerpFree = pNode;
}

/* @n64 0x80223470 located */
float BrPolyDistMaxX(const BrScrPt *pPt)
{
    return BR_POLY_CLIP_MAX - pPt->f0C;
}

float BrPolyDistMaxY(const BrScrPt *pPt)
{
    return BR_POLY_CLIP_MAX - pPt->f10;
}

/* -- 0x100109A0 ---------------------------------------------------------- */

/* WHAT IT DOES: trims a shape against one straight edge, so that only the part
 * on the visible side survives. It walks the ring of corners, drops the ones
 * that fall outside, and puts a new corner exactly where the outline crosses
 * the edge. Calling it four times -- once per side -- is how a triangle gets
 * cut down to what actually fits on screen. */
/* @implements 0x1000DF00 glide BrPolyClipPlane */
void BrPolyClipPlane(BrPolyList *pList, BrPolyDistFn pfnDist)
{
    BrLerpNode *pPrev    = pList->pHead;
    BrLerpNode *pCur     = pPrev->pNext;
    BrLerpNode *pOut     = pPrev;
    BrLerpNode *pRecycle = NULL;
    int32_t     n        = pList->cVerts;

    if (n > 0) {
        for (;;) {
            BrLerpNode *pNext = pCur->pNext;
            float dCur  = pfnDist((const BrScrPt *)(const void *)pCur->pData);
            float dPrev = pfnDist((const BrScrPt *)(const void *)pPrev->pData);

            /* Nested on the x87 C0 flags; int temps materialize 0/1. */
            if (dCur >= 0.0f) {
                if (dPrev >= 0.0f) {
                pOut = pCur;
            } else {
                /* entering: splice in the crossing, keep pCur */
                float       t    = dPrev / (dPrev - dCur);
                BrLerpNode *pNew = BrLerpNodeAlloc(pPrev, pCur, t);

#ifndef BR_MATCHING_BUILD
                /* DEVIATION: the original never tests the allocation and
                 * writes through a null node when the pool is empty. */
                if (pNew != NULL) {
#endif
                    pNew->pNext = pOut->pNext;
                    pOut->pNext = pNew;
#ifndef BR_MATCHING_BUILD
                }
#endif
                pList->cVerts = pList->cVerts + 1;
                pOut = pCur;
                }
            } else if (dPrev >= 0.0f) {
                /* leaving: drop pCur, splice in the crossing in its place */
                float       t;
                BrLerpNode *pNew;

                if (pOut->pNext != NULL)
                    pOut->pNext = pOut->pNext->pNext;

                t = dCur / (dCur - dPrev);
                pCur->pNext = pRecycle;
                pRecycle    = pCur;

                pNew = BrLerpNodeAlloc(pCur, pPrev, t);
#ifndef BR_MATCHING_BUILD
                if (pNew != NULL) {
#endif
                    pNew->pNext = pOut->pNext;
                    pOut->pNext = pNew;
                    pOut        = pNew;
#ifndef BR_MATCHING_BUILD
                }
#endif
                /* count unchanged: one out, one in */
            } else {
                /* wholly outside: drop pCur */
                if (pOut->pNext != NULL)
                    pOut->pNext = pOut->pNext->pNext;

                pCur->pNext = pRecycle;
                pRecycle    = pCur;
                pList->cVerts = pList->cVerts - 1;
            }

            pPrev = pCur;
            pCur  = pNext;

            if (pList->cVerts < 2)
                break;
            if (--n <= 0)
                break;
        }
    }

    pList->pHead = pCur;

    {
        /* The chain is walked one node ahead, because the free-list push
         * overwrites the node's link. Inlined: a call to BrPolyPoolFree
         * cannot emit the hoisted free-head load. */
        BrLerpNode *p      = pRecycle;
        BrLerpNode *pNextR = (p != NULL) ? p->pNext : NULL;

        while (p != NULL) {
            BrLerpNode *pHead = g_pBrLerpFree;

            if (p >= &g_aBrPolyPool[0] && p < &g_aBrPolyPool[BR_POLY_POOL_NODES]) {
                p->pNext      = pHead;
                g_pBrLerpFree = p;
            }
            p = pNextR;
            if (p != NULL)
                pNextR = p->pNext;
        }
    }
}

/* -- 0x100106A0 ---------------------------------------------------------- */

/* One of the three identical vertex-setup blocks the original inlines. */
static BrLerpNode *BrPolyMakeVert(const BrScrPt *pSrc)
{
    BrLerpNode *p = BrPolyPoolAlloc();
    BrScrPt    *pDst;

    if (p == NULL)
        return NULL;   /* DEVIATION: see BrPolyPoolAlloc */

    p->pData = &p->data[0];
    pDst     = (BrScrPt *)(void *)p->pData;

    /* f0C / f10 are deliberately NOT copied -- BrScrPtProject produces them. */
    pDst->f00    = pSrc->f00;
    pDst->f04    = pSrc->f04;
    pDst->f08    = pSrc->f08;
    pDst->pad[0] = pSrc->pad[0];
    pDst->pad[1] = pSrc->pad[1];
    pDst->pad[2] = pSrc->pad[2];

    BrScrPtProject(pDst);
    return p;
}

void BrPolyClipTri(const BrMat4 *pM, BrScrPt *aOut, int *aFlags,
                   const BrScrPt *pV0, const BrScrPt *pV1, const BrScrPt *pV2,
                   const BrDepthRef *pRef)
{
    BrPolyList  list;
    BrLerpNode *n0, *n1, *n2;
    int32_t     i;

    list.pHead  = NULL;
    list.cVerts = 0;

    /* Allocation order is pV2, pV1, pV0 -- the LAST vertex argument gets the
     * first node off the free list. */
    n0 = BrPolyMakeVert(pV2);
    if (n0 == NULL)
        return;
    n0->pNext  = list.pHead;   /* NULL at this point */
    list.pHead = n0;

    n1 = BrPolyMakeVert(pV1);
    if (n1 == NULL)
        return;
    n1->pNext  = list.pHead;   /* n0 */
    list.pHead = n1;

    n2 = BrPolyMakeVert(pV0);
    if (n2 == NULL)
        return;
    n2->pNext  = list.pHead;   /* n1 */
    list.pHead = n2;

    n0->pNext   = n2;          /* close the ring: n2 -> n1 -> n0 -> n2 */
    list.cVerts = 3;

    BrPolyClipPlane(&list, BrPolyDistX);
    if (list.cVerts >= 2) BrPolyClipPlane(&list, BrPolyDistMaxX);
    if (list.cVerts >= 2) BrPolyClipPlane(&list, BrPolyDistY);
    if (list.cVerts >= 2) BrPolyClipPlane(&list, BrPolyDistMaxY);

    if (list.cVerts < 2) {
        int32_t k = list.cVerts;
        BrLerpNode *p = list.pHead;

        if (k <= 0)
            return;
        do {
            BrLerpNode *pNext = p->pNext;

            BrPolyPoolFree(p);
            p = pNext;
        } while (--k != 0);
        return;
    }

    for (i = 0; i < list.cVerts; ++i) {
        BrLerpNode    *p    = list.pHead;
        const BrScrPt *pPt  = (const BrScrPt *)(const void *)p->pData;

        list.pHead = p->pNext;

        BrScrPtKeepNearest(pM, aOut, aFlags, 0, pPt, 0.0f, 0.0f, pRef);
        BrScrPtKeepNearest(pM, aOut, aFlags, 1, pPt,
                           BR_POLY_CLIP_MAX, 0.0f, pRef);
        BrScrPtKeepNearest(pM, aOut, aFlags, 2, pPt,
                           0.0f, BR_POLY_CLIP_MAX, pRef);
        BrScrPtKeepNearest(pM, aOut, aFlags, 3, pPt,
                           BR_POLY_CLIP_MAX, BR_POLY_CLIP_MAX, pRef);

        BrPolyPoolFree(p);
    }
}

/* ==========================================================================
 * 5. 0x1000F5C0 / 0x1000F620
 * ========================================================================== */

static BrGfxBanks    g_BrGfxBanks;
static BrGfxCounters g_BrGfxCounters;

BrGfxBanks *BrGfxGetBanks(void)
{
    return &g_BrGfxBanks;
}

void BrGfxSetBankPointers(void)
{
    BrGfxBanks *pB = &g_BrGfxBanks;
    int32_t     i  = pB->iBank;

    pB->p363FF0 = pB->p2E5EC8 = (char *)pB->pBase0 + (ptrdiff_t)i * 80000;
    pB->p364304 = pB->p3643BC = (char *)pB->pBase1 + (ptrdiff_t)i * 32000;
    pB->p2E5EC4 = pB->p363FF4 = (char *)pB->pBase2 + (ptrdiff_t)i * 256000;
}

BrGfxCounters *BrGfxGetCounters(void)
{
    return &g_BrGfxCounters;
}

/* @n64 0x8026C5C0 located */
void BrGfxClearCounters(void)
{
    memset(g_BrGfxCounters.a364308, 0, sizeof(g_BrGfxCounters.a364308));
    memset(g_BrGfxCounters.a363F68, 0, sizeof(g_BrGfxCounters.a363F68));
}

/* ==========================================================================
 * NOT PORTED -- five functions of the packet, and why
 * ==========================================================================
 *
 * 0x1000C6E0  (932 bytes) display-list emitter. Four identical passes that
 *   append ~35 fixed 8-byte RDP/RSP commands to the cursor at 0x106C0680 and
 *   make five outside calls, one of them (0x1002F900) with sixteen stack
 *   arguments whose meaning is not established anywhere in this slice. It is
 *   transcribable but nothing in this packet can validate the transcription,
 *   and a silently mis-copied command word is worse than a missing function.
 *
 * 0x1000CA90  (322 bytes) per-entity visibility gate. Reaches five outside
 *   vector routines (0x1003B0E0, 0x10031D3F, 0x1003A950, 0x1003AFE0) and
 *   seven globals whose meaning is not established; the two indexed writes at
 *   0x10277E60 and 0x10277B68 are keyed off entity+0x140, which this packet
 *   never bounds. Skipped rather than guessed.
 *
 * 0x1000E950  (1576 bytes) the same kind of emitter as 0x1000C6E0, plus two
 *   float accumulators at 0x10575504 / 0x105754F8 fed through 0x1003AC90 and
 *   0x1003B020. Same reasoning.
 *
 * 0x1000EF80  (1246 bytes) walks a 10x10 grid of command lists looking for
 *   G_VTX (0x04) blocks and displaces their vertices. The vertex count is
 *   decoded as bits [15:10] of the command word, which is the F3DEX layout
 *   the contract records -- but the displacement is eight separate x87
 *   sequences with sign-dependent branches on the low nibble of a truncated
 *   angle, and getting one of them backwards would silently corrupt geometry.
 *   Not ported without a way to test it.
 *
 * 0x1000F480  (308 bytes) projects a bounding box corner pair into two
 *   16-bit screen rectangles. Nine values live on the x87 stack at once
 *   across six fxch pairs and four __ftol calls; the contract's own warning
 *   about untraceable x87 operand order applies exactly. Skipped.
 */

/* ── Ghidra-matched functions ─────────────────────────── */
#ifdef BR_MATCHING_BUILD

/* WHAT IT DOES: qsort comparator on the int16 at +2: 1 / 0 / -1, first argument compared
 * on the left (jle / setge). */
/* @implements 0x1000E2F0 glide BrQsortCmpS2 */

int BrQsortCmpS2(int param_1,int param_2)

{
  if (*(short *)(param_1 + 2) > *(short *)(param_2 + 2)) {
    return 1;
  }
  return (*(short *)(param_1 + 2) >= *(short *)(param_2 + 2)) - 1;
}

#endif /* BR_MATCHING_BUILD */
