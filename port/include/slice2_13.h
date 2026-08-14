/* slice2_13.h -- decompiled from BRD3D.dll, pass-13 packet
 * (0x10008B90 - 0x100109A0, 23 functions).
 *
 * Four unrelated clusters ended up in this packet:
 *
 *   1. Two file helpers plus a path helper.
 *        0x10008B90  BrPathBaseName
 *        0x10008BE0  BrFileOpenWrite
 *        0x10008C90  BrFileWriteChecked
 *
 *   2. One global-state index walker.
 *        0x1000BA70  BrCursorAdvance
 *
 *   3. The DirectPlay receive thread and its system-message handling.
 *        0x1000C000  BrDPlaySysMsgDispatch
 *        0x1000C170  BrDPlaySysMsgLog
 *        0x1000C350  BrDPlayPump
 *        0x1000C440  BrDPlayThreadProc
 *        0x1000C510  BrDPlayShutdown
 *        0x1000C5D0  BrDPlayStartup
 *        0x1000C670  BrDPlayGetCurrentPlayers
 *
 *   4. The SECOND vertex-clipping pool (0x102E54C0) and its driver. This is
 *      NOT the pool slice1_03 describes (that one is at 0x104C01A8 with a
 *      different node layout); it is the pool a later pass exposes as
 *      g_pBrLerpFree, and the node type is another module's BrLerpNode.
 *        0x1000F460  BrPolyPoolInit
 *        0x1000F5C0  BrGfxSetBankPointers
 *        0x1000F620  BrGfxClearCounters
 *        0x100106A0  BrPolyClipTri
 *        0x10010970  BrPolyDistMaxX
 *        0x10010990  BrPolyDistMaxY
 *        0x100109A0  BrPolyClipPlane
 *
 * Five functions in the packet are NOT here; see the tail of slice2_13.c for
 * the address and the reason for each.
 *
 * Everything the original reached through a fixed address is gathered into a
 * state struct reachable through a Get...() accessor, so the ported functions
 * keep the original's argument lists exactly.
 */
#ifndef SLICE2_13_H
#define SLICE2_13_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "slice2_14.h"   /* BrLerpNode, BrScrPt, BrDepthRef, g_pBrLerpFree */

/* =====================================================================
 * 1. File / path helpers
 * ===================================================================== */

/* 0x10008B90  __stdcall(pszPath, pszDst) -- copy the last '\\'-separated
 * component of pszPath, NUL included, to pszDst.
 *
 * NOTE THE ARGUMENT ORDER: SOURCE FIRST. Every vector routine in this project
 * is destination-first; this one is not. Preserved, not harmonised.
 *
 * Only '\\' (0x5C) separates -- '/' is not a separator. A path with no
 * backslash copies whole.
 *
 * GOTCHA: the scan starts at the LAST character and only ever inspects the
 * byte BEFORE the cursor, so a TRAILING backslash is not treated as a
 * separator at all: "c:\\games\\" yields "games\\", not "" and not "games".
 *
 * pszDst must have room for strlen(basename)+1 bytes; the original has no
 * length argument and neither does this. */
void BrPathBaseName(const char *pszPath, char *pszDst);

/* 0x10008BE0  __stdcall(pszPath) -- fopen(pszPath, "wb").
 * The mode really is "wb" (the string at 0x100946A8): this opens for WRITING.
 * On failure it reports "Error opening %s: %s" with strerror(errno) and
 * returns NULL anyway -- the report is not fatal. */
FILE *BrFileOpenWrite(const char *pszPath);

/* 0x10008C90  __stdcall(pFile, pvData, cbData) -- fwrite(pvData, 1, cbData,
 * pFile), reporting on a short count.
 *
 * GOTCHA: the callee at 0x1007D150 is fwrite, not fread (0x100709EA writes
 * the literal "RSea" through it into a file opened "wb"), yet the message
 * this function reports is "File read failure". The message is wrong in the
 * original; it is reproduced verbatim. */
void BrFileWriteChecked(FILE *pFile, const void *pvData, uint32_t cbData);

/* =====================================================================
 * 2. 0x1000BA70 -- index walker
 * ===================================================================== */

/* Every global 0x1000BA70 touches. Nothing in this packet establishes what
 * the cursor indexes, so the fields are positional. */
typedef struct BrCursorState {
    int32_t         step;     /* 0x10277B20 -- also the "armed" flag */
    int32_t         pos;      /* 0x1039B6C0 -- the cursor, read AND written */
    int32_t         limit;    /* 0x106C7CAC */
    int32_t         cStops;   /* 0x103643B4 */
    const uint16_t *aStops;   /* 0x10362F28 -- cStops 16-bit values */
} BrCursorState;

BrCursorState *BrCursorGetState(void);

/* 0x1000BA70  no arguments; everything is global.
 *
 * Does nothing at all when step == 0. Otherwise repeatedly does
 *
 *     pos += step;
 *     if (pos >= limit) pos = 0;        <- checked FIRST
 *     if (pos <  0)     pos = limit-1;
 *
 * and stops as soon as pos == 0 or pos equals one of the cStops values in
 * aStops (compared zero-extended from 16 bits against the signed cursor).
 * On the way out it clears step to 0.
 *
 * GOTCHAS:
 *  - the wrap to 0 uses >=, the wrap to limit-1 uses <; they are asymmetric,
 *    and pos == 0 is BOTH a legal cursor value and the loop's stop sentinel.
 *  - when cStops <= 0 the stop table is skipped entirely and the loop can
 *    only end by landing exactly on 0.
 *  - step is never re-read inside the loop, so a step of 0 cannot occur
 *    mid-loop; with limit <= 0 and step > 0 the very first wrap ends it.
 *  - NON-TERMINATION, in the original as well as here: a POSITIVE step always
 *    ends, because the wrap sets pos to exactly 0. A NEGATIVE step wraps to
 *    limit-1 instead, so it walks the cycle limit-1, limit-1-|step|, ... and
 *    back to limit-1 forever unless that cycle contains 0 or a stop value.
 *    limit 64 with step -5 and no matching stop hangs. */
void BrCursorAdvance(void);

/* =====================================================================
 * 3. DirectPlay
 * ===================================================================== */

/* The interface is IDirectPlay4A: 0x1000C350 tests its Receive result against
 * DPERR_BUFFERTOOSMALL (0x8877001E) and calls vtable byte offset 0x64, which
 * is exactly IDirectPlay4::Receive. 0x1000C510 uses 0x24 (DestroyPlayer),
 * 0x10 (Close) and 0x08 (Release), all of which line up with the same table.
 *
 * NAMED BrDPlay4* AND NOT BrDPlayVtbl ON PURPOSE: slice1_06.h already owns
 * `BrDPlayVtbl`, and its layout stops at slot 21. The two cannot be merged
 * without editing that header, which this packet may not do. */
struct BrDPlay4Obj;

typedef struct BrDPlay4Vtbl {
    void *aSlots00[2];                                        /* +0x00,+0x04 */
    int32_t (*Release)(struct BrDPlay4Obj *pThis);            /* +0x08 */
    void *aSlots03[1];                                        /* +0x0C */
    int32_t (*Close)(struct BrDPlay4Obj *pThis);              /* +0x10 */
    void *aSlots05[4];                                        /* +0x14..+0x20 */
    int32_t (*DestroyPlayer)(struct BrDPlay4Obj *pThis,
                             uint32_t idPlayer);              /* +0x24 */
    void *aSlots10[15];                                       /* +0x28..+0x60 */
    int32_t (*Receive)(struct BrDPlay4Obj *pThis,
                       uint32_t *pidFrom, uint32_t *pidTo,
                       uint32_t dwFlags, void *pvData,
                       uint32_t *pcbData);                    /* +0x64 */
} BrDPlay4Vtbl;

typedef struct BrDPlay4Obj {
    const BrDPlay4Vtbl *pVtbl;   /* +0x00 */
} BrDPlay4Obj;

#define BR_DP_E_BUFFERTOOSMALL ((int32_t)0x8877001E)   /* DPERR_BUFFERTOOSMALL */
#define BR_DP_E_OUTOFMEMORY    ((int32_t)0x8007000E)   /* E_OUTOFMEMORY */

/* The 20-byte record 0x1000C5D0 builds and every other routine here carries.
 * Only +0x00, +0x04 and +0x08 are ever read; +0x0C selects the dispatcher in
 * 0x1000C170 and +0x10 is only ever zeroed. */
typedef struct BrDPlayCtx {
    BrDPlay4Obj *pDP;         /* +0x00 */
    void        *hRecvEvent;  /* +0x04 */
    uint32_t     idPlayer;    /* +0x08 */
    int32_t      f0C;         /* +0x0C -- non-zero picks 0x1000C000 */
    int32_t      f10;         /* +0x10 -- written once, never read */
} BrDPlayCtx;

/* A DirectPlay system message, as far as this packet reads one. Offsets 0x20
 * and 0x24 are the DPNAME.lpszShortNameA of DPMSG_CREATEPLAYERORGROUP and
 * DPMSG_DESTROYPLAYERORGROUP respectively, which is what fixes dwType 3 and
 * 5 as "player created" and "player destroyed". */
typedef struct BrDPlaySysMsg {
    uint32_t dwType;      /* +0x00 */
    uint32_t f04;
    uint32_t f08;         /* +0x08 -- the DPID for dwType 5 */
    uint32_t f0C;
    uint32_t f10;
    uint32_t f14;
    uint32_t f18;
    uint32_t f1C;
    char    *pszName20;   /* +0x20 -- read only when dwType == 3 */
    char    *pszName24;   /* +0x24 -- read only when dwType == 5 */
} BrDPlaySysMsg;

/* The OS primitives the originals reach through the import table. Every one
 * is called with a fixed argument pattern, so the hooks take only what
 * actually varies. */
typedef struct BrDPlayOs {
    void    *(*pfnCreateEvent)(void);         /* CreateEventA(0,0,0,0)        */
    void     (*pfnSetEvent)(void *h);
    void     (*pfnCloseHandle)(void *h);
    /* WaitForMultipleObjects(c, ah, FALSE, INFINITE) */
    uint32_t (*pfnWaitMultiple)(uint32_t c, void *const *ah);
    void     (*pfnWaitSingle)(void *h);       /* ..SingleObject(h, INFINITE)  */
    void    *(*pfnCreateThread)(uint32_t (*pfnStart)(void *), void *pvArg,
                                uint32_t *pidOut);
    void     (*pfnExitThread)(uint32_t code);
    void     (*pfnInitCrit)(void);            /* on 0x10277B28 */
    void     (*pfnDeleteCrit)(void);
    /* GlobalAlloc(GMEM_MOVEABLE|GMEM_ZEROINIT, cb) + GlobalLock */
    void    *(*pfnAlloc)(uint32_t cb);
    /* GlobalUnlock(GlobalHandle(p)) + GlobalFree(GlobalHandle(p)) */
    void     (*pfnFree)(void *pv);
    void     (*pfnDebugOut)(const char *psz); /* OutputDebugStringA */
    /* PostMessageA(pWnd, msg, wParam, lParam) */
    void     (*pfnPost)(void *pWnd, uint32_t msg, uintptr_t wParam,
                        void *lParam);
} BrDPlayOs;

#define BR_DP_WM_LOGLINE  0x501u   /* the message 0x1000C170 posts */
#define BR_DP_SLOTS       8        /* 0x10AA2538 .. 0x10AA2598, stride 0x0C */

typedef struct BrDPlayState {
    BrDPlayOs    os;
    int32_t      fCritInit;              /* 0x10277B60 */
    void        *hThread;                /* 0x10277B54 */
    uint32_t     idThread;               /* 0x10277B58 */
    void        *hQuit;                  /* 0x10277B5C */
    BrDPlay4Obj *pDPGlobal;              /* 0x10277B40 */
    void        *pWnd;                   /* 0x10680584 */
    int32_t      fLog;                   /* 0x100AC300 -- 1 in the shipped
                                          * image; see the GOTCHA below */
    int32_t      aSlots[BR_DP_SLOTS][3]; /* 0x10AA2538 */
} BrDPlayState;

BrDPlayState *BrDPlayGetState(void);

/* 0x1000C000  the system-message dispatcher used when pCtx->f0C != 0.
 *
 * The jump table at 0x1000C074 and the 0xD7-byte index table at 0x1000C08C
 * were read out of the retail DLL: five of the six jump-table slots are the
 * function's own `ret`, and only index 4 (dwType 0x107, the last entry of the
 * byte table) reaches real code. So the whole switch has exactly two live
 * arms:
 *
 *   dwType == 0x005 and state->fLog == 0  ->  0x10071480(f08), 0x10005FE0(f08)
 *   dwType == 0x107                       ->  0x100360F0(pv1, f0C, f10, f08,
 *                                                        idTo)
 *
 * GOTCHA: fLog (0x100AC300) is 1 in the shipped image, so the dwType 5 arm is
 * dead unless something writes that global at run time -- and 0x1000C170 uses
 * the SAME global with the opposite sense, logging only when it is non-zero.
 *
 * GOTCHA: dwType 3 and dwType 0x21 are explicitly matched and then fall into
 * the same `ret` as the default. */
void BrDPlaySysMsgDispatch(void *pv1, const BrDPlaySysMsg *pMsg,
                           uint32_t cbData, uint32_t idFrom, uint32_t idTo);

/* 0x1000C170  forward the message, then (when state->fLog != 0) build a
 * human-readable line for it and hand that to the UI.
 *
 * The forward goes to 0x1000C000 when pCtx->f0C != 0 and to 0x1000BEA0
 * (slice1_03's BrAppMsgDispatch) otherwise, with all five arguments passed
 * straight through.
 *
 * Then, only for these three types:
 *   3      "%s joined the game.\r\n" with pMsg->pszName20 (or "unknown")
 *   5      "%s left the game.\r\n"   with pMsg->pszName24 (or "unknown"),
 *          plus: find pMsg->f08 among the 8 slots at 0x10AA2538 and, if it is
 *          there, set that slot's [0] to -1 and [1] to 0 and emit
 *          "Destroy Player message received, ID: %d\n" to the debugger
 *   0x104  call 0x1003CE80 and build no line at all
 *
 * The line, if one was built, is posted to state->pWnd as message 0x501 with
 * the buffer as LPARAM (the receiver owns it); with no window it is freed
 * here instead.
 *
 * GOTCHA: the buffer is sized strlen(fmt)+strlen(name)+1, which is two bytes
 * MORE than the formatted result needs -- the "%s" is counted as well as its
 * expansion. Harmless, but it is not a tight bound and must not be tightened.
 *
 * GOTCHA: the slot scan runs before the line is posted, and a miss simply
 * skips the OutputDebugString -- the line is still posted. */
void BrDPlaySysMsgLog(BrDPlayCtx *pCtx, const BrDPlaySysMsg *pMsg,
                      uint32_t cbData, uint32_t idFrom, uint32_t idTo);

/* 0x1000C350  drain IDirectPlay4A::Receive into a growable buffer and route
 * each message. Always returns 0.
 *
 * Loop shape: Receive(pDP, &idFrom, &idTo, 1 = DPRECEIVEALL, pBuf, &cb).
 *   - DPERR_BUFFERTOOSMALL: free pBuf, allocate cb bytes, retry. If the
 *     allocation fails the code becomes E_OUTOFMEMORY and drops through.
 *   - hr < 0: stop.
 *   - otherwise, if cb >= 4, route:
 *        idFrom == 0  ->  BrDPlaySysMsgLog(pCtx, pBuf, cb, 0, idTo)
 *        idFrom != 0  ->  0x1000BAF0      (pCtx, pBuf, cb, idFrom, idTo)
 *     and keep looping while hr >= 0.
 *
 * GOTCHA: idFrom and idTo are re-zeroed before EVERY Receive, but cb is NOT.
 * cb is zeroed once, before the first call, and thereafter carries whatever
 * the previous successful Receive reported as the message's ACTUAL length --
 * which is then offered back as the buffer's capacity. A large message
 * followed by a small one therefore shrinks the declared capacity and can
 * make the next Receive answer DPERR_BUFFERTOOSMALL and reallocate, even
 * though the buffer is physically big enough.
 *
 * GOTCHA: a message shorter than 4 bytes is silently dropped, but the loop
 * still continues on it. */
int32_t BrDPlayPump(BrDPlayCtx *pCtx);

/* 0x1000C440  __stdcall thread body: wait on { pCtx->hRecvEvent, hQuit } and
 * pump while index 0 is the one that signalled. Anything else -- including
 * hQuit, an abandoned wait or a failure -- exits the thread.
 *
 * The `return 0` after ExitThread is unreachable in the original and is kept
 * only so the C function has a value on every path. */
uint32_t BrDPlayThreadProc(void *pvCtx);

/* 0x1000C510  tear the whole thing down. Always returns 0. Safe on a context
 * that was never started, and on a NULL context.
 *
 * GOTCHA -- REAL BUG: the critical section is deleted FIRST, before hQuit is
 * signalled and before the worker thread is joined. The worker can still be
 * inside 0x1000C4D0 (which takes that very critical section) at that moment.
 * Reproduced as-is. */
int32_t BrDPlayShutdown(BrDPlayCtx *pCtx);

/* 0x1000C5D0  zero the context, create the receive event, the quit event and
 * the worker thread. Returns 0, or E_OUTOFMEMORY after calling
 * BrDPlayShutdown on any failure.
 *
 * GOTCHA: pCtx->pDP is ZEROED here. The caller must install the interface
 * AFTER this returns, and the worker thread is already running by then -- it
 * dereferences pCtx->pDP on its first wake. */
int32_t BrDPlayStartup(BrDPlayCtx *pCtx);

/* 0x1000C670  return the dword at +0x2C of the record 0x1003D0B0 allocates
 * from state->pDPGlobal, then release that record. Returns 0xFFFF when
 * 0x1003D0B0 fails.
 *
 * +0x2C of a DPSESSIONDESC2 is dwCurrentPlayers (4+4+16+16 = 0x28 puts
 * dwMaxPlayers at 0x28), and slice2_22.h independently identifies
 * 0x10277B40 as the IDirectPlay4A. That is the basis for the name; this
 * packet alone does not prove which "get" 0x1003D0B0 performs.
 *
 * GOTCHA: 0xFFFF is a sentinel, not a count, and it is NOT negative -- a
 * caller comparing < 0 will treat a failure as 65535 players. */
uint32_t BrDPlayGetCurrentPlayers(void);

/* =====================================================================
 * 4. The 0x102E54C0 clip pool
 * ===================================================================== */

/* The pool is 64 nodes of 0x28 bytes at 0x102E54C0..0x102E5EC0, with the
 * free-list head at 0x102E5ECC. The node type is another module's BrLerpNode and
 * the head is another module's g_pBrLerpFree; both are reused rather than
 * redeclared. The payload is another module's BrScrPt.
 *
 * The pool BOUNDS are behaviour, not just allocation: both 0x100109A0 and
 * 0x100106A0 return a node to the free list ONLY if its address lies inside
 * the pool. A caller may therefore build a polygon out of its own storage and
 * the clipper will drop those nodes instead of recycling them. */
#define BR_POLY_POOL_NODES 64

/* 1024.0f, the constant at 0x1008F25C. */
#define BR_POLY_CLIP_MAX 1024.0f

/* The head of the polygon: +0x00 is a CIRCULAR singly-linked list, +0x04 the
 * vertex count, which BrPolyClipPlane both reads and updates. */
typedef struct BrPolyList {
    BrLerpNode *pHead;    /* +0x00 */
    int32_t     cVerts;   /* +0x04 */
} BrPolyList;

/* 0x1000F460  thread all 64 nodes onto the free list and point the head at
 * the LOWEST-addressed one. Only the link word is written; pData is left
 * alone (BrLerpNodeAlloc re-points it on every allocation). */
void BrPolyPoolInit(void);

/* NOT in the original: the 64-node array itself, exposed for tests. */
BrLerpNode *BrPolyPoolBase(void);

/* NOT in the original: nodes currently on the free list. */
int BrPolyPoolCount(void);

/* NOT in the original as functions -- both are inlined at every use site.
 * BrPolyPoolFree is the guarded push described above; a node outside the
 * pool is silently DROPPED, not freed. */
BrLerpNode *BrPolyPoolAlloc(void);
void        BrPolyPoolFree(BrLerpNode *pNode);

/* A clip-plane distance function. The original hands it the node's +0x04
 * payload pointer, never the node. Inside is d >= 0.0f. */
typedef float (*BrPolyDistFn)(const BrScrPt *pPt);

/* 0x10010970 and 0x10010990. The two matching lower planes, 0x10010960
 * (return pPt->f0C) and 0x10010980 (return pPt->f10), are in NO packet --
 * see the report; they are declared extern here.
 *
 * Together the four are a screen-space scissor rectangle 0 <= f0C <= 1024 and
 * 0 <= f10 <= 1024, which is what identifies f0C/f10 as the 2D key a later pass
 * describes. */
float BrPolyDistMaxX(const BrScrPt *pPt);   /* 0x10010970  1024 - f0C */
float BrPolyDistMaxY(const BrScrPt *pPt);   /* 0x10010990  1024 - f10 */

/* XSLICE 0x10010960 */
extern float BrPolyDistX(const BrScrPt *pPt);
/* XSLICE 0x10010980 */
extern float BrPolyDistY(const BrScrPt *pPt);

/* 0x100109A0  clip pList against one plane, in place.
 *
 * Sutherland-Hodgman over the circular list, walking at most the ORIGINAL
 * vertex count of edges and stopping early as soon as cVerts drops below 2.
 * Discarded vertices go on a local chain and are returned to the free list
 * only at the very end, after pHead has been rewritten -- so a vertex that is
 * still reachable is never recycled mid-walk.
 *
 * Interpolation always calls BrLerpNodeAlloc(OUTSIDE, INSIDE, dOut/(dOut-dIn)),
 * i.e. the outside vertex is the t=0 end. That is the same convention
 * slice1_03 records for the other pool.
 *
 * GOTCHAS:
 *  - pHead is ROTATED forward by one node on every call, even when cVerts is
 *    0 and the loop body never runs.
 *  - the count is updated on the crossing arms only: entering the volume
 *    increments it, leaving it leaves it alone (one node in, one node out),
 *    and a fully-outside edge decrements it.
 *  - the "inside" test is d >= 0.0f, so a NaN distance counts as OUTSIDE
 *    (the original branches on the x87 C0 flag, which an unordered compare
 *    also sets).
 *  - the iteration budget is captured BEFORE the walk, so vertices added by
 *    this same call are never revisited. */
void BrPolyClipPlane(BrPolyList *pList, BrPolyDistFn pfnDist);

/* 0x100106A0  build a triangle out of three screen points, clip it against
 * all four scissor planes, and for every surviving vertex offer it to
 * BrScrPtKeepNearest (0x10010BF0) at each of the four corners of the
 * 1024x1024 square, in the order (0,0), (1024,0), (0,1024), (1024,1024).
 *
 * The three vertices are wound pV0 -> pV1 -> pV2 and pV0 becomes the list
 * head. NOTE the original's argument order: the node built from the LAST
 * vertex argument is allocated FIRST.
 *
 * Only f00/f04/f08 and the three trailing words are copied out of each source
 * point; f0C/f10 are then produced by BrScrPtProject (0x10010D10).
 *
 * GOTCHA: slice2_14.h calls BrScrPt's +0x14..+0x1C "pad; unused". They are
 * not unused -- this function copies all three of them into the clip node,
 * and BrLerpNodeAlloc interpolates them.
 *
 * GOTCHA: when fewer than two vertices survive, the whole polygon is returned
 * to the free list and NOTHING is emitted -- including the degenerate
 * one-vertex case. */
void BrPolyClipTri(const BrMat4 *pM, BrScrPt *aOut, int *aFlags,
                   const BrScrPt *pV0, const BrScrPt *pV1, const BrScrPt *pV2,
                   const BrDepthRef *pRef);

/* =====================================================================
 * 5. Two pointer-table setters
 * ===================================================================== */

/* 0x1000F5C0  derive six base pointers from one bank index. The three
 * multipliers are exact: 625<<7 == 80000, 125<<8 == 32000, 125<<11 == 256000.
 * The three destination pairs are genuinely aliased in the original -- each
 * value is stored to two different globals. */
typedef struct BrGfxBanks {
    int32_t  iBank;     /* 0x106C65EC */
    void    *pBase0;    /* 0x103643C0 */
    void    *pBase1;    /* 0x1038BCC0 */
    void    *pBase2;    /* 0x102E5F28 */

    void    *p363FF0;   /* 0x10363FF0 } both = pBase0 + iBank*80000  */
    void    *p2E5EC8;   /* 0x102E5EC8 }                              */
    void    *p364304;   /* 0x10364304 } both = pBase1 + iBank*32000  */
    void    *p3643BC;   /* 0x103643BC }                              */
    void    *p2E5EC4;   /* 0x102E5EC4 } both = pBase2 + iBank*256000 */
    void    *p363FF4;   /* 0x10363FF4 }                              */
} BrGfxBanks;

BrGfxBanks *BrGfxGetBanks(void);
void BrGfxSetBankPointers(void);

/* 0x1000F620  zero two independent 32-dword blocks (0x10364308 and
 * 0x10363F68). They are NOT adjacent in the original and the routine is a
 * pair of `rep stosd`, not one. */
#define BR_GFX_COUNTERS 32

typedef struct BrGfxCounters {
    uint32_t a364308[BR_GFX_COUNTERS];
    uint32_t a363F68[BR_GFX_COUNTERS];
} BrGfxCounters;

BrGfxCounters *BrGfxGetCounters(void);
void BrGfxClearCounters(void);

#endif /* SLICE2_13_H */
